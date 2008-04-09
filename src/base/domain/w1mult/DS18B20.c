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

/* We will sample at least every 2**7 seconds (about 2 minutes). */
#define maxLog2Seconds 7

Link DS18B20_workQueue[maxLog2Seconds+1];

int // returns -1 if bus gone, 1 if error, 0 if OK
SetConfiguration(struct W1Device * dev)
{
  uint8_t configReg = (dev->u.thermom.resolution - 1) << 5;
  ClearProgram();
  AddressDevice(dev);
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(4)
  wp(0x4e)	// write scratchpad
  wp(0x7f)	// High alarm - disable
  wp(0x80)	// Low alarm - disable
  wp(configReg)	// configuration register
  // Read scratchpad back to verify.
  AddressDevice(dev);
  /* Can't use step code readCRC8, because the command code is not
  followed by any bytes of address. */
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(1)	
  wp(0xbe)	// Read Scrachpad command
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
  if (inBuf[4] != configReg) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS18B20 config wrote %#.2x read %#.2x\n",
                          configReg, inBuf[4]);
    return 101;
  }

  // Write the scratchpad to EEPROM
  ClearProgram();
  AddressDevice(dev);
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(1)
  wp(0x48)	// copy scratchpad

  // Device will be busy for 10 ms.
  dev->busyUntil = GetCurrentTime() + 10000000;
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

void
DS18B20_InitDev(struct W1Device * dev)
{
  dev->u.thermom.time = 0;  // no temperature yet
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

      SetConfiguration(dev);	// Set configuration in device.
    }
    break;
  }

  case OC_capros_DS18B20_getTemperature:
  {
    msg->snd_w1 = 0;////
    msg->snd_w2 = (uint32_t)0;////
    msg->snd_w3 = (0ULL >> 32);
  }
    break;

  }
}
