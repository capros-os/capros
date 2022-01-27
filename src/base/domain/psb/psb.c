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

/* This object is a persistent interface to the non-persistent
 * space bank. */

#include <string.h>
#include <eros/Invoke.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/NPLinkee.h>
#include <domain/Runtime.h>
#include <domain/assert.h>
#include <eros/machine/cap-instr.h>

/* Bypass all the usual initialization. */
unsigned long __rt_runtime_hook = 0;

#define dbg_server 1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

// KR_OSTREAM is present in both threads.
#define KR_OSTREAM KR_APP(0)
// KR_NPSB is the same slot in both threads, but may not hold the same key.
#define KR_NPSB    KR_APP(1)
// nplinkee thread only:
#define KR_RetMain KR_APP(2)
// main thread only:
#define KR_NPLinkee KR_APP(3)
#define KR_RARG0    KR_APP(4)
#define KR_RARG1    KR_APP(5)
#define KR_RARG2    KR_APP(6)
#define KR_RRETURN  KR_APP(7)

#define min(a,b) ((a) < (b) ? (a) : (b))

/*************************** nplinkee thread ******************************/

#define keyInfo_nplinkee 0	// NPLinkee key
#define keyInfo_main     1	// used by main thread

#define OC_getNPSB 99

bool haveNPSB = false;
bool haveMain = false;

void
nplinkee_main(void)
{
  result_t result;
  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .rcv_key0 = KR_ARG(0),
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_limit = 0,
  };

  for (;;) {
    RETURN(&Msg);

    // Set defaults for the reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;

    switch (Msg.rcv_keyInfo) {
    default:
      assert(false);

    case keyInfo_nplinkee:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_NPLinkee;
        break;

      case OC_capros_NPLinkee_registerNPCap:
        DEBUG(server) kdprintf(KR_OSTREAM, "psb got npsb\n");
        assert(Msg.rcv_w1 == IKT_capros_SpaceBank);
        COPY_KEYREG(KR_ARG(0), KR_NPSB);
        haveNPSB = true;
        if (haveMain) {
          Msg.snd_invKey = KR_RetMain;
          Msg.snd_key0 = KR_NPSB;
          PSEND(&Msg);
          haveMain = false;
          Msg.snd_invKey = KR_RETURN;
          Msg.snd_key0 = KR_VOID;
        }
        break;
      }
      break;

    case keyInfo_main:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_getNPSB:
        if (haveNPSB) {		// we think we have it; do we really?
          capros_key_type keyType;
          result = capros_key_getType(KR_NPSB, &keyType);
          if (result != RC_OK)
            haveNPSB = false;	// sure enough, it's gone
        }
        if (haveNPSB) {
          Msg.snd_key0 = KR_NPSB;
        } else {
          COPY_KEYREG(KR_RETURN, KR_RetMain);	// save
          haveMain = true;
          Msg.snd_invKey = KR_VOID;
        }
        break;
      }
      break;
    }
  }
}

/*************************** main thread ******************************/

/* The persistent SpaceBank key is a start key to main. keyInfo is not used.
 */

bool
SBVoid(result_t result)
{
  bool isVoid = result == RC_capros_key_Void;
  if (isVoid) {
    // Call nplinkee thread to wait for and get NPSB key.
    Message Msg = {
      .snd_invKey = KR_NPLinkee,
      .snd_key0 = KR_VOID,
      .snd_key1 = KR_VOID,
      .snd_key2 = KR_VOID,
      .snd_rsmkey = KR_VOID,
      .snd_code = OC_getNPSB,
      .snd_len = 0,
      .rcv_key0 = KR_NPSB,
      .rcv_key1 = KR_VOID,
      .rcv_key2 = KR_VOID,
      .rcv_rsmkey = KR_VOID,
      .rcv_limit = 0,
    };
    CALL(&Msg);
    assert(Msg.rcv_code == RC_OK);
  }
  return isVoid;
}

int
main(void)
{
  char buff[sizeof(capros_SpaceBank_limits) + 2];

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
    .rcv_key1 = KR_ARG(1),
    .rcv_key2 = KR_ARG(2),
    .rcv_rsmkey = KR_RETURN,
    .rcv_limit = sizeof(buff),
    .rcv_data = buff
  };

  for (;;) {
    RETURN(&Msg);

    // Send the message to the non-persistent key unchanged.
    Message Msg2 = {
      .snd_invKey = KR_NPSB,
      .snd_key0 = KR_ARG(0),
      .snd_key1 = KR_ARG(1),
      .snd_key2 = KR_ARG(2),
      .snd_len = min(Msg.rcv_limit, Msg.rcv_sent),
      .snd_data = buff,
      .snd_code = Msg.rcv_code,
      .snd_w1 = Msg.rcv_w1,
      .snd_w2 = Msg.rcv_w2,
      .snd_w3 = Msg.rcv_w3,
      // Receive keys without clobbering KR_ARG(n),
      // in case operation must be retried.
      .rcv_key0 = KR_RARG0,
      .rcv_key1 = KR_RARG1,
      .rcv_key2 = KR_RARG2,
      .rcv_rsmkey = KR_RRETURN,
      // A Void key does not return a string, so reusing buff is OK.
      .rcv_limit = sizeof(buff),
      .rcv_data = buff
    };
    do {
      DEBUG(server) kdprintf(KR_OSTREAM, "psb: forwarding message %#x as %#x\n", &Msg, &Msg2);
      CALL(&Msg2);
      DEBUG(server) kprintf(KR_OSTREAM, "psb: rc=%#x\n", Msg2.rcv_code);
    } while (SBVoid(Msg2.rcv_code));

    // Set up reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = Msg2.rcv_code;
    Msg.snd_w1 = Msg2.rcv_w1;
    Msg.snd_w2 = Msg2.rcv_w2;
    Msg.snd_w3 = Msg2.rcv_w3;
    Msg.snd_key0 = KR_RARG0;
    Msg.snd_key1 = KR_RARG1;
    Msg.snd_key2 = KR_RARG2;
    Msg.snd_len = min(Msg2.rcv_limit, Msg2.rcv_sent);
    Msg.snd_data = buff;
  }	// end of loop forever
}
