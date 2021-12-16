/*
 * Copyright (C) 2010, 2012, Strawberry Development Group.
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

#include <idl/capros/DS2408.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include "DS2408.h"
#include "w1mult.h"

void
DS2408_Init(void)
{
}

/* This is called to initialize a configured device.
This happens once at the big bang, or if a new device is configured.
The device hasn't been located on the network yet. */
void
DS2408_InitStruct(struct W1Device * dev)
{
  /* Default output state is, don't pull to ground. */
  dev->u.pio8.outputState = 0xff;
}

static bool
RetryableStatus(int status)
{
  return status >= capros_W1Bus_StatusCode_CRCError;
}

// The caller must have checked that the device is not busy.
// The caller must address the device first.
static int
WriteOutput(struct W1Device * dev, uint8_t output)
{
  NotReset();
  wp(capros_W1Bus_stepCode_writeBytes)
  wp(3)
  wp(0x5a)	// Channel-Access Write
  wp(output)
  wp(~ output)
  wp(capros_W1Bus_stepCode_readBytes)
  wp(1)
  /* The DS2408 sends the PIO status, but we don't read it, because
     it may be affected by inputs. */
  int status = RunProgram();
  if (status) {
    DEBUG(errors) kprintf(KR_OSTREAM, "DS2408 %#llx Write status %#x!\n",
                          dev->rom, status);
    return status;
  }
  switch (inBuf[0]) {
  default:
    return capros_W1Bus_StatusCode_BusError;	// unexpected data

  case 0xff:	// The DS2408 did not receive correctly
    return capros_W1Bus_StatusCode_CRCError;

  case 0xaa:	// The DS2408 did receive correctly
    return capros_W1Bus_StatusCode_OK;
  }
} 

static inline int
AddressWriteOutput(struct W1Device * dev, uint8_t output)
{
  int tries = 0;
  while (1) {
    AddressDevice(dev);

    int status = WriteOutput(dev, output);
    if (status) {
      if (RetryableStatus(status)) {
        if (++tries < 4) {
          continue;	// try again
        }
      }
    }
    return status;
  }
}

/* This is called to initialize a device that has been found on the network.
This is called on every reboot and whenever the bus is rescanned.
This procedure can issue device I/O, but must not take a long time.
The device is addressed, since we just completed a searchROM that found it.
Returns true iff dev is OK.
*/
bool
DS2408_InitDev(struct W1Device * dev)
{
  int status;
  int tries = 0;

  /* We never do conditional search, so the configuration of the
     control register bits PLS, CT and PORL don't matter.
     We don't allow configuring ROS (yet).
     So we don't need to set the control register. */
  while (1) {
    if (tries != 0)
      AddressDevice(dev);	// not necessary first time
    status = WriteOutput(dev, dev->u.pio8.outputState);	// restore outputState
    if (! status)
      break;
    if (RetryableStatus(status)) { 
      if (++tries < 4) {
        continue;     // try again
      }
    }
    kprintf(KR_OSTREAM, "DS2408 %#llx permanent error.\n", dev->rom);
    return false;
  }

  DEBUG(gpio8) kprintf(KR_OSTREAM, "DS2408 %#llx is found.\n",
                   dev->rom);
  return true;
}

void
DS2408_HeartbeatAction(uint32_t hbCount)
{
}

void
DS2408_ProcessRequest(struct W1Device * dev, Message * msg)
{
  switch (msg->rcv_code) {
  default:
    msg->snd_code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_key_getType:
    msg->snd_w1 = IKT_capros_DS2408;
    break;

  case OC_capros_DS2408_setOutputs:
  {
    uint8_t output = msg->rcv_w1;
    if (! dev->found) {
      msg->snd_code = RC_capros_DS2408_Offline;
    } else {
      int status = AddressWriteOutput(dev, output);
      if (status) {
        msg->snd_code = RC_capros_DS2408_BusError;
        break;
      }
      dev->u.pio8.outputState = output;	// so we can restore it
    }
    break;
  }

  }
}
