/*
 * Copyright (C) 2008-2010, 2012, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <idl/capros/DS2438.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include "DS2438.h"
#include "w1mult.h"

// Bits in status/configuration register:
#define scr_IAD 0x01
#define scr_CA  0x02
#define scr_EE  0x04
#define scr_AD  0x08
#define scr_TB  0x10
#define scr_NVB 0x20
#define scr_ADB 0x40

/* VEEExpiry is the time after which no DS2438 will be busy doing
 * a Convert V or Copy Scrachpad command. */
capros_Sleep_nanoseconds_t VEEExpiry;

Link DS2438_TSamplingQueue[maxLog2Seconds+1];
Link DS2438_VSamplingQueue[maxLog2Seconds+1];
Link DS2438_CSamplingQueue[maxLog2Seconds+1];
capros_Sleep_nanoseconds_t DS2438_sampledTime;
capros_RTC_time_t DS2438_sampledRTC;

void
DS2438_Init(void)
{
  int i;
  for (i = 0; i <= maxLog2Seconds; i++) {
    link_Init(&DS2438_TSamplingQueue[i]);
    link_Init(&DS2438_VSamplingQueue[i]);
    link_Init(&DS2438_CSamplingQueue[i]);
  }
}

/* This is called to initialize a configured device.
This happens once at the big bang, or if a new device is configured.
The device hasn't been located on the network yet. */
void
DS2438_InitStruct(struct W1Device * dev)
{
  link_Init(&dev->u.bm.tSamplingQueueLink);
  link_Init(&dev->u.bm.vSamplingQueueLink);
  link_Init(&dev->u.bm.cSamplingQueueLink);
  dev->u.bm.tempHL.logSlot = NOLOG;
  dev->u.bm.voltHL.logSlot = NOLOG;
  dev->u.bm.currentHL.logSlot = NOLOG;
  // Default configuration: accumEE, no Vad.
  dev->u.bm.configReg = scr_IAD | scr_CA | scr_EE | scr_AD;
  dev->u.bm.threshReg = 0;
}

/* Wait until all devices are not busy converting.
 *
 * As far as I know, the device can't respond to other commands
 * while it is busy doing a conversion or copying to EEPROM.
 * Wait for those activities to be done.
 * On entry, currentTime must be current. */
void
WaitUntilNotBusy(void)
{
  // A Convert T takes 10 ms.
  capros_Sleep_nanoseconds_t wakeTime = latestConvertTTime + 10000000;
  // Wait for Convert V and Copy Scratchpad too.
  if (wakeTime < VEEExpiry)
    wakeTime = VEEExpiry;
  if (wakeTime > currentTime) {
    DEBUG(bm) kprintf(KR_OSTREAM, "WUNB tt %llu VEEE %llu wt %llu",
                latestConvertTTime/1000000,
                VEEExpiry/1000000,
                wakeTime/1000000);
    result_t result = capros_Sleep_sleepTillPersistent(KR_SLEEP, wakeTime);
    assert(result == RC_OK || result == RC_capros_key_Restart);
    currentTime = wakeTime;
  }
  // else no need to wait.
}

// Page is left in inBuf.
static int
ReadSPADPage(struct W1Device * dev, unsigned int page)
{
  AddressDevice(dev);
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(2)
  wp(0xbe)	// Read Scratchpad
  wp(page)
  wp(capros_W1Bus_stepCode_readBytes)
  wp(9)		// read 8 bytes of data and CRC8
  int status = RunProgram();
  if (status) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS2438 %#llx ReadSPAD status %#x!\n",
                          dev->rom, status);
    return status;
  }

  DEBUG(bm) kprintf(KR_OSTREAM,
              "DS2438 %#llx pg %d %#.2x %#.2x %#.2x %#.2x %#.2x %#.2x %#.2x %#.2x crc %#.2x\n",
              dev->rom, page, inBuf[0], inBuf[1], inBuf[2], inBuf[3],
              inBuf[4], inBuf[5], inBuf[6], inBuf[7], inBuf[8]);

  int c = CalcCRC8(inBuf, 8);
  if (c != inBuf[8]) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS2438 %#llx ReadSPAD CRC error!\n",
                          dev->rom);
    return capros_W1Bus_StatusCode_CRCError;
  }

  return capros_W1Bus_StatusCode_OK;
}

// The caller must have checked that the device is not busy.
// The caller must address the device first.
static int
RecallAndReadPage(struct W1Device * dev, unsigned int page)
{
  NotReset();
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(2)
  wp(0xb8)	// Recall Memory (copy EEPROM/SRAM to scratchpad)
  wp(page)
  return ReadSPADPage(dev, page);
} 

static int
AddressRecallAndReadPage(struct W1Device * dev, unsigned int page)
{
  int tries = 0;
  while (1) {
    AddressDevice(dev);
    int status = RecallAndReadPage(dev, page);
    if (status) {
      if (status == capros_W1Bus_StatusCode_BusError
          || status == capros_W1Bus_StatusCode_CRCError) {
        if (++tries < 4) {
          continue;	// try again
        }
      }
    }
    return status;
  }
}

static int
MatchRecallAndReadPage(struct W1Device * dev, unsigned int page)
{
  int tries = 0;
  while (1) {
    // The device is on active branches, so we can just address it:
    ProgramReset();
    ProgramMatchROM(dev);
    int status = RecallAndReadPage(dev, 0);
    if (status) {
      if (status == capros_W1Bus_StatusCode_BusError
          || status == capros_W1Bus_StatusCode_CRCError) {
        if (++tries < 4) {
          continue;	// try again
        }
      }
    }
    return status;
  }
}

// The caller must have checked that the device is not busy.
static int
WritePage(struct W1Device * dev, unsigned int page, uint8_t * data)
{
  int status;
  int tries = 0;
  while (tries++ < 2) {
    int i;
    AddressDevice(dev);
    wp(capros_W1Bus_stepCode_writeBytes)
    wp(2+8)
    wp(0x4e)      // Write Scratchpad
    wp(page)
    for (i = 0; i < 8; i++) {
      wp(data[i])
    }
    // Read scratchpad back to verify
    status = ReadSPADPage(dev, page);
    if (status) {
      if (status == capros_W1Bus_StatusCode_CRCError)
        continue;	// try again
      return status;
    }
    if (page) {
      status = memcmp(inBuf, data, 8);
    } else {
      // Page 0 has some read-only bytes; only compare the read/write ones.
      status = data[0] != inBuf[0]
                || data[7] != inBuf[7];
    }
    if (status != 0) {	// read back incorrect data
      continue;	// try again
    }
    // Data read back was correct.
    AddressDevice(dev);
    wp(capros_W1Bus_stepCode_writeBytes)
    wp(2)
    wp(0x48)      // Copy Scratchpad
    wp(page)
    status = RunProgram();
    if (status) {
      return status;
    }
    result_t result;
    result = capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &VEEExpiry);
    assert(result == RC_OK);
    VEEExpiry += 10000000;	// Copy will be done 10 ms from now
    return capros_W1Bus_StatusCode_OK;
  }
  // Got a data error, exhausted retries.
  return capros_W1Bus_StatusCode_CRCError;
}

int
SetConfiguration(struct W1Device * dev)
{
  uint8_t data[8] = {
    [0] = dev->u.bm.configReg,
    [7] = dev->u.bm.threshReg
    // don't care about other bytes, since they are read-only in page 0
  };
  WaitUntilNotBusy();
  return WritePage(dev, 0, data);
}

int
SetConfigurationIfFound(struct W1Device * dev)
{
  if (dev->found)
    return SetConfiguration(dev);
  return 0;
}

static void
EnsureConfiguration(struct W1Device * dev, unsigned int newConfig)
{
  DEBUG(bm) kprintf(KR_OSTREAM, "configC old %#.2x new %#.2x",
              dev->u.bm.configReg, newConfig);
  if (dev->u.bm.configReg != newConfig) {
    dev->u.bm.configReg = newConfig;
    SetConfigurationIfFound(dev);
    /* Ignore any status error; we've captured the config in dev. */
  }
}

/* This is called to initialize a device that has been found on the network.
This is called at least on every reboot.
This procedure can issue device I/O, but must not take a long time.
The device is addressed, since we just completed a searchROM that found it.
Returns true iff dev is OK.
*/
bool
DS2438_InitDev(struct W1Device * dev)
{
  int status;
  int tries = 0;
  // AddressDevice(dev);	not necessary
  while (1) {
    status = RecallAndReadPage(dev, 0);	// read page 0
    if (status) {
      DEBUG(errors) kprintf(KR_OSTREAM,
             "DS2438 read page 0 status=%d, bytes=%d data= %#.2x %#.2x %#.2x\n",
             status, RunPgmMsg.rcv_sent, inBuf[0], inBuf[1], inBuf[2]);
      if (status == capros_W1Bus_StatusCode_BusError
          || status == capros_W1Bus_StatusCode_CRCError) {
        if (++tries < 4) {
          AddressDevice(dev);
          continue;	// try again
        }
      }
      return false;
    }
    break;
  }
  if ((inBuf[0] & 0x7f) != dev->u.bm.configReg
      || inBuf[7] != dev->u.bm.threshReg) {	// need to set configuration
    status = SetConfiguration(dev);
    if (status) {
      DEBUG(errors) kprintf(KR_OSTREAM,
             "DS2438 SetConfiguration status=%d\n", status);
      return false;
    }
  }

  DEBUG(bm) kprintf(KR_OSTREAM, "DS2438 %#llx is found.\n",
                   dev->rom);
  return true;
}

/* heartbeatSeed keeps all the different types of devices
from going off in sync. */
#define heartbeatSeed 3

struct W1Device * DS2438_samplingListHead;

static void
ConvertTemp(struct W1Device * dev)
{
  RecordCurrentTime();
  // WaitUntilNotBusy();
  // The device is on active branches, so we can just address it:
  ProgramReset();
  ProgramMatchROM(dev);
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(1)
  wp(0x44)	// Convert T
  int status = RunProgram();
  if (! status) {
    RecordCurrentRTC();
    RecordCurrentTime();
    dev->u.bm.tSampledTime = currentTime;
    dev->u.bm.tSampledRTC = currentRTC;
    dev->u.bm.tSampled = true;
    latestConvertTTime = currentTime;
  }
  DEBUG(doall) kprintf(KR_OSTREAM, "DS2438 %#llx temp sampled at %llu\n",
                       dev->rom, dev->u.bm.tSampledTime);
}

static void
readResults(struct W1Device * dev)
{
  do {
    int status = MatchRecallAndReadPage(dev, 0);
    if (status)
      return;
  } while (inBuf[0] & (scr_TB | scr_ADB));	// if still busy, try again
  if (dev->u.bm.tSampled) {
    int16_t temperature = (inBuf[1] | (inBuf[2] << 8)) >> 3;
    if (temperature > 0x7d00 || temperature < -0x3700) {
      DEBUG(errors) kprintf(KR_OSTREAM, "DS2438 %#llx temp is %#x!\n",
                            dev->rom, temperature & 0xffff);
    } else {
      temperature &= dev->u.bm.tempResolutionMask;
      HystLog16_log(&dev->u.bm.tempHL, temperature, dev->u.bm.tSampledRTC,
                    dev->u.bm.tSampledTime, 0);
    }
  }
  if (dev->u.bm.vSampled) {
    uint16_t voltage = inBuf[3] | (inBuf[4] << 8);
    if (voltage > 0x3ff) {
      DEBUG(errors) kprintf(KR_OSTREAM, "DS2438 %#llx voltage is %#x!\n",
                            dev->rom, voltage & 0xffff);
    } else {
      voltage &= dev->u.bm.voltResolutionMask;
      HystLog16_log(&dev->u.bm.voltHL, voltage, DS2438_sampledRTC,
                    DS2438_sampledTime, dev->u.bm.voltSelect);
      DEBUG(doall) kprintf(KR_OSTREAM, "DS2438 %#llx volt sampled at %llu\n",
                           dev->rom, DS2438_sampledTime);
    }
  }
  if (dev->u.bm.cSampled) {
    int16_t current = inBuf[5] | (inBuf[6] << 8);
    int sign = current >> 10;
    if (sign != 0 && sign != -1) {
      DEBUG(errors) kprintf(KR_OSTREAM, "DS2438 %#llx current is %#x!\n",
                            dev->rom, current & 0xffff);
    } else {
      current &= dev->u.bm.currentResolutionMask;
      HystLog16_log(&dev->u.bm.currentHL, current, currentRTC, currentTime, 0);
      DEBUG(doall) kprintf(KR_OSTREAM, "DS2438 %#llx current sampled at %llu\n",
                           dev->rom, currentTime);
    }
  }
}

/* We can save addressing each individual device
 * and just issue a Convert to all active devices.
 * It takes at most 10 ms to do a Convert V,
 * while it takes 5.28 ms to address an individual device. */
static void
ConvertV(struct Branch * br)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "ConvertV called.\n");
  /* Convert only on this branch, so we must have a smart-on: */
  EnsureBranchSmartReset(br);
  wp(capros_W1Bus_stepCode_skipROM);
  WriteOneByte(0xb4);	// Convert V
  RunProgram();
}

void
DS2438_HeartbeatAction(uint32_t hbCount)
{
  struct W1Device * dev;

  DEBUG(doall) {
    RecordCurrentTime();
    kprintf(KR_OSTREAM, "DS2438_HeartbeatAction called at %llu ms "
                 "wq0=%#x wq1=%#x\n",
                 currentTime/1000000,
                 DS2438_VSamplingQueue[0].next,
                 DS2438_VSamplingQueue[1].next);
  }

  uint32_t thisCount;
  if (hbCount == 0) {
    // First time after a boot. Sample all devices.
    thisCount = 0;
  } else {
    thisCount = hbCount + heartbeatSeed;
  }

  // First need to clear all the Sampled flags.
  MarkForSampling(0, &DS2438_TSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.tSamplingQueueLink) );
  ReMarkForSampling(0, &DS2438_VSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.vSamplingQueueLink) );
  ReMarkForSampling(0, &DS2438_CSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.cSamplingQueueLink) );
  for (dev = DS2438_samplingListHead; dev; dev = dev->nextInSamplingList) {
    dev->u.bm.tSampled = false;
    dev->u.bm.vSampled = false;
    dev->u.bm.cSampled = false;
  }
  UnmarkSamplingList(DS2438_samplingListHead);

  // Sample temperature.
  MarkForSampling(thisCount, &DS2438_TSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.tSamplingQueueLink) );
  /* We do not broadcast Convert T (that is, send with Skip ROM) for DS2438's
  because that could trigger DS18B20's, which would make them busy
  for up to 750 ms. Instead, we just read each device individually.
  Convert T takes up to 10 ms. */
  DoAllWorkFunction = &DoEach;
  DoEachWorkFunction = &ConvertTemp;
  DoAll(&root);
  UnmarkSamplingList(DS2438_samplingListHead);

  // Sample voltage.
  MarkForSampling(thisCount, &DS2438_VSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.vSamplingQueueLink) );

  if (DS2438_samplingListHead) {	// if there are any this time
    DoAllWorkFunction = &ConvertV;
    DoAll(&root);

    RecordCurrentTime();
    RecordCurrentRTC();
    DS2438_sampledTime = currentTime;
    DS2438_sampledRTC = currentRTC;

    // We could be busy on the Convert V for 10 ms:
    VEEExpiry = currentTime + 10000000;
    DEBUG(bm) kprintf(KR_OSTREAM, "Sampled at %llu ms", currentTime/1000000);

    // Note that every device on the list has been sampled:
    for (dev = DS2438_samplingListHead; dev; dev = dev->nextInSamplingList) {
      dev->u.bm.vSampled = true;
    }

    UnmarkSamplingList(DS2438_samplingListHead);
  }

  /* Wait right now until the DS2438's aren't busy. */
  WaitUntilNotBusy();

  MarkForSampling(thisCount, &DS2438_CSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.cSamplingQueueLink) );
  if (DS2438_samplingListHead) {
    RecordCurrentTime();
    RecordCurrentRTC();
    for (dev = DS2438_samplingListHead; dev; dev = dev->nextInSamplingList) {
      dev->u.bm.cSampled = true;	// note we need to report it
    }
  }

  // Add devices being sampled for T or V:
  ReMarkForSampling(thisCount, &DS2438_TSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.tSamplingQueueLink) );
  ReMarkForSampling(thisCount, &DS2438_VSamplingQueue[0],
                  &DS2438_samplingListHead,
                  offsetof(struct W1Device, u.bm.vSamplingQueueLink) );

  // Read all the results:
  DoAllWorkFunction = &DoEach;
  DoEachWorkFunction = &readResults;
  DoAll(&root);
  UnmarkSamplingList(DS2438_samplingListHead);

  DEBUG(doall) kprintf(KR_OSTREAM, "DS2438_HeartbeatAction done\n");
}

void
DS2438_ProcessRequest(struct W1Device * dev, Message * msg)
{
  result_t result;

  switch (msg->rcv_code) {
  default:
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_key_getType:
    msg->snd_w1 = IKT_capros_DS2438;
    break;

  case OC_capros_DS2438_configureTemperature:
  {
    unsigned int log2Seconds = msg->rcv_w1;
    int resolution = msg->rcv_w2;
    unsigned int hysteresis = msg->rcv_w3;
    if (log2Seconds > 255
        || resolution > 5
        || resolution < -6
        || hysteresis >= 32768 ) {
reqerr:
      msg->snd_code = RC_capros_key_RequestError;
    } else {
      result = EnsureLog(&dev->u.bm.tempHL.logSlot);
      if (result != RC_OK) {
        msg->snd_code = result;
      } else {
        dev->u.bm.tempResolutionMask = (-(1 << 11)) >> (resolution + 6);
        dev->u.bm.tempHL.hysteresis = hysteresis;
        dev->u.bm.tempHL.hysteresisLow = 16384;	// log the next reading

        link_Unlink(&dev->u.bm.tSamplingQueueLink);
        dev->u.bm.tSampled = false;
        if (log2Seconds != 255) {
          if (log2Seconds > maxLog2Seconds)
            log2Seconds = maxLog2Seconds;	// no harm in sampling more often
          link_insertAfter(&DS2438_TSamplingQueue[log2Seconds],
                           &dev->u.bm.tSamplingQueueLink);
        }
        GetLogfile(dev->u.bm.tempHL.logSlot);
        msg->snd_key0 = KR_TEMP0;
      }
    }
    break;
  }

  {
    int select;
  case OC_capros_DS2438_configureVoltageVad:
    select = 0;
    goto configV;

  case OC_capros_DS2438_configureVoltageVdd:
    select = 1;
  configV: ;
    unsigned int log2Seconds = msg->rcv_w1;
    int resolution = msg->rcv_w2;
    unsigned int hysteresis = msg->rcv_w3;
    if (log2Seconds > 255
        || resolution > 0
        || resolution < -9
        || hysteresis >= 32768 )
      goto reqerr;

    result = EnsureLog(&dev->u.bm.voltHL.logSlot);
    if (result != RC_OK) {
      msg->snd_code = result;
    } else {
      dev->u.bm.voltSelect = select;
      dev->u.bm.voltResolutionMask = (-(1 << 9)) >> (resolution + 9);
      dev->u.bm.voltHL.hysteresis = hysteresis;
      dev->u.bm.voltHL.hysteresisLow = 16384;	// log the next reading

      EnsureConfiguration(dev, (dev->u.bm.configReg & ~scr_AD)
                               | (select ? scr_AD : 0));

      link_Unlink(&dev->u.bm.vSamplingQueueLink);
      dev->u.bm.vSampled = false;
      if (log2Seconds != 255) {
        if (log2Seconds > maxLog2Seconds)
          log2Seconds = maxLog2Seconds;	// no harm in sampling more often
        link_insertAfter(&DS2438_VSamplingQueue[log2Seconds],
                         &dev->u.bm.vSamplingQueueLink);
      }

      GetLogfile(dev->u.bm.voltHL.logSlot);
      msg->snd_key0 = KR_TEMP0;
    }
    break;
  }

  case OC_capros_DS2438_configureCurrentOff:
  {
    EnsureConfiguration(dev, dev->u.bm.configReg & 0xf8);
    link_Unlink(&dev->u.bm.cSamplingQueueLink);
    dev->u.bm.cSampled = false;
    break;
  }

  case OC_capros_DS2438_configureCurrent:
  {
    unsigned int log2Seconds = msg->rcv_w1;
    int resolution = msg->rcv_w2;
    unsigned int hysteresis = msg->rcv_w3;
    if (log2Seconds > 255
        || resolution > 0
        || resolution < -10
        || hysteresis >= 32768 )
      goto reqerr;

    result = EnsureLog(&dev->u.bm.currentHL.logSlot);
    if (result != RC_OK) {
      msg->snd_code = result;
    } else {
      dev->u.bm.currentResolutionMask = (-(1 << 10)) >> (resolution + 10);
      dev->u.bm.currentHL.hysteresis = hysteresis;
      dev->u.bm.currentHL.hysteresisLow = 16384;	// log the next reading

      // Set IAD, clear CA and EE
      EnsureConfiguration(dev, (dev->u.bm.configReg & 0xf8) | 0x1);

      link_Unlink(&dev->u.bm.cSamplingQueueLink);
      dev->u.bm.cSampled = false;
      if (log2Seconds > maxLog2Seconds)
        log2Seconds = maxLog2Seconds;	// no harm in sampling more often
      link_insertAfter(&DS2438_CSamplingQueue[log2Seconds],
                       &dev->u.bm.cSamplingQueueLink);

      GetLogfile(dev->u.bm.currentHL.logSlot);
      msg->snd_key0 = KR_TEMP0;
    }
    break;
  }

  case OC_capros_DS2438_readThreshold:
  {
    msg->snd_w1 = dev->u.bm.threshReg;
    break;
  }

  case OC_capros_DS2438_writeThreshold:
  {
    if (dev->u.bm.configReg & scr_IAD) {	// current measuring is on
      dev->u.bm.configReg &= ~scr_IAD;	// disable it temporarily
      int status = SetConfigurationIfFound(dev);
      if (status) {
buserr:
        msg->snd_code = RC_capros_DS2438_BusError;
        break;
      }
      dev->u.bm.threshReg = msg->rcv_w1;
      status = SetConfigurationIfFound(dev);
      if (status) goto buserr;
      dev->u.bm.configReg |= scr_IAD;	// restore it
      status = SetConfigurationIfFound(dev);
      if (status) goto buserr;
    } else {
      dev->u.bm.threshReg = msg->rcv_w1;
    }
    break;
  }

  case OC_capros_DS2438_readPage:
  {
    unsigned int page = msg->rcv_w1;
    if (page == 0 || page >= 8)
      goto reqerr;
    if (! dev->found) {
      msg->snd_code = RC_capros_DS2438_Offline;
    } else {
      WaitUntilNotBusy();
      int status = AddressRecallAndReadPage(dev, page);
      if (status)
        goto buserr;
      msg->snd_len = 8;
      msg->snd_data = &inBuf[0];
    }
    break;
  }

  case OC_capros_DS2438_writePage:
  {
    unsigned int page = msg->rcv_w1;
    if (page == 0 || page >= 8)
      goto reqerr;
    if (msg->rcv_sent != 8)
      goto reqerr;
    if (! dev->found) {
      msg->snd_code = RC_capros_DS2438_Offline;
    } else {
      WaitUntilNotBusy();
      int status = WritePage(dev, page, msg->rcv_data);
      if (status)
        goto buserr;
    }
    break;
  }

  }
}
