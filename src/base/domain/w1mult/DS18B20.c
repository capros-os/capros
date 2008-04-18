/*
 * Copyright (C) 2008, Strawberry Development Group.
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

#include <idl/capros/DS18B20.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include "DS18B20.h"
#include "w1mult.h"

Link DS18B20_workQueue[maxLog2Seconds+1];

/* Read the scratchpad into inBuf. */
static int
ReadSpad(struct W1Device * dev)
{
  // The device has already been addressed.
  /* Can't use step code readCRC8, because the command code is not
  followed by any bytes of address. */
  WriteOneByte(0xbe);	// Read Scrachpad command
  wp(capros_W1Bus_stepCode_readBytes)
  wp(9)
  int status = RunProgram();
  if (status) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS18B20 spad status %d\n", status);
    return status;
  }
  assert(RunPgmMsg.rcv_sent == 9);
  uint8_t crc = CalcCRC8(&inBuf[0], 8);
  if (crc != inBuf[8]) {
    DEBUG(errors) kprintf(KR_OSTREAM,
                          "DS18B20 spad crc calc %#.2x read %#.2x\n",
                          crc, inBuf[8]);
    return 100;
  }
  return 0;
}

int // returns -1 if bus gone, 1 if error, 0 if OK
SetConfiguration(struct W1Device * dev)
{
  DEBUG(thermom) kprintf(KR_OSTREAM, "Setting config");
  uint8_t configReg = (dev->u.thermom.resolution - 1) << 5;
  assert(ProgramIsClear());
  AddressDevice(dev);
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(4)
  wp(0x4e)	// write scratchpad
  wp(0x7f)	// High alarm - disable
  wp(0x80)	// Low alarm - disable
  wp(configReg)	// configuration register
  NotReset();
  // Read scratchpad back to verify.
  AddressDevice(dev);
  int status = ReadSpad(dev);
  if (status) return status;
  if ((inBuf[4] & 0x60) != configReg) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS18B20 config wrote %#.2x read %#.2x\n",
                          configReg, inBuf[4]);
    return 101;
  }

  // Remember to write the EEPROM later.
  /* We don't write it now, because it takes 10ms, and could interfere
  with sampling the temperature. */
  dev->u.thermom.needToWriteEEPROM = true;
#if 0
  // Write the scratchpad to EEPROM
  assert(ProgramIsClear());
  AddressDevice(dev);
  WriteOneByte(0x48);	// copy scratchpad

  // Device will be busy for 10 ms.
  dev->busyUntil = GetCurrentTime() + 10000000;
#endif
  return 0;
}

void
DS18B20_Init(void)
{
  int i;
  for (i = 0; i <= maxLog2Seconds; i++) {
    link_Init(&DS18B20_workQueue[i]);
  }
}

/* This is called to initialize a configured device.
This happens once at the big bang, or if a new device is configured.
The device hasn't been located on the network yet. */
void
DS18B20_InitStruct(struct W1Device * dev)
{
  dev->u.thermom.time = 0;  // no temperature yet
  dev->u.thermom.resolution = 255;  // not specified yet
  dev->u.thermom.needToWriteEEPROM = false;
}

/* This is called to initialize a device that has been found on the network.
This is called at least on every reboot.
This procedure can issue device I/O, but must not take a long time.
The device is addressed, since we just completed a searchROM that found it.
*/
void
DS18B20_InitDev(struct W1Device * dev)
{
  assert(ProgramIsClear());
  // AddressDevice(dev);	not necessary
  WriteOneByte(0xb4);	// read power supply
  wp(capros_W1Bus_stepCode_write1Read)
  int status = RunProgram();
  if (status) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS18B20 read power status %d\n", status);
    dev->found = false;
    return;
  }
  assert(RunPgmMsg.rcv_sent == 1);
  if (inBuf[0] != 1) {
    kprintf(KR_OSTREAM, "DS18B20 %#llx is parasite powered, not supported\n",
            dev->rom);
    dev->found = false;
    return;
  }
  DEBUG(thermom) kprintf(KR_OSTREAM, "DS18B20 %#llx is bus powered %#x %d\n",
                   dev->rom, dev, dev->u.thermom.resolution);
  if (dev->u.thermom.resolution != 255) {	// if it's configured
    // Make sure the device's configuration matches what we want.
    AddressDevice(dev);
    status = ReadSpad(dev);
    if (status) {
      DEBUG(errors) kprintf(KR_OSTREAM, "DS18B20 spad status %d\n", status);
      dev->found = false;
      return;
    }
    if ((inBuf[4] & 0x60) != ((dev->u.thermom.resolution - 1) << 5)) {
      status = SetConfiguration(dev);
      if (status) {
        DEBUG(errors) kprintf(KR_OSTREAM, "DS18B20 set config status %d\n", status);
        dev->found = false;
        return;
      }
    }
  }
}

/* heartbeatSeed keeps all the different types of devices
from going off in sync. */
#define heartbeatSeed 0

struct W1Device * DS18B20_samplingListHead;

/* We can save addressing each individual device
 * and just issue a Convert T to all active devices. */
void
ConvertT(struct Branch * br)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "ConvertT called.\n");
  /* Convert only on this branch, so we must have a smart-on: */
  EnsureBranchSmartReset(br);
  wp(capros_W1Bus_stepCode_skipROM);
  WriteOneByte(0x44);	// Convert T
  RunProgram();
}

uint64_t sampledTime;

static void
readTemperature(struct W1Device * dev)
{
  // The device is on active branches, so we can just address it:
  assert(ProgramIsClear());
  ProgramReset();
  ProgramMatchROM(dev);
  ReadSpad(dev);//// check return
  dev->u.thermom.time = sampledTime;
  dev->u.thermom.temperature = inBuf[0] + (inBuf[1] << 8);
  //// Somewhere, check needToWriteEEPROM
}

static void
readResultsFunction(void * arg)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "readResultsFunction\n");
  MarkSamplingList(DS18B20_samplingListHead);
  DoAllWorkFunction = &DoEach;
  DoEachWorkFunction = &readTemperature;
  DoAll(&root);

  // Next heartbeat can proceed:
  EnableHeartbeat(hbBit_DS18B20);
  DEBUG(doall) kprintf(KR_OSTREAM, "readResultsFunction done\n");
}

static struct w1Timer readResultsTimer = {
  .function = &readResultsFunction
};

void
DS18B20_HeartbeatAction(uint32_t hbCount)
{
  DEBUG(doall) kprintf(KR_OSTREAM, "DS18B20_HeartbeatAction called\n"
                 "wq0=%#x wq1=%#x\n", DS18B20_workQueue[0].next,
                 DS18B20_workQueue[1].next);
  uint32_t thisCount;
  if (hbCount == 0) {
    // First time after a boot. Sample all devices.
    thisCount = ~0;
  } else {
    thisCount = hbCount + heartbeatSeed;
  }
  MarkForSampling(thisCount, &DS18B20_workQueue[0],
                  &DS18B20_samplingListHead);
  if (DS18B20_samplingListHead) {	// if there are any this time
    struct W1Device * dev;

    // Calculate the maximum conversion time:
    uint8_t maxResolution = 1;
    for (dev = DS18B20_samplingListHead; dev; dev = dev->nextInSamplingList) {
      if (dev->u.thermom.resolution > maxResolution)
        maxResolution = dev->u.thermom.resolution;
    }

    DoAllWorkFunction = &ConvertT;
    assert(ProgramIsClear());
    ProgramReset();
    DoAll(&root);

    // Wait until all conversions are complete.
    RecordCurrentTime();
    sampledTime = currentTime;
    // A resolution of 1 binary digit takes 93.75 milliseconds.
    readResultsTimer.expiration = sampledTime
      + (93750000ULL << (maxResolution - 1));	// in nanoseconds
    InsertTimer(&readResultsTimer);

    // Don't let the heart beat again until we are done with this round:
    DisableHeartbeat(hbBit_DS18B20);
  }
  DEBUG(doall) kprintf(KR_OSTREAM, "DS18B20_HeartbeatAction done\n");
}

void
DS18B20_ProcessRequest(struct W1Device * dev, Message * msg)
{
  switch (msg->rcv_code) {
  default:
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_key_getType:
    msg->snd_w1 = IKT_capros_DS18B20;
    break;

  case OC_capros_DS18B20_configure:
  {
    uint8_t log2Seconds = msg->rcv_w1;
    uint8_t res = msg->rcv_w2;

    if (res < 1 || res > 4) {
      msg->snd_code = RC_capros_key_RequestError;
    } else {
      dev->u.thermom.resolution = res;

      if (log2Seconds > maxLog2Seconds)
        log2Seconds = maxLog2Seconds;	// no harm in sampling more often
      link_Unlink(&dev->workQueueLink);
      link_insertAfter(&DS18B20_workQueue[log2Seconds], &dev->workQueueLink);

      if (dev->found)
        SetConfiguration(dev);	// Set configuration in device.
    }
    break;
  }

  case OC_capros_DS18B20_getTemperature:
  {//// wait if no data?
    msg->snd_w1 = dev->u.thermom.temperature;
    msg->snd_w2 = (uint32_t)dev->u.thermom.time;
    msg->snd_w3 = (dev->u.thermom.time >> 32);
  }
    break;

  }
}
