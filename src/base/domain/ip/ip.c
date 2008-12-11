/*
 * Copyright (C) 2008, Strawberry Development Group.
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
#include <idl/capros/IP.h>
#include <idl/capros/UDPPortNum.h>
#include <idl/capros/TCPPortNum.h>
#include <idl/capros/NPIP.h>
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
// KR_NPIP is the same slot in both threads, but may not hold the same key.
#define KR_NPIP    KR_APP(1)
// main thread only:
#define KR_NPLinkee KR_APP(2)
// nplinkee thread only:
#define KR_RetMain KR_APP(3)

/*************************** nplinkee thread ******************************/

#define keyInfo_nplinkee 0	// NPLinkee key
#define keyInfo_main     1	// used by main thread

#define OC_getNPIP 99

bool haveNPIP = false;
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
        assert(Msg.rcv_w1 == IKT_capros_NPIP);
        COPY_KEYREG(KR_ARG(0), KR_NPIP);
        haveNPIP = true;
        if (haveMain) {
          Msg.snd_invKey = KR_RetMain;
          Msg.snd_key0 = KR_NPIP;
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

      case OC_getNPIP:
        if (haveNPIP) {		// we think we have it; do we really?
          capros_key_type keyType;
          result = capros_key_getType(KR_NPIP, &keyType);
          if (result != RC_OK)
            haveNPIP = false;	// sure enough, it's gone
        }
        if (haveNPIP) {
          Msg.snd_key0 = KR_NPIP;
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

/* The IP key is a start key to main with keyInfo_IP.
 * A UDPPortNum key is a forwarder with a start key to main with
 *   keyInfo_UDPPortNum, and the port number in the data word.
 * A TCPPortNum key is a forwarder with a start key to main with
 *   keyInfo_TCPPortNum, and the port number in the data word.
 */
#define keyInfo_IP         0
#define keyInfo_UDPPortNum 1
#define keyInfo_TCPPortNum 2

bool
IPVoid(result_t result)
{
  bool isVoid = result == RC_capros_key_Void;
  if (isVoid) {
    // Call nplinkee thread to wait for and get NPIP key.
    Message Msg = {
      .snd_invKey = KR_NPLinkee,
      .snd_key0 = KR_VOID,
      .snd_key1 = KR_VOID,
      .snd_key2 = KR_VOID,
      .snd_rsmkey = KR_VOID,
      .snd_code = OC_getNPIP,
      .snd_len = 0,
      .rcv_key0 = KR_NPIP,
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

NORETURN int
main(void)
{
  result_t result;
  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    /* The void key is not picky about the other parameters,
    so it's OK to leave them uninitialized. */
    .rcv_key0 = KR_VOID,
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
    Msg.snd_key0 = KR_VOID;

    switch (Msg.rcv_keyInfo) {
    default:
      assert(false);

    case keyInfo_IP:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_IP;
        break;

      case OC_capros_IP_createUDPPort:
        do {
          result = capros_NPIP_createUDPPort(KR_NPIP,
                     capros_NPIP_LocalPortAny, KR_TEMP0);
        } while (IPVoid(result));
        Msg.snd_key0 = KR_TEMP0;
        Msg.snd_code = result;
        break;

      case OC_capros_IP_TCPConnect:
        do {
          result = capros_NPIP_connect(KR_NPIP, Msg.rcv_w1, Msg.rcv_w2,
                                       KR_TEMP0);
        } while (IPVoid(result));
        Msg.snd_key0 = KR_TEMP0;
        Msg.snd_code = result;
        break;
      }
      break;

    case keyInfo_UDPPortNum:
    {
      unsigned int localPort = Msg.rcv_w3;	// data word from forwarder
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_UDPPortNum;
        break;

      case OC_capros_UDPPortNum_createUDPPort:
        do {
          result = capros_NPIP_createUDPPort(KR_NPIP, localPort, KR_TEMP0);
        } while (IPVoid(result));
        Msg.snd_key0 = KR_TEMP0;
        Msg.snd_code = result;
        break;
      }
      break;
    }

    case keyInfo_TCPPortNum:
    {
      unsigned int localPort = Msg.rcv_w3;	// data word from forwarder
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_TCPPortNum;
        break;

      case OC_capros_TCPPortNum_listen:
        do {
          result = capros_NPIP_listen(KR_NPIP, localPort, KR_TEMP0);
        } while (IPVoid(result));
        Msg.snd_key0 = KR_TEMP0;
        Msg.snd_code = result;
        break;
      }
      break;
    }
    }	// end of switch (keyInfo)
  }	// end of loop forever
}
