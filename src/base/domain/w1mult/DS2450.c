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

#include <idl/capros/DS2450.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include "DS2450.h"
#include "w1mult.h"

// Bits in control information:
#define lo_OC  0x40
#define lo_OE  0x80
#define hi_IR  0x01
#define hi_POR 0x80

Link DS2450_samplingQueue[maxLog2Seconds+1];

bool converting = false;
struct W1Device * DS2450_workList = NULL;

static void
EnsureOnWorkList(struct W1Device * dev)
{
  if (! dev->onWorkList) {
    dev->onWorkList = true;
    dev->nextInWorkList = DS2450_workList;
    DS2450_workList = dev;
  }
}

static unsigned int currentAddr;

int
WriteDS2450MemoryFirst(struct W1Device * dev,
  unsigned int startAddr, uint8_t data)
{
  int tries = 0;
  while (1) {
    AddressDevice(dev);
    NotReset();
    crc = 0;
    wp(capros_W1Bus_stepCode_writeBytes)
    wp(4)
    ProgramByteCRC16(0x55);	// write memory
    currentAddr = startAddr;
    ProgramByteCRC16(startAddr);
    ProgramByteCRC16(0);	// high addr
    ProgramByteCRC16(data);
    wp(capros_W1Bus_stepCode_readBytes)
    wp(3)		// read CRC16 and data byte
    int status = RunProgram();
    if (status)
      return status;
    if (! CheckCRC16()) {
      if (++tries < 4)
        continue;	// try again
      DEBUG(errors) kprintf(KR_OSTREAM,
                      "DS2450 write memory CRC error, giving up\n");
      return capros_W1Bus_StatusCode_CRCError;
    }
    if (inBuf[2] != data) {
      DEBUG(errors) kprintf(KR_OSTREAM,
                      "DS2450 write memory wrote %#.2x, got %#.2x!\n",
                      data, inBuf[2]);
      if (++tries < 4)
        continue;	// try again
      return capros_W1Bus_StatusCode_BusError;
    }
    return 0;	// success
  }
}

int
WriteDS2450Memory(struct W1Device * dev, uint8_t data)
{
  crc = ++currentAddr;
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(1)
  ProgramByteCRC16(data);
  wp(capros_W1Bus_stepCode_readBytes)
  wp(3)		// read CRC16 and data byte
  int status = RunProgram();
  if (status) return status;
  if (! CheckCRC16())
    return capros_W1Bus_StatusCode_CRCError;
  if (inBuf[2] != data) {
    DEBUG(errors) kprintf(KR_OSTREAM,
                    "DS2450 write memory wrote %#.2x, got %#.2x!\n",
                    data, inBuf[2]);
    return capros_W1Bus_StatusCode_BusError;
  }
  return capros_W1Bus_StatusCode_OK;	// success
}

// Send the desired configuration to the device.
static int
SendConfig(struct W1Device * dev)
{
  int i;
  uint8_t * requestedBytes = (void *)&dev->u.ad.requestedCfg[0];
  uint8_t * devBytes = (void *)&dev->u.ad.devCfg[0];

  // Find the first byte that differs:
  int firstByte;
  for (firstByte = 0; firstByte < 8; firstByte++)
    if (requestedBytes[firstByte] != devBytes[firstByte])
      break;
  if (firstByte == 8)	// no change
    return capros_W1Bus_StatusCode_OK;

  // Find the last byte that differs:
  int lastByte;
  for (lastByte = 7; lastByte >= 0; lastByte--)
    if (requestedBytes[lastByte] != devBytes[lastByte])
      break;
  assert(lastByte >= 0);

  DEBUG(ad) kprintf(KR_OSTREAM, "Setting config");
  int tries = 0;
  while (1) {
    int status = WriteDS2450MemoryFirst(dev,
                   8 + firstByte, requestedBytes[firstByte]);
    if (status) {
statuserr:
      if (status == capros_W1Bus_StatusCode_CRCError
          || status == capros_W1Bus_StatusCode_BusError) {
        if (++tries < 4)
          continue;	// try again
      }
      return status;
    }
    for (i = firstByte+1; i <= lastByte; i++) {
      status = WriteDS2450Memory(dev, requestedBytes[i]);
      if (status) goto statuserr;
    }
  }
  memcpy(&dev->u.ad.devCfg, &dev->u.ad.requestedCfg, 8);
  return capros_W1Bus_StatusCode_OK;	// success
}

void
DS2450_Init(void)
{
  int i;
  for (i = 0; i <= maxLog2Seconds; i++) {
    link_Init(&DS2450_samplingQueue[i]);
  }
}

// Called when both found and client has configured.
static void
CheckConfigured(struct W1Device * dev)
{
  if (dev->u.ad.devCfg != dev->u.ad.requestedCfg) {
    // The configuration needs to be sent to the device.
    if (converting) {
      // Don't send the configuration now,
      // because the device might be in the middle of a conversion.
      EnsureOnWorkList(dev);
    } else {
      SendConfig(dev);
    }
  }
  // The configuration will be loaded into SPAD at the beginning
  // of the next heartbeat, and into EEPROM at the end of that heartbeat.
}

/* This is called to initialize a configured device.
This happens once at the big bang, or if a new device is configured.
The device hasn't been located on the network yet. */
void
DS2450_InitStruct(struct W1Device * dev)
{
  int i;

  link_Init(&dev->u.ad.samplingQueueLink);
  dev->u.ad.requestedCfg[0].cfglo = 0xff;  // not configured yet
  for (i = 0; i < 4; i++) {
    dev->u.ad.port[i].logSlot = -1;	// no log yet
  }
}

// The device must be addressed before calling.
int
ReadMemPage(struct W1Device * dev, unsigned int addr)
{
  int tries = 0;
  while (1) {
    NotReset();
    wp(capros_W1Bus_stepCode_readCRC16)
    wp(3)	// page is 8 bytes
    wp(1)	// read one page
    wp(0xaa)	// read memory command
    wp(addr)	// page 1
    wp(0)
    int status = RunProgram();
    if (status != capros_W1Bus_StatusCode_CRCError) {
      if (! status) {
        assert(RunPgmMsg.rcv_sent == 8);
      }
      return status;
    }
    DEBUG(errors) kprintf(KR_OSTREAM, "DS2450 ReadMemPage got status %d!\n",
                          status);
    if (++tries >= 4)
      return status;
    AddressDevice(dev);
  }
}

/* This is called to initialize a device that has been found on the network.
This is called at least on every reboot.
This procedure can issue device I/O, but must not take a long time.
The device is addressed, since we just completed a searchROM that found it.
*/
void
DS2450_InitDev(struct W1Device * dev)
{
  int i;

  // AddressDevice(dev);	not necessary
  int status = ReadMemPage(dev, 8);	// read page 1
  if (status) {
    DEBUG(errors) kdprintf(KR_OSTREAM,
           "DS2450 read config status=%d, bytes=%d data= %#.2x %#.2x %#.2x\n",
           status, RunPgmMsg.rcv_sent, inBuf[0], inBuf[1], inBuf[2]);
    dev->found = false;
    return;
  }
  memcpy(&dev->u.ad.devCfg, &inBuf, 8);
  // Check power-on reset:
  if (dev->u.ad.devCfg[0].cfghi & hi_POR) {
    DEBUG(ad) kprintf(KR_OSTREAM, "DS2450 %#llx had POR\n", dev->rom);
    int tries = 0;
    while (1) {
      status = WriteDS2450MemoryFirst(dev, 0x1c, 0x40);
      if (status) {
        DEBUG(errors) kprintf(KR_OSTREAM, "DS2450 write calib %d\n", status);
        if (status == capros_W1Bus_StatusCode_CRCError
            || status == capros_W1Bus_StatusCode_BusError) {
          if (++tries < 4)
            continue;	// try again
        }
        dev->found = false;
        return;
      }
      break;
    }
    
    // Clear POR in devCfg:
    for (i = 0; i < 4; i++) {
      dev->u.ad.devCfg[i].cfghi &= ~ hi_POR;
    }
  }

  DEBUG(ad) kprintf(KR_OSTREAM, "DS2450 %#llx is found.\n",
                   dev->rom);
  if (dev->u.ad.requestedCfg[0].cfglo != 0xff) {	// if it's configured
    CheckConfigured(dev);
  }

  // Ensure we log the next readings:
  for (i = 0; i < 4; i++) {
    dev->u.ad.port[i].hysteresisLow = 0x20000;
  }
}

/* At the end of heartbeat work, all DS2450's are idle, so we can
 * do any copying to EEPROM now.
 * When done and devices are all idle, we re-enable the heartbeat. */
static void
EndHeartbeat(void)
{
  // Any configuring to do?
  struct W1Device * dev = DS2450_workList;
  DS2450_workList = NULL;
  for (; dev; dev = dev->nextInWorkList) {
    CheckConfigured(dev);
    dev->onWorkList = false;
  }

  // Next heartbeat can proceed:
  EnableHeartbeat(hbBit_DS2450);
}

/* heartbeatSeed keeps all the different types of devices
from going off in sync. */
#define heartbeatSeed 1

struct W1Device * DS2450_samplingListHead;

bool ds2450ConvertedOK;

/* We can save addressing each individual device
 * and just issue a Convert to all active devices.
 * Even though some devices may not need all 4 ports converted,
 * it takes at most 5.12 ms to do all 4 conversions,
 * while it takes 5.28 ms to address an individual device. */
static void
Convert(struct Branch * br)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "Convert called.\n");
  int tries = 0;
  while (1) {
    /* Convert only on this branch, so we must have a smart-on: */
    EnsureBranchSmartReset(br);
    wp(capros_W1Bus_stepCode_skipROM);
    NotReset();
    crc = 0;
    wp(capros_W1Bus_stepCode_writeBytes)
    wp(3)
    ProgramByteCRC16(0x3c);	// Convert
    ProgramByteCRC16(0x0f);	// input select mask - all 4 channels
    ProgramByteCRC16(0x00);	// no presets

    wp(capros_W1Bus_stepCode_readBytes)
    wp(2)		// read CRC16
    int status = RunProgram();
    if (status) {
      DEBUG(errors) kprintf(KR_OSTREAM, "DS2450 Convert got status %d\n",
                             status);
      ds2450ConvertedOK = false;
      return;
    }
    if (! CheckCRC16()) {
      DEBUG(errors) kprintf(KR_OSTREAM, "DS2450 Convert got CRC error.\n");
      if (++tries < 4)
        continue;	// try again
      ds2450ConvertedOK = false;
      return;
    }
    return;	// all is OK
  }
}

static void
readData(struct W1Device * dev)
{
  // The device is on active branches, so we can just address it:
  ProgramReset();
  ProgramMatchROM(dev);
  int status = ReadMemPage(dev, 0);	// read page 0
  if (status) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS2450 read data %d\n", status);
    return;
  }
  DEBUG(doall) kprintf(KR_OSTREAM, "DS2450 %#llx sampled at %u\n",
                       dev->rom, sampledTime);
  int i;
  for (i = 0; i < 4; i++) {
    if (! (dev->u.ad.requestedCfg[i].cfglo & lo_OE)) {	// input port
      uint16_t data = inBuf[i*2] | (inBuf[i*2+1] << 8);
      if (data < dev->u.ad.port[i].hysteresisLow
          || data > dev->u.ad.port[i].hysteresisLow
                           + dev->u.ad.port[i].hysteresis ) {
        // Temperature changed sufficiently to log.
        if (AddLogRecord16(dev->u.ad.port[i].logSlot, data, 0)) {
          if (data < dev->u.ad.port[i].hysteresisLow)
            dev->u.ad.port[i].hysteresisLow = data;
          else
            dev->u.ad.port[i].hysteresisLow = data
                                          - dev->u.ad.port[i].hysteresis;
        }
      }
    }
  }
}

static void
readResultsFunction(void * arg)
{
  DEBUG(doall) {
    RecordCurrentTime();
    kprintf(KR_OSTREAM, "DS2450_readResultsFunction at %lld\n",
            currentTime/1000000);
  }
  MarkSamplingList(DS2450_samplingListHead);
  DoAllWorkFunction = &DoEach;
  DoEachWorkFunction = &readData;
  DoAll(&root);
  UnmarkSamplingList(DS2450_samplingListHead);

  converting = false;

  EndHeartbeat();
  DEBUG(doall) kprintf(KR_OSTREAM, "DS2450_readResultsFunction done\n");
}

static struct w1Timer readResultsTimer = {
  .link = link_Initializer(readResultsTimer.link),
  .function = &readResultsFunction
};

void
DS2450_HeartbeatAction(uint32_t hbCount)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "DS2450_HeartbeatAction called "
                 "wq0=%#x wq1=%#x\n", DS2450_samplingQueue[0].next,
                 DS2450_samplingQueue[1].next);

  uint32_t thisCount;
  if (hbCount == 0) {
    // First time after a boot. Sample all devices.
    thisCount = ~0;
  } else {
    thisCount = hbCount + heartbeatSeed;
  }
  MarkForSampling(thisCount, &DS2450_samplingQueue[0],
                  &DS2450_samplingListHead,
                  offsetof(struct W1Device, u.ad.samplingQueueLink) );

  // Don't let the heart beat again until we are done with this round:
  DisableHeartbeat(hbBit_DS2450);

  ds2450ConvertedOK = true;
  if (DS2450_samplingListHead) {	// if there are any this time
    DoAllWorkFunction = &Convert;
    DoAll(&root);

    UnmarkSamplingList(DS2450_samplingListHead);

    if (ds2450ConvertedOK) {
      // When all conversions are complete, read the results.
      RecordCurrentRTC();
      RecordCurrentTime();
      sampledTime = currentRTC;
      // Maximum conversion time is 5.12 ms.
      // There is no offset time because the device must be VCC powered.
      readResultsTimer.expiration = currentTime + 5120000ULL;
      DEBUG(doall) kprintf(KR_OSTREAM,
                           "DS2450 sampledTime %u expiration %lld",
                           sampledTime,
                           readResultsTimer.expiration/1000000);
      InsertTimer(&readResultsTimer);
      converting = true;
    } else {
      EndHeartbeat();
    }
  } else {
    EndHeartbeat();
  }
  DEBUG(doall) kprintf(KR_OSTREAM, "DS2450_HeartbeatAction done\n");
}

static void
ReturnLog(struct W1Device * dev,
  unsigned int i, cap_t kr, uint8_t * sndkey)
{
  if (dev->u.ad.requestedCfg[i].cfglo & lo_OE) {	// output port
    *sndkey = KR_VOID;		// no log needed for output
  } else {		// input port
    result_t result = capros_Node_getSlotExtended(KR_KEYSTORE,
                        dev->u.ad.port[i].logSlot, kr);
    assert(result == RC_OK);
    *sndkey = kr;
  }
}

void
DS2450_ProcessRequest(struct W1Device * dev, Message * msg)
{
  switch (msg->rcv_code) {
  default:
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_key_getType:
    msg->snd_w1 = IKT_capros_DS2450;
    break;

  case OC_capros_DS2450_configurePorts:
  {
    if (msg->rcv_sent < sizeof(capros_DS2450_portsConfiguration)) {
reqerr:
      msg->snd_code = RC_capros_key_RequestError;
err: ;
    } else {
      capros_DS2450_portConfiguration * portConfig = msg->rcv_data;
      int i;
      // Validate all before creating logs.
      for (i = 0; i < 4; i++) {
        if (! portConfig[i].output
            && (portConfig[i].bitsToConvert <= 0
                || portConfig[i].bitsToConvert > 16)) {
          goto reqerr;
        }
      }
      // Create all logs before setting requestedCfg.
      for (i = 0; i < 4; i++) {
        if (! portConfig[i].output
            && dev->u.ad.port[i].logSlot == -1) {
          result_t result = CreateLog(&dev->u.ad.port[i].logSlot);
          if (result != RC_OK) {
            msg->snd_code = result;
            goto err;
          }
        }
      }
      struct portCfg * cfg = &dev->u.ad.requestedCfg[0];
      unsigned int minLog2Seconds = maxLog2Seconds;
      for (i = 0; i < 4; i++) {
        if (portConfig[i].output) {
          cfg[i].cfglo = lo_OE | (portConfig[i].rangeOrOutput ? lo_OC : 0) | 1;
          cfg[i].cfghi = hi_IR;
        } else {
          cfg[i].cfglo = portConfig[i].bitsToConvert & 0xf;
          cfg[i].cfghi = (portConfig[i].rangeOrOutput ? hi_IR : 0);
          if (portConfig[i].log2Seconds < minLog2Seconds)
            minLog2Seconds = portConfig[i].log2Seconds;
        }
        dev->u.ad.port[i].hysteresis = portConfig[i].hysteresis;
        dev->u.ad.port[i].hysteresisLow = 0x20000;	// log the next reading
      }

      link_Unlink(&dev->u.ad.samplingQueueLink);
      link_insertAfter(&DS2450_samplingQueue[minLog2Seconds],
                       &dev->u.ad.samplingQueueLink);

      if (dev->found) {
        CheckConfigured(dev);
      }
      // Return the relevant logs.
      ReturnLog(dev, 0, KR_TEMP0, &msg->snd_key0);
      ReturnLog(dev, 1, KR_TEMP1, &msg->snd_key1);
      ReturnLog(dev, 2, KR_TEMP2, &msg->snd_key2);
      ReturnLog(dev, 3, KR_TEMP3, &msg->snd_rsmkey);
    }
    break;
  }
  }
}
