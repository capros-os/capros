/*
 * Copyright (C) 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <string.h>
#include <eros/Invoke.h>
#include <idl/capros/RendezvousGetCaller.h>
#include <domain/Runtime.h>
#include <domain/assert.h>
#include <eros/machine/cap-instr.h>

/* Bypass all the usual initialization. */
unsigned long __rt_runtime_hook = 0;

#define dbg_server 1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_OSTREAM KR_APP(0)
#define KR_PassedCap KR_APP(1)
#define KR_Caller KR_APP(2)
#define KR_Getter KR_APP(3)

#define keyInfo_RendezvousCall 0
#define keyInfo_RendezvousGetCaller 1

uint32_t savedCode;
bool haveCaller = false;
bool haveGetter = false;

void
CheckRendezvous(void)
{
  if (haveCaller && haveGetter) {
    Message Msg = {
      .snd_invKey = KR_Getter,
      .snd_key0 = KR_PassedCap,
      .snd_key1 = KR_Caller,
      .snd_key2 = KR_VOID,
      .snd_rsmkey = KR_VOID,
      .snd_len = 0,
      .snd_code = savedCode,
      .snd_w1 = 0,
      .snd_w2 = 0,
      .snd_w3 = 0,
    };
    SEND(&Msg);
    haveCaller = haveGetter = false;	// reset for next time
  }
}

NORETURN int
main(void)
{
  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    /* The void key is not picky about the other parameters,
    so it's OK to leave them uninitialized. */
    .rcv_key0 = KR_ARG(0),
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_limit = 0,
  };

  for (;;) {
    RETURN(&Msg);

    // Set up defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;

    switch (Msg.rcv_keyInfo) {
    default:
      assert(false);

    case keyInfo_RendezvousCall:
      savedCode = Msg.rcv_code;
      COPY_KEYREG(KR_ARG(0), KR_PassedCap);
      COPY_KEYREG(KR_RETURN, KR_Caller);
      haveCaller = true;
      CheckRendezvous();
      Msg.snd_invKey = KR_VOID;
      break;

    case keyInfo_RendezvousGetCaller:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_RendezvousGetCaller;
        break;

      case OC_capros_RendezvousGetCaller_get:
        COPY_KEYREG(KR_RETURN, KR_Getter);
        haveGetter = true;
        CheckRendezvous();
        Msg.snd_invKey = KR_VOID;
        break;
      }
      break;
    }	// end of switch (keyInfo)
  }	// end of loop forever
}
