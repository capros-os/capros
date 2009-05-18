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

/* NPLink
   This persistent program receives capabilities from non-persistent drivers
   on boot or any other time they become available.
 */

#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Number.h>
#include <idl/capros/NPLink.h>
#include <idl/capros/NPLinkee.h>
#include <idl/capros/W1Bus.h>
#include <idl/capros/W1Mult.h>
#include <idl/capros/SerialPort.h>
#include <idl/capros/NPIP.h>
#include <idl/capros/SpaceBank.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>

#define dbg_run 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Memory:
  0: nothing
  0x01000: code
  0x1f000: stack */

uint32_t __rt_unkept = 1;

#define KC_DS9490R 0
#define KC_DS9097U 1
#define KC_IP      2
#define KC_Serial3 3
#define KC_Serial6 6
#define KC_SB      10

#define KR_OSTREAM KR_APP(0)

void
AssignW1BusTo(uint32_t kc, uint32_t w, const char * name)
{
  result_t result;
  result = capros_Node_getSlotExtended(KR_CONSTIT, kc, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_W1Mult_registerBus(KR_TEMP0, KR_ARG(0), w);
  switch (result) {
  default:
    kprintf(KR_OSTREAM, "Handler for %s returned %#x\n", name, result);
    break;
  case RC_capros_key_Void:
    kprintf(KR_OSTREAM, "No handler for %s\n", name);
  case RC_OK:
    break;
  }
}

void
AssignW1Bus(Message * msg)
{
  result_t result;

  // This logic is very ad-hoc.
  switch (msg->rcv_w2) {
    case capros_W1Bus_BusType_DS9490R:	// USB 1-wire master
      /* If we wanted to support multiple DS9490R's, 
      we should here search for the included custom DS2401 with ROM ID
      0x...81 (start the search with that value) to identify which one it is. */
      AssignW1BusTo(KC_DS9490R, msg->rcv_w2, "DS9490R USB 1-wire master");
      break;

    case capros_W1Bus_BusType_DS9097U:	// serial 1-wire master
      result = capros_Node_getSlotExtended(KR_CONSTIT, KC_DS9490R, KR_TEMP0);
      assert(result == RC_OK);
      result = capros_W1Mult_registerBus(KR_TEMP0, KR_ARG(0), msg->rcv_w2);
      assert(result == RC_OK);
      break;
  }
}

void
AssignSerialPort(Message * msg)
{
  result_t result;
  capros_Node_extAddr_t slot;

  // The subtype in w2 is the hardware port number.
  switch (msg->rcv_w2) {
    default:
      kprintf(KR_OSTREAM, "Unknown serial port # %d.\n", msg->rcv_w2);
      return;
    case 3:	// UART3
      slot = KC_Serial3;
      break;
    case 6:	// Netburner port 3
      slot = KC_Serial6;
      break;
  }
  DEBUG(run) kprintf(KR_OSTREAM, "nplink assigning serial port... ");
  result = capros_Node_getSlotExtended(KR_CONSTIT, slot, KR_TEMP0);
  assert(result == RC_OK);

  result = capros_NPLinkee_registerNPCap(KR_TEMP0, KR_ARG(0),
                                         msg->rcv_w1, msg->rcv_w2);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "No recipient for serial port %d cap.\n", msg->rcv_w2);
  }

  DEBUG(run) kprintf(KR_OSTREAM, "nplink done.\n");
}

void
AssignIp(Message * msg)
{
  result_t result;

  // This logic is very ad-hoc.
  // The subtype in w2 is the device number.
  switch (msg->rcv_w2) {
    default:
      kprintf(KR_OSTREAM, "Unknown IP # %d.\n", msg->rcv_w2);
      break;
    case 0:
      result = capros_Node_getSlotExtended(KR_CONSTIT, KC_IP, KR_TEMP0);
      assert(result == RC_OK);
      result = capros_NPLinkee_registerNPCap(KR_TEMP0, KR_ARG(0),
                                             msg->rcv_w1, msg->rcv_w2);
      if (result != RC_OK) {
        kprintf(KR_OSTREAM, "No recipient for IP cap.\n");
      }
      break;
  }
}

void
AssignSB(Message * msg)
{
  result_t result;

  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_SB, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_NPLinkee_registerNPCap(KR_TEMP0, KR_ARG(0),
                                         msg->rcv_w1, 0);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "No recipient for SB cap.\n");
  }
}

int
main(void)
{
  Message Msg;
  
  Msg.snd_invKey = KR_VOID;
  Msg.snd_key0 = KR_VOID;
  Msg.snd_key1 = KR_VOID;
  Msg.snd_key2 = KR_VOID;
  Msg.snd_rsmkey = KR_VOID;
  Msg.snd_len = 0;
  Msg.snd_code = 0;
  Msg.snd_w1 = 0;
  Msg.snd_w2 = 0;
  Msg.snd_w3 = 0;

  Msg.rcv_key0 = KR_ARG(0);
  Msg.rcv_key1 = KR_VOID;
  Msg.rcv_key2 = KR_VOID;
  Msg.rcv_rsmkey = KR_RETURN;
  Msg.rcv_limit = 0;
  
  // kdprintf(KR_OSTREAM, "nplink: accepting requests\n");

  for(;;) {
    RETURN(&Msg);

    DEBUG(run) kprintf(KR_OSTREAM, "nplink was called, oc=%#x w1=%#x\n",
                       Msg.rcv_code, Msg.rcv_w1);

    // Defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_NPLink;
      break;

    case OC_capros_NPLink_RegisterNPCap:
      switch (Msg.rcv_w1) {
      default:
        kprintf(KR_OSTREAM, "nplink: Unrecognized type %#x!\n", Msg.rcv_w1);
        break;
      case IKT_capros_W1Bus:
        AssignW1Bus(&Msg);
        break;
      case IKT_capros_SerialPort:
        AssignSerialPort(&Msg);
        break;
      case IKT_capros_NPIP:
        AssignIp(&Msg);
        break;
      case IKT_capros_SpaceBank:
        AssignSB(&Msg);
        break;
      }
      break;
    }
  }
}
