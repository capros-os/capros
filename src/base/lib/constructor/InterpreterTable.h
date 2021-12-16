/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
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
#include <idl/capros/ProcCre.h>
#include <idl/capros/Constructor.h>

/* Use like this:
struct InterpreterStep ConstructionTable[] = {
  StepStruct(...)
  StepStruct(...)
  MsgNewSpace(...)
};
struct InterpreterStep DestructionTable[] = {
  StepStruct(...)
  StepStruct(...)
  MsgDestroyProcess(KR_CREATOR, KR_BANK, KR_RETURN)
};
*/

struct InterpreterStep {
  struct Message message;

  /* If this message gets a result other than RC_OK, then:
     If errorResult is faultOnError, the process faults.
     Otherwise, we will go to the DestructionTable
       at the offset in destructionOffset.
       That offset must be a multiple of sizeof(struct InterpreterStep).
       If errorResult is passErrorThrough,
         the error result from this message is copied to the holding cell,
       otherwise the value in errorResult is copied to the holding cell.
   */
#define passErrorThrough 0
#define faultOnError 1
  result_t errorResult;

  /* If destructionOffset is getHolding, errorResult must be faultOnError,
     and the holding cell is passed in snd_w1.
     If destructionOffset is setHolding, errorResult must be faultOnError,
     and if the message gets a result of RC_OK,
     the value of rcv_w1 is copied to the holding cell.
     Otherwise,
       destructionOffset is the index in DestructionTable to go to on error. */
#define getHolding (-1)
#define setHolding (-2)
  int32_t destructionOffset;
};

#define StepStruct(s_invKey, s_code, s_w1, s_w2, s_w3, \
	s_key0, s_key1, s_key2, \
	s_len, s_data, \
	r_key0, r_key1, r_key2, r_rsmkey, \
	err_res, destr_offs) \
{ \
  .message = { \
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
  }, \
  .errorResult = err_res, \
  .destructionOffset = destr_offs \
}

/* Simplified macros: */

#define MsgPw1Pk2Rk2(s_invKey, s_code, s_w1, s_key0, s_key1, r_key0, r_key1, \
	err_res, destr_step) \
  StepStruct(s_invKey, s_code, s_w1, 0, 0, \
	s_key0, s_key1, KR_VOID, \
	0, 0, \
	r_key0, r_key1, KR_VOID, KR_VOID, \
	err_res, destr_step * sizeof(struct InterpreterStep))

#define MsgPkRk(s_invKey, s_code, s_key0, r_key0, err_res, destr_step) \
  MsgPw1Pk2Rk2(s_invKey, s_code, 0, s_key0, KR_VOID, r_key0, KR_VOID, \
	err_res, destr_step)

// Convenience macros for construction:

#define MsgAlloc1(s_invKey, t0, r_key0, err_res, destr_step) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_SpaceBank_alloc1, \
    capros_Range_ot##t0, \
    KR_VOID, KR_VOID, r_key0, KR_VOID, err_res, destr_step)

#define MsgAlloc2(s_invKey, t0, t1, r_key0, r_key1, err_res, destr_step) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_SpaceBank_alloc2,  \
    capros_Range_ot##t0 + (capros_Range_ot##t1<<8), \
    KR_VOID, KR_VOID, r_key0, r_key1, err_res, destr_step)

#define MsgAlloc3(s_invKey, t0, t1, t2, r_key0, r_key1, r_key2, \
	err_res, destr_step) \
   StepStruct(s_invKey, OC_capros_SpaceBank_alloc3, \
    capros_Range_ot##t0 + (capros_Range_ot##t1<<8) \
      + (capros_Range_ot##t2<<16), \
    0, 0, \
    KR_VOID, KR_VOID, KR_VOID, \
    0, 0, \
    r_key0, r_key1, r_key2, KR_VOID, \
	err_res, destr_step * sizeof(struct InterpreterStep))

#define MsgClonePage(s_invKey, fromPage) \
  MsgPkRk(s_invKey, OC_capros_Page_clone, fromPage, KR_VOID, faultOnError, 0)

#define MsgSetL2v(s_invKey, l2v) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_GPT_setL2v, l2v, \
    KR_VOID, KR_VOID, KR_VOID, KR_VOID, faultOnError, 0)

#define MsgMakeGuarded(s_invKey, guard, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_Memory_makeGuarded, guard, \
    KR_VOID, KR_VOID, r_key0, KR_VOID, faultOnError, 0)

#define MsgGPTGetSlot(s_invKey, slot, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_GPT_getSlot, slot, \
    KR_VOID, KR_VOID, r_key0, KR_VOID, faultOnError, 0)

#define MsgGPTSetSlot(s_invKey, slot, s_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_GPT_setSlot, slot, \
    s_key0, KR_VOID, KR_VOID, KR_VOID, faultOnError, 0)

#define MsgNodeGetSlot(s_invKey, slot, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_Node_getSlot, slot, \
    KR_VOID, KR_VOID, r_key0, KR_VOID, faultOnError, 0)

#define MsgNodeGetSlotExtended(s_invKey, slot, r_key0) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_Node_getSlotExtended, slot, \
    KR_VOID, KR_VOID, r_key0, KR_VOID, faultOnError, 0)

#define MsgGetNumber(s_invKey) \
  StepStruct(s_invKey, OC_capros_Number_get, 0, 0, 0, \
	KR_VOID, KR_VOID, KR_VOID, \
	0, 0, \
	KR_VOID, KR_VOID, KR_VOID, KR_VOID, \
	faultOnError, setHolding)

// Set s_key0 as the address space, and set pc from the holding cell.
#define MsgNewSpace(s_key0) \
  StepStruct(KR_SELF, OC_capros_Process_swapAddrSpaceAndPC32, 0, 0, 0, \
	s_key0, KR_VOID, KR_VOID, \
	0, 0, \
	KR_VOID, KR_VOID, KR_VOID, KR_VOID, \
	faultOnError, getHolding)

#define MsgNewVCSK(s_invKey, p_key0, p_key1, r_key0, err_res, destr_step) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_Constructor_request, 0, \
    p_key0, p_key1, r_key0, KR_VOID, err_res, destr_step)

// And for destruction:

#define MsgFree1(s_invKey, s_key0) \
  MsgPkRk(s_invKey, OC_capros_SpaceBank_free1, s_key0, KR_VOID, \
    faultOnError, 0)

#define MsgFree2(s_invKey, s_key0, s_key1) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_SpaceBank_free2, 0, s_key0, s_key1, \
    KR_VOID, KR_VOID, faultOnError, 0)

#define MsgFree3(s_invKey, s_key0, s_key1, s_key2) \
  StepStruct(s_invKey, OC_capros_SpaceBank_free3, \
    0, 0, 0, \
    s_key0, s_key1, s_key2, \
    0, 0, \
    KR_VOID, KR_VOID, KR_VOID, KR_VOID, \
    faultOnError, 0)

#define MsgDestroy(s_invKey) \
  MsgPkRk(s_invKey, OC_capros_key_destroy, KR_VOID, KR_VOID, faultOnError, 0)

#define MsgDestroyProcess(s_invKey, s_key0, s_key1) \
  MsgPw1Pk2Rk2(s_invKey, OC_capros_ProcCre_destroyCallerAndReturn, 0, \
    s_key0, s_key1, KR_VOID, KR_VOID, faultOnError, \
    -1 /* special value of destr_step */ )

