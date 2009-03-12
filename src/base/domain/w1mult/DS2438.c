/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

Link DS2438_samplingQueue[maxLog2Seconds+1];

void
DS2438_Init(void)
{
  int i;
  for (i = 0; i <= maxLog2Seconds; i++) {
    link_Init(&DS2438_samplingQueue[i]);
  }
}

/* This is called to initialize a configured device.
This happens once at the big bang, or if a new device is configured.
The device hasn't been located on the network yet. */
void
DS2438_InitStruct(struct W1Device * dev)
{
  dev->u.bm.tTime = 0;  // no data yet
  dev->u.bm.vTime = 0;  // no data yet
  dev->u.bm.voltageIsRead = true;	// there is no converted unread data
  dev->u.bm.tempLog2Sec = 255;	// don't sample temperature
  // Default configuration: accumEE, no Vad.
  dev->u.bm.configReg = scr_IAD | scr_CA | scr_EE | scr_AD;
  dev->u.bm.threshReg = 0;
}

/* As far as I know, the device can't respond to other commands
 * while it is busy doing a conversion or copying to EEPROM.
 * Wait for those activities to be done. */
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

/* This is called to initialize a device that has been found on the network.
This is called at least on every reboot.
This procedure can issue device I/O, but must not take a long time.
The device is addressed, since we just completed a searchROM that found it.
*/
void
DS2438_InitDev(struct W1Device * dev)
{
  int status;
  int tries = 0;
  // AddressDevice(dev);	not necessary
  while (1) {
    status = RecallAndReadPage(dev, 0);	// read page 0
    if (status) {
      DEBUG(errors) kdprintf(KR_OSTREAM,
             "DS2438 read page 0 status=%d, bytes=%d data= %#.2x %#.2x %#.2x\n",
             status, RunPgmMsg.rcv_sent, inBuf[0], inBuf[1], inBuf[2]);
      if (status == capros_W1Bus_StatusCode_BusError
          || status == capros_W1Bus_StatusCode_CRCError) {
        if (++tries < 4) {
          AddressDevice(dev);
          continue;	// try again
        }
      }
      dev->found = false;
      return;
    }
    break;
  }
  if ((inBuf[0] & 0x7f) != dev->u.bm.configReg
      || inBuf[7] != dev->u.bm.threshReg) {	// need to set configuration
    status = SetConfiguration(dev);
    if (status) {
      DEBUG(errors) kdprintf(KR_OSTREAM,
             "DS2438 SetConfiguration status=%d\n", status);
      dev->found = false;
      return;
    }
  }

  DEBUG(bm) kprintf(KR_OSTREAM, "DS2438 %#llx is found.\n",
                   dev->rom);
}

/* heartbeatSeed keeps all the different types of devices
from going off in sync. */
#define heartbeatSeed 3

struct W1Device * DS2438_samplingListHead;

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
  DEBUG(doall) {
    RecordCurrentTime();
    kprintf(KR_OSTREAM, "DS2438_HeartbeatAction called at %llu ms "
                 "wq0=%#x wq1=%#x\n",
                 currentTime/1000000,
                 DS2438_samplingQueue[0].next,
                 DS2438_samplingQueue[1].next);
  }

  uint32_t thisCount;
  if (hbCount == 0) {
    // First time after a boot. Sample all devices.
    thisCount = ~0;
  } else {
    thisCount = hbCount + heartbeatSeed;
  }
  MarkForSampling(thisCount, &DS2438_samplingQueue[0],
                  &DS2438_samplingListHead);

  if (DS2438_samplingListHead) {	// if there are any this time
    DoAllWorkFunction = &ConvertV;
    DoAll(&root);
    // We could be busy on the Convert V for 10 ms:
    RecordCurrentTime();
    RecordCurrentRTC();
    VEEExpiry = currentTime + 10000000;
    DEBUG(bm) kprintf(KR_OSTREAM, "Sampled at %llu ms", currentTime/1000000);

    UnmarkSamplingList(DS2438_samplingListHead);

    // Record the time sampled:
    struct W1Device * dev;
    for (dev = DS2438_samplingListHead; dev; dev = dev->nextInSamplingList) {
      dev->u.bm.vTime = currentRTC;
      dev->u.bm.voltageIsRead = false;	// converted, but not read
    }

    /* Wait right now until the DS2438's aren't busy.
     * That way, when we continue to the DS18B20 heartbeat, which
     * broadcasts Convert T, which may inadvertently be seen by DS2438's,
     * those DS2438's won't be busy and so won't be confused. */
    WaitUntilNotBusy();
  }
  DEBUG(doall) kprintf(KR_OSTREAM, "DS2438_HeartbeatAction done\n");
}

void
DS2438_ProcessRequest(struct W1Device * dev, Message * msg)
{
  switch (msg->rcv_code) {
  default:
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_key_getType:
    msg->snd_w1 = IKT_capros_DS2438;
    break;

  case OC_capros_DS2438_configureTemperature:
  {
    if (msg->rcv_w1 > 255) {
reqerr:
      msg->snd_code = RC_capros_key_RequestError;
    } else {
      dev->u.bm.tempLog2Sec = msg->rcv_w1;
    }
    break;
  }

  case OC_capros_DS2438_configureVoltage:
  {
    unsigned int vddBit = msg->rcv_w1 ? scr_AD : 0;
    unsigned int log2Seconds = msg->rcv_w2;
    if (log2Seconds > 255)
      goto reqerr;

    link_Unlink(&dev->samplingQueueLink);
    if (log2Seconds != 255) {
      if (log2Seconds > maxLog2Seconds)
        log2Seconds = maxLog2Seconds;	// no harm in sampling more often
      link_insertAfter(&DS2438_samplingQueue[log2Seconds],
                       &dev->samplingQueueLink);
    }

    DEBUG(bm) kprintf(KR_OSTREAM, "configureV %d", vddBit);
    if ((dev->u.bm.configReg & scr_AD) != vddBit) {	// changing AD bit
      dev->u.bm.configReg ^= scr_AD;
      dev->u.bm.vTime = 0;	// We don't have a reading for this voltage
      int status = SetConfigurationIfFound(dev);
      if (status) goto buserr;
    }
    break;
  }

  case OC_capros_DS2438_configureCurrent:
  {
    unsigned int conf = msg->rcv_w1;
    switch (conf) {
    default:
      goto reqerr;
    case capros_DS2438_CurrentConfig_off:
    case capros_DS2438_CurrentConfig_noAccum:
    case capros_DS2438_CurrentConfig_AccumNoEE:
    case capros_DS2438_CurrentConfig_AccumEE:
      break;
    }
    // The CurrentConfig values translate directly to IAD, CA, and EE.
    unsigned int newConfig = (dev->u.bm.configReg & 0xf8) | conf;
    DEBUG(bm) kprintf(KR_OSTREAM, "configC old %#.2x new %#.2x",
                dev->u.bm.configReg, newConfig);
    if (dev->u.bm.configReg != newConfig) {
      dev->u.bm.configReg = newConfig;
      int status = SetConfigurationIfFound(dev);
      if (status) goto buserr;
    }
    break;
  }

  case OC_capros_DS2438_getTemperature:
  {
    /* We do not broadcast Convert T (that is, send with Skip ROM) for DS2438's
    because that could trigger DS18B20's, which would make them busy
    for up to 750 ms. Instead, we just read each device when we need to. */
    RecordCurrentRTC();
    // If the reading we have is too old, read again.
    if ((dev->u.bm.tTime == 0
         || currentRTC >= dev->u.bm.tTime + (1 << dev->u.bm.tempLog2Sec) )
        && dev->found ) {
      RecordCurrentTime();
      WaitUntilNotBusy();
      AddressDevice(dev);
      wp(capros_W1Bus_stepCode_writeBytes)
      wp(1)
      wp(0x44)	// Convert T
      int status = RunProgram();
      if (status) {
buserr:
        msg->snd_code = RC_capros_DS2438_BusError;
        break;
      }
      RecordCurrentRTC();
      RecordCurrentTime();
      latestConvertTTime = currentTime;
      WaitUntilNotBusy();	// wait for the convert T to finish
      do {
        int status = AddressRecallAndReadPage(dev, 0);
        if (status)
          goto buserr;
      } while (inBuf[0] & scr_TB);	// if still busy, try again
      dev->u.bm.tTime = currentRTC;
      dev->u.bm.temperature = inBuf[1] | (inBuf[2] << 8);
    }
    msg->snd_w1 = dev->u.bm.temperature;
    msg->snd_w2 = dev->u.bm.tTime;
    break;
  }

  case OC_capros_DS2438_getVoltage:
  {
    /* We periodically issue Convert V commands.
    If voltageIsRead, we have read the voltage and saved it in dev. */
    if (! dev->u.bm.voltageIsRead
        && dev->found ) {
      /* Get the result from the device. */
      WaitUntilNotBusy();
      do {
        int status = AddressRecallAndReadPage(dev, 0);
        if (status)
          goto buserr;
      } while (inBuf[0] & scr_ADB);	// if still busy, try again
      dev->u.bm.voltageIsRead = true;
      dev->u.bm.voltage = inBuf[3] | (inBuf[4] << 8);
    }
    msg->snd_w1 = dev->u.bm.voltage;
    msg->snd_w2 = dev->u.bm.vTime;
    break;
  }

  case OC_capros_DS2438_getCurrent:
  {
    if (! dev->found) {
      msg->snd_code = RC_capros_DS2438_Offline;
    } else {
      WaitUntilNotBusy();
      int status = AddressRecallAndReadPage(dev, 0);
      if (status)
        goto buserr;
      RecordCurrentRTC();
      msg->snd_w1 = inBuf[5] | (inBuf[6] << 8);
      msg->snd_w2 = currentRTC;
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
      if (status) goto buserr;
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
