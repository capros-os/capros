/*
 * Copyright (C) 2010, Strawberry Development Group.
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

/* This program provides access to a subset of the bits of a DS2408.
 * The subset is in the keyInfo field.
 */

#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Number.h>
#include <idl/capros/DS2408.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Bypass all the usual initialization. */
unsigned long __rt_runtime_hook = 0;
uint32_t __rt_unkept = 1;

#define KR_OSTREAM KR_APP(0)
#define KR_DS2408  KR_APP(1)
/* This program should be the only holder of the DS2408 cap.
 * so no one else will interfere with its operation. */

unsigned int outputs = 0xff;

/* Map the client's bits to the bit positions in the mask. */
static uint8_t
MapBits(uint8_t mask, uint8_t bits)
{
  int i;
  unsigned int out;
  for (i = 0, out = 0;  mask;  i++, mask >>= 1) {
    if (mask & 1) {
      out |= (bits & 1) << i;
      bits >>= 1;
    }
  }
  return out;
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

  Msg.rcv_key0 = KR_VOID;
  Msg.rcv_key1 = KR_VOID;
  Msg.rcv_key2 = KR_VOID;
  Msg.rcv_rsmkey = KR_RETURN;
  Msg.rcv_limit = 0;
  
  for(;;) {
    RETURN(&Msg);

    // Defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;

    uint8_t mask = Msg.rcv_keyInfo;

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_DS2408;
      break;

    case OC_capros_DS2408_setOutputs:
    {
      uint8_t bts = MapBits(mask, Msg.rcv_w1);
      outputs &= ~ mask;
      outputs |= bts;
      Msg.snd_code = capros_DS2408_setOutputs(KR_DS2408, outputs);
      break;
    }
    }
  }
}
