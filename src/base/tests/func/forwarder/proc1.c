/*
 * Copyright (C) 2007, 2009, Strawberry Development Group
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <eros/StdKeyType.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Discrim.h>
#include <idl/capros/Forwarder.h>
#include <domain/domdbg.h>
#include <idl/capros/arch/arm/SysTrace.h>

#define KR_DISCRIM 8
#define KR_OSTREAM 10
#define KR_ECHO_PROCESS 12
#define KR_FORWARDER 13
#define KR_OP_FORWARDER 14	// opaque
#define KR_ECHO_START0 15
#define KR_ECHO_START1 16
#define KR_SCRATCH 17
#define KR_SCRATCH2 18

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

Message msg;

#define checkEqual(a,b) \
  if ((a) != (b)) \
    kprintf(KR_OSTREAM, "Line %d, expecting %s=%d(0x%x), got %d(0x%x)\n", \
            __LINE__, #a, (b), (b), (a), (a));

int
main()
{
  uint32_t retval;
  bool b;
  uint32_t class;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = 0;		/* no data returned */

  /* Get echo process started. */
  capros_Process_makeResumeKey(KR_ECHO_PROCESS, KR_SCRATCH);
  msg.snd_invKey = KR_SCRATCH;
  msg.snd_code = RC_OK;
  SEND(&msg);

  kprintf(KR_OSTREAM, "Calling forwarder getType.\n");

  capros_key_type type;
  retval = capros_key_getType(KR_FORWARDER, &type);
  checkEqual(retval, RC_OK);
  checkEqual(type, AKT_Forwarder);

  kprintf(KR_OSTREAM, "Calling through forwarder.\n");

  msg.snd_invKey = KR_OP_FORWARDER;
  msg.snd_code = 7;
  msg.snd_w3 = 13;
  CALL(&msg);
  checkEqual(msg.rcv_code, 7);	// code is echoed
  checkEqual(msg.rcv_w1, 0);	// w1 has keyData
  checkEqual(msg.rcv_w3, 13);	// w3 is echoed

  retval = capros_Process_makeStartKey(KR_ECHO_PROCESS, 0, KR_ECHO_START0);
  checkEqual(retval, RC_OK);

  retval = capros_Process_makeStartKey(KR_ECHO_PROCESS, 17, KR_ECHO_START1);
  checkEqual(retval, RC_OK);

  kprintf(KR_OSTREAM, "Calling forwarder.\n");

  // Check swapTarget.
  retval = capros_Forwarder_swapTarget(KR_FORWARDER, KR_ECHO_START1, KR_SCRATCH);
  checkEqual(retval, RC_OK);

  // Should get the original target, which is a start key with keyInfo 0. 
  retval = capros_Discrim_compare(KR_DISCRIM, KR_SCRATCH, KR_ECHO_START0, &b);
  checkEqual(retval, RC_OK);
  checkEqual(b, true);

  // Now get the target key we just stored. 
  retval = capros_Forwarder_getTarget(KR_FORWARDER, KR_SCRATCH);
  checkEqual(retval, RC_OK);

  // Should get the new target, which is a start key with keyInfo 17. 
  retval = capros_Discrim_compare(KR_DISCRIM, KR_SCRATCH, KR_ECHO_START1, &b);
  checkEqual(retval, RC_OK);
  checkEqual(b, true);

  // Call, should see new keyInfo.
  msg.snd_invKey = KR_OP_FORWARDER;
  msg.snd_code = 21;
  msg.snd_w3 = 19;
  CALL(&msg);
  checkEqual(msg.rcv_code, 21);	// code is echoed
  checkEqual(msg.rcv_w1, 17);	// w1 has keyData
  checkEqual(msg.rcv_w3, 19);	// w3 is echoed

  // Check swapSlot.
  retval = capros_Forwarder_swapSlot(KR_FORWARDER, capros_Forwarder_maxSlot+1,
             KR_ECHO_START1, KR_SCRATCH);
  checkEqual(retval, RC_capros_key_RequestError);

  retval = capros_Forwarder_swapSlot(KR_FORWARDER, capros_Forwarder_maxSlot,
             KR_ECHO_START1, KR_SCRATCH);
  checkEqual(retval, RC_OK);

  // Should get void.
  retval = capros_Discrim_classify(KR_DISCRIM, KR_SCRATCH, &class);
  checkEqual(retval, RC_OK);
  checkEqual(class, capros_Discrim_clVoid);

  // Check getSlot.
  retval = capros_Forwarder_getSlot(KR_FORWARDER, capros_Forwarder_maxSlot+1,
             KR_SCRATCH);
  checkEqual(retval, RC_capros_key_RequestError);

  retval = capros_Forwarder_getSlot(KR_FORWARDER, capros_Forwarder_maxSlot,
             KR_SCRATCH);
  checkEqual(retval, RC_OK);

  // Should get the key we stored.
  retval = capros_Discrim_compare(KR_DISCRIM, KR_SCRATCH, KR_ECHO_START1, &b);
  checkEqual(retval, RC_OK);
  checkEqual(b, true);


  // Check sendWord.
  retval = capros_Forwarder_getOpaqueForwarder(KR_FORWARDER,
             capros_Forwarder_sendWord, KR_OP_FORWARDER);

  // Call, should see dataword.
  msg.snd_invKey = KR_OP_FORWARDER;
  msg.snd_code = 21;
  msg.snd_w3 = 19;
  CALL(&msg);
  checkEqual(msg.rcv_code, 21);	// code is echoed
  checkEqual(msg.rcv_w1, 17);	// w1 has keyData
  checkEqual(msg.rcv_w3, 0);	// w3 is dataword
  // Should get void.
  retval = capros_Discrim_classify(KR_DISCRIM, KR_ARG(2), &class);
  checkEqual(retval, RC_OK);
  checkEqual(class, capros_Discrim_clVoid);


  // Check swapDataWord.
#define dwval 0x98765432
  uint32_t dw;
  retval = capros_Forwarder_swapDataWord(KR_FORWARDER, dwval, &dw);
  checkEqual(retval, RC_OK);
  checkEqual(dw, 0);	// old word was zero

  retval = capros_Forwarder_getDataWord(KR_FORWARDER, &dw);
  checkEqual(retval, RC_OK);
  checkEqual(dw, dwval);

  // Call, should see new dataword.
  msg.snd_invKey = KR_OP_FORWARDER;
  msg.snd_code = 21;
  msg.snd_w3 = 19;
  CALL(&msg);
  checkEqual(msg.rcv_code, 21);	// code is echoed
  checkEqual(msg.rcv_w1, 17);	// w1 has keyData
  checkEqual(msg.rcv_w3, dwval);	// w3 is dataword


  // Check sendCap.
  retval = capros_Forwarder_getOpaqueForwarder(KR_FORWARDER,
             capros_Forwarder_sendCap, KR_OP_FORWARDER);

  // Call, should see cap.
  msg.snd_invKey = KR_OP_FORWARDER;
  msg.snd_code = 21;
  msg.snd_w3 = 19;
  CALL(&msg);
  checkEqual(msg.rcv_code, 21);	// code is echoed
  checkEqual(msg.rcv_w1, 17);	// w1 has keyData
  checkEqual(msg.rcv_w3, 19);	// w3 is echoed
  // Should get cap.
  retval = capros_Discrim_compare(KR_DISCRIM, KR_ARG(2), KR_FORWARDER, &b);
  checkEqual(retval, RC_OK);
  checkEqual(b, true);


  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
