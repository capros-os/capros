/*
 * Copyright (C) 2009-2011, Strawberry Development Group.
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

/* 
An HTTPResource that allows only the POST method.
*/

#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Number.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/HTTPResource.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpaceDS.h>

#define KC_OSTREAM 0
#define KC_PROTOSPC 1

#define KR_OSTREAM    KR_APP(0)
#define KR_HTTPRHC    KR_APP(1)

int
main(void)
{
  result_t result;
  uint32_t sendLimit;
#define bufSize capros_HTTPResource_maxLengthOfPathAndQuery
  uint8_t buf[bufSize];

  capros_Node_getSlotExtended(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  Message Msg = {
    .snd_invKey = KR_RETURN,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_code = RC_OK,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_len = 0,
    .rcv_key0 = KR_HTTPRHC,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_data = buf,
    .rcv_limit = bufSize
  };

  // Call back to our caller to get KR_HTTPRHC and sendLimit.
  result = CALL(&Msg);
  sendLimit = Msg.rcv_w1;

  capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  Msg.snd_key0 = KR_TEMP0;
  Msg.rcv_key0 = KR_VOID;

  while (1) {
    RETURN(&Msg);

    // Set defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_key0 = KR_VOID;

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_HTTPResource;
      break;

    case OC_capros_key_destroy:
      capros_Node_getSlotExtended(KR_CONSTIT, KC_PROTOSPC, KR_TEMP0);
      protospace_destroy_small(KR_TEMP0, 0 /* return code */);
      // protospace_destroy_small does not return.

    case 2:	// OC_capros_HTTPResource_request
    {
      unsigned int major = Msg.rcv_w1 >> 16;
      assert(major == 1);
      (void)major;

      switch (Msg.rcv_w2) {	// switch on Method
      default:
        Msg.snd_w1 = capros_HTTPResource_RHType_MethodNotAllowed;
        Msg.snd_w2 =    (1UL << capros_HTTPResource_Method_POST);
        break;

      case capros_HTTPResource_Method_POST:
        // Ignore path and query.

        // Construct the HTTPResourceHandler
        result = capros_Constructor_request(KR_HTTPRHC,
                   KR_BANK, KR_SCHED, KR_VOID,
                   KR_TEMP0);
        switch (result) {
        default:
          kprintf(KR_OSTREAM, "HTTPRGet: HTTPRHC returned %#x\n", result);
        case RC_capros_SpaceBank_LimitReached:
          Msg.snd_code = result;
          break;

        case RC_OK:
          Msg.snd_w1 = capros_HTTPResource_RHType_HTTPRequestHandler;
          Msg.snd_w2 = sendLimit;
          Msg.snd_key0 = KR_TEMP0;
        }
      }
      break;
    }
    }
  }
}
