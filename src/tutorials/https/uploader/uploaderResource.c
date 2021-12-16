/*
 * Copyright (C) 2009-2010, Strawberry Development Group.
 * All rights reserved.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* 
An HTTPResource that allows PUTting a confined program.
*/

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/machine/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/HTTPResource.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define KR_OSTREAM    KR_APP(0)
#define KR_DemoHTTPC  KR_APP(1)
#define KR_SubBank    KR_APP(2)

#define dbg_init	0x1 

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

int
main(void)
{
  result_t result;
#define bufSize capros_HTTPResource_maxLengthOfPathAndQuery
  uint8_t buf[bufSize];
  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_w3 = 0,
    .snd_len = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_data = buf,
    .rcv_limit = bufSize
  };

  // Init KR_SubBank to Void:
  COPY_KEYREG(KR_VOID, KR_SubBank);

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

    case 2:	// OC_capros_HTTPResource_request
    {
      unsigned int major = Msg.rcv_w1 >> 16;
      assert(major == 1);
      (void)major;

      switch (Msg.rcv_w2) {	// switch on Method
      default:
        Msg.snd_w1 = capros_HTTPResource_RHType_MethodNotAllowed;
        Msg.snd_w2 =   (1UL << capros_HTTPResource_Method_PUT);
        break;

      case capros_HTTPResource_Method_PUT:
        // Ignore path and query.

        // We only run one confined program at once. Zap any previous one.
        result = capros_SpaceBank_destroyBankAndSpace(KR_SubBank);
        (void)result;

        // Make a new limited bank for this instance of the confined program.
        result = capros_SpaceBank_createSubBank(KR_BANK, KR_SubBank);
        assert(result == RC_OK);
#define BANK_LIMIT 100
        result = capros_SpaceBank_setLimit(KR_SubBank, BANK_LIMIT);
        assert(result == RC_OK);
        result = capros_SpaceBank_reduce(KR_SubBank,
                   capros_SpaceBank_precludeSetLimit, KR_TEMP1);
        assert(result == RC_OK);

        result = capros_Constructor_request(KR_DemoHTTPC,
                   KR_TEMP1, KR_SCHED, KR_VOID,
                   KR_TEMP0);
        switch (result) {
        default:
          kprintf(KR_OSTREAM, "DemoHTTPC returned %#x\n", result);
        case RC_capros_SpaceBank_LimitReached:
          Msg.snd_code = result;
          break;

        case RC_OK:
          Msg.snd_w1 = capros_HTTPResource_RHType_HTTPRequestHandler;
          Msg.snd_w2 = 4096;	// by convention with DemoHTTP
          Msg.snd_key0 = KR_TEMP0;
        }
      }
      break;
    }
    }
  }
}
