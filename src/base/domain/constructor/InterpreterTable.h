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

/* Macros to help build a table of Message structures
for the interpreter program.

The array must end with a message that transfers control, usually
swapAddrSpaceAndPC32. */

#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Node.h>
#include <idl/capros/Memory.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Range.h>
#include <idl/capros/Process.h>
#include <idl/capros/Constructor.h>

/* Use this like this:
struct Message InterpreterTable[] = {
  MessageStruct(...)
  MessageStruct(...)
};
*/

#define MessageStruct(s_invKey, s_code, s_w1, s_w2, s_w3, \
	s_key0, s_key1, s_key2, \
	s_len, s_data, \
	r_key0, r_key1, r_key2, r_rsmkey) \
{ \
  .snd_invKey = s_invKey, \
  .invType = IT_Call, \
  .snd_code = s_code, \
  .snd_w1 = s_w1, \
  .snd_w2 = s_w2, \
  .snd_w3 = s_w3, \
  .snd_key0 = s_key0, \
  .snd_key1 = s_key1, \
  .snd_key2 = s_key2, \
  .snd_rsmkey = KR_VOID, /* not used because IT_Call */ \
  .snd_len = s_len, \
  .snd_data = s_data, \
  .rcv_key0 = r_key0, \
  .rcv_key1 = r_key1, \
  .rcv_key2 = r_key2, \
  .rcv_rsmkey = r_rsmkey, \
  .rcv_limit = 0	/* cannot receive data because no writeable memory */ \
}

/* Simplified macros: */

#define MsgPw1Pk2Rk2(s_invKey, s_code, s_w1, s_key0, s_key1, r_key0, r_key1) \
  MessageStruct(s_invKey, s_code, s_w1, 0, 0, \
	s_key0, s_key1, KR_VOID, \
	0, 0, \
	r_key0, r_key1, KR_VOID, KR_VOID)

#define MsgPkRk(s_invKey, s_code, s_key0, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, s_code, 0, s_key0, KR_VOID, r_key0, KR_VOID)

#define MsgAlloc3(s_invKey, t0, t1, t2, r_key0, r_key1, r_key2) \
   MessageStruct(s_invKey, OC_capros_SpaceBank_alloc3, \
    capros_Range_ot##t0 + (capros_Range_ot##t1<<8) \
      + (capros_Range_ot##t2<<16), \
    0, 0, \
    KR_VOID, KR_VOID, KR_VOID, \
    0, 0, \
    r_key0, r_key1, r_key2, KR_VOID)

#define MsgSetL2v(s_invKey, l2v) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_GPT_setL2v, l2v, \
    KR_VOID, KR_VOID, KR_VOID, KR_VOID)

#define MsgMakeGuarded(s_invKey, guard, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_Memory_makeGuarded, guard, \
    KR_VOID, KR_VOID, r_key0, KR_VOID)

#define MsgGPTGetSlot(s_invKey, slot, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_GPT_getSlot, slot, \
    KR_VOID, KR_VOID, r_key0, KR_VOID)

#define MsgGPTSetSlot(s_invKey, slot, s_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_GPT_setSlot, slot, \
    s_key0, KR_VOID, KR_VOID, KR_VOID)

#define MsgNodeGetSlotExtended(s_invKey, slot, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_Node_getSlotExtended, slot, \
    KR_VOID, KR_VOID, r_key0, KR_VOID)

#define MsgNewSpace(s_key0, pc) \
  MsgPw1Pk2Rk2(KR_SELF, OC_capros_Process_swapAddrSpaceAndPC32, pc, \
    s_key0, KR_VOID, KR_VOID, KR_VOID)

#define MsgNewVCSK(s_invKey, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_Constructor_request, 0, \
    KR_BANK, KR_SCHED, r_key0, KR_VOID)
