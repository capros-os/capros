/*
 * Copyright (C) 2006, 2007, Strawberry Development Group
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/Invocation.h>
#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <kerninc/KernStats.h>
#include <eros/Invoke.h>
//#include <kerninc/IRQ.h>
#include <arch-kerninc/Process-inline.h>
#include <arch-kerninc/IRQ-inline.h>

#define dbg_init	0x1u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init)

#define DEBUG(x) if (dbg_##x & dbg_flags)

void 
proc_DeliverResult(Process * thisPtr, Invocation * inv /*@ not null @*/)
{
  assert(proc_IsRunnable(thisPtr));

  assert (inv->invKeyType > KKT_Resume);
  
  /* copy return code and words */
  thisPtr->trapFrame.r1 = inv->exit.code;
  thisPtr->trapFrame.r2 = inv->exit.w1;
  thisPtr->trapFrame.r3 = inv->exit.w2;
  thisPtr->trapFrame.r4 = inv->exit.w3;
  thisPtr->trapFrame.r12 = 0;		/* key info field */

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.r14 = inv->sentLen;

  /* If the recipient specified an invalid receive area, though, they
   * are gonna get FC_ParmLack:
   */
  if (inv->validLen < inv->exit.len) {
#if 1
printf("Unimplemented ParmLack\n");
#else
    uint32_t rcvPtr = thisPtr->trapFrame.EDI;
    proc_SetFault(thisPtr, FC_ParmLack, rcvPtr + inv->validLen, false);
#endif
  }
}

/* May Yield. */
void 
proc_SetupEntryBlock(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  uint8_t *sndKeys;
  uva_t const entryMessage = thisPtr->trapFrame.r0;
  uint32_t const invKeyAndType = thisPtr->trapFrame.r1;
  
  /* Not hazarded because invocation key */
  inv->key = &thisPtr->keyReg[invKeyAndType & 0xff];
  inv->invType = (invKeyAndType >> 8) & 0xff;

  key_Prepare(inv->key);
#ifndef invKeyType
  inv->invKeyType = keyBits_GetType(inv->key);
#endif

  inv->entry.code = thisPtr->trapFrame.r4;
/* We may be able to eliminate the copy of w1, w2, and w3 when invoking
   kernel keys that don't use those parameters. 
   For now, just copy them. */
  inv->entry.w1 = thisPtr->trapFrame.r5;
  /* Current process's address map is still loaded. */
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w2),
                              &inv->entry.w2))
    fatal("Error loading w1-w3");
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w3),
                              &inv->entry.w3))
    fatal("Error loading w1-w3");

  sndKeys = (uint8_t *) &thisPtr->trapFrame.r2;
  
  /* Not hazarded because invocation key */
  inv->entry.key[0] = &thisPtr->keyReg[sndKeys[0]];
  inv->entry.key[1] = &thisPtr->keyReg[sndKeys[1]];
  inv->entry.key[2] = &thisPtr->keyReg[sndKeys[2]];
  inv->entry.key[3] = &thisPtr->keyReg[sndKeys[3]];

  inv->entry.len = thisPtr->trapFrame.r3;
  inv->sentLen = 0;		/* set in CopyOut */
}

/* NOTE that this can be called with /thisPtr/ == 0, and must guard
   against that possibility! */
void 
proc_SetupExitBlock(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  uint8_t *rcvKeys;
  /* NOTE THAT THIS PROCEEDS EVEN IF THE EXIT BLOCK IS INVALID!!! */
  
  /* inv.exit.code may be overwritten by the actual key handler: */
  inv->exit.code = RC_OK;
  inv->exit.w1 = 0;
  inv->exit.w2 = 0;
  inv->exit.w3 = 0;
  inv->exit.len = 0;		/* setting this twice cheaper than branch */
  inv->validLen = 0;		/* until proven otherwise. */

  inv->exit.pKey[0] = 0;
  inv->exit.pKey[1] = 0;
  inv->exit.pKey[2] = 0;
  inv->exit.pKey[3] = 0;

  if (thisPtr == 0) {
    inv->suppressXfer = true;
    return;
  }

  /* /thisPtr/ is valid. Decode the exit block. */
  /* Note, we can't easily get the exit block from the Message structure,
  because that process's address map isn't loaded now.
  Fortunately we have stashed the values in registers. */

  rcvKeys = (uint8_t *) &thisPtr->trapFrame.r14;

  if (thisPtr->trapFrame.r14) {		/* if any nonzero */
    if (thisPtr->trapFrame.r14 & 0xe0e0e0e0) {	/* if any too large */
      proc_SetFault(thisPtr, FC_BadExitBlock, 0, false);
      inv->suppressXfer = true;
      inv->invokee->processFlags &= ~PF_ExpectingMsg;
      return;
    }
    else {
      if (rcvKeys[0])
	inv->exit.pKey[0] = &thisPtr->keyReg[rcvKeys[0]];
      if (rcvKeys[1])
	inv->exit.pKey[1] = &thisPtr->keyReg[rcvKeys[1]];
      if (rcvKeys[2])
	inv->exit.pKey[2] = &thisPtr->keyReg[rcvKeys[2]];
      if (rcvKeys[3])
	inv->exit.pKey[3] = &thisPtr->keyReg[rcvKeys[3]];
    }
  }
  
  inv->validLen = thisPtr->trapFrame.r12;	/* rcv_limit */

  /* Should this set a fault code? */
  if (inv->validLen > EROS_MESSAGE_LIMIT)
    inv->validLen = EROS_MESSAGE_LIMIT;

  assert( proc_IsRunnable(thisPtr) );
}

void 
proc_DeliverGateResult(Process* thisPtr,
                       Invocation* inv /*@ not null @*/, bool wantFault)
{
  /* No need to call Prepare() here -- it has already been  called in
   * the invocation path.
   */

  /* There used to be a check of IsRunnable() here.  I removed it
   * because if the recipient had a bad string a fault code was
   * generated in SetupExitString, and as a consequence the recipient
   * is no longer runnable.  It is guaranteed, however, that such a
   * short string will not impede progress.
   */
  
  uint16_t keyData = inv->key->keyData;
  
  if (thisPtr->trapFrame.r14) {		/* rcv_keys */
    if (thisPtr->trapFrame.r14 & 0x1f1f1fu) {
      if (inv->exit.pKey[0])
        key_NH_Set(inv->exit.pKey[0], inv->entry.key[0]);
      if (inv->exit.pKey[1])
	key_NH_Set(inv->exit.pKey[1], inv->entry.key[1]);
      if (inv->exit.pKey[2])
	key_NH_Set(inv->exit.pKey[2], inv->entry.key[2]);
    }
    
    if (inv->exit.pKey[RESUME_SLOT]) {
      if (inv->invType == IT_Call) {
	proc_BuildResumeKey(act_CurContext(), inv->exit.pKey[RESUME_SLOT]);
	if (wantFault)
	  inv->exit.pKey[RESUME_SLOT]->keyPerms = KPRM_FAULT;
      }
      else
	key_NH_Set(inv->exit.pKey[RESUME_SLOT], inv->entry.key[RESUME_SLOT]);
    }
  }

  /* copy return code and words */
  thisPtr->trapFrame.r1 = inv->entry.code;
  thisPtr->trapFrame.r2 = inv->entry.w1;
  thisPtr->trapFrame.r3 = inv->entry.w2;
  thisPtr->trapFrame.r4 = inv->entry.w3;

#ifdef OPTION_DDB
  /* Duplicate here so that debugger reporting is accurate. */
  inv->exit.code = inv->entry.code;
  inv->exit.w1 = inv->entry.w1;
  inv->exit.w2 = inv->entry.w2;
  inv->exit.w3 = inv->entry.w3;
#endif
  
  thisPtr->trapFrame.r12 = keyData;

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.r14 = inv->sentLen;

  /* If the recipient specified an invalid receive area, though, they
   * are gonna get FC_ParmLack:
   */

  /* BUG? This seems wrong, since exit.len is set to no more than validLen
     in inv_CopyOut. */
  if (inv->validLen < inv->exit.len) {
#if 1
    fatal("Setting FC_ParmLack");
#else
    uint32_t rcvBase;
    rcvBase = thisPtr->trapFrame.EDI;

    proc_SetFault(thisPtr, FC_ParmLack, rcvBase + inv->validLen, false);
#endif
  }


  /* Make sure it isn't later overwritten by the general delivery
   * mechanism.
   */
  inv->suppressXfer = true;
}

void /* does not return */
InvokeArm(Process * invokerProc,
          uint32_t typeAndKey,	/* invoked key in low byte, type in next */
          uint32_t snd_keys,
          uint32_t snd_len)
{
  Process * invokee;

#if 0
  printf("Inv p=%x, type.key %x, oc 0x%x, psr=%x, pc=0x%08x, r0=%x, sp=0x%08x\n",
         invokerProc, typeAndKey, invokerProc->trapFrame.r4,
         invokerProc->trapFrame.CPSR, invokerProc->trapFrame.r15,
         invokerProc->trapFrame.r0, invokerProc->trapFrame.r13);
#endif

#if 1
  goto general_path1;	/* bypass fast path */
#endif

  /* Instead of validating parameters, just ignore any cruft. It's faster. */
#define InvTypeMask 0x7	/* must be >= IT_NUM_INVTYPES */
  typeAndKey &= (InvTypeMask << 8) | (EROS_NODE_SIZE -1);
  snd_keys &= (EROS_NODE_SIZE -1)
              | ((EROS_NODE_SIZE -1) << 8)
              | ((EROS_NODE_SIZE -1) << 16)
              | ((EROS_NODE_SIZE -1) << 24);

  /* See if we can use a fast path. */
  if (snd_len) goto general_path0;	/* no strings */

  unsigned int invokerFinalState;
  { unsigned int type = typeAndKey >> 8;
    if (type < IT_Call) invokerFinalState = RS_Available; /* return */
    else if (type == IT_Call) invokerFinalState = RS_Waiting; /* call */
    else goto general_path1; /* only call, return */
  }
  //// no one blocked on stall queue? interrupt.S line 1051
  Key * invKey = &invokerProc->keyReg[typeAndKey & (EROS_NODE_SIZE -1)];
  /* Assuming it is a Start or Resume key, it must be prepared. */
  if (!(invKey->keyFlags & KFL_PREPARED)) goto general_path1;
  switch (invKey->keyType) {
  case KKT_Start:
    invokee = invKey->u.gk.pContext;
    if (invokee->runState != RS_Available) goto general_path1;
    break;
  case KKT_Resume:
    invokee = invKey->u.gk.pContext;
    /* Since a resume key exists, invokee must be in the waiting state. */
    break;
  default: goto general_path1;
  }

  if (invokee->processFlags & PF_Faulted) goto general_path1;
  /* Do we need to check invokee has an address space? */

  invokee->trapFrame.r12 = invKey->keyData;

  if (invokee->trapFrame.r14) {	/* invokee is receiving some keys */
    unsigned int rcvKeyNdx = invokee->trapFrame.r14 & (EROS_NODE_SIZE -1);
    unsigned int sndKeyNdx;
    if (rcvKeyNdx != 0) {	/* send key 0 */
      Key * rcvKey0 = &invokee->keyReg[rcvKeyNdx];
#if RESUME_SLOT == 0
      if (invokerFinalState != RS_Available) { /* a call */
        proc_BuildResumeKey(invokerProc, rcvKey0);
      } else /* a return */
#endif
      {
        sndKeyNdx = invokerProc->trapFrame.r2 & (EROS_NODE_SIZE -1);
        /* key_NH_Set handles some cases that don't occur here:
           src == dst
           src has hazard bits */
        key_NH_Set(rcvKey0, &invokerProc->keyReg[sndKeyNdx]);
      }
    }
    rcvKeyNdx = (invokee->trapFrame.r14 >> 8) & (EROS_NODE_SIZE -1);
    if (rcvKeyNdx != 0) {	/* send key 1 */
      sndKeyNdx = (invokerProc->trapFrame.r2 >> 8) & (EROS_NODE_SIZE -1);
      key_NH_Set(&invokee->keyReg[rcvKeyNdx],
                 &invokerProc->keyReg[sndKeyNdx]);
    }
    rcvKeyNdx = (invokee->trapFrame.r14 >> 16) & (EROS_NODE_SIZE -1);
    if (rcvKeyNdx != 0) {	/* send key 2 */
      sndKeyNdx = (invokerProc->trapFrame.r2 >> 16) & (EROS_NODE_SIZE -1);
      key_NH_Set(&invokee->keyReg[rcvKeyNdx],
                 &invokerProc->keyReg[sndKeyNdx]);
    }
    rcvKeyNdx = (invokee->trapFrame.r14 >> 24) & (EROS_NODE_SIZE -1);
    if (rcvKeyNdx != 0) {	/* send key 3 */
      Key * rcvKey0 = &invokee->keyReg[rcvKeyNdx];
#if RESUME_SLOT == 3
      if (invokerFinalState != RS_Available) { /* a call */
        proc_BuildResumeKey(invokerProc, rcvKey0);
      } else /* a return */
#endif
      {
        sndKeyNdx = (invokerProc->trapFrame.r2 >> 24) & (EROS_NODE_SIZE -1);
        key_NH_Set(rcvKey0, &invokerProc->keyReg[sndKeyNdx]);
      }
    }
  }

  /* Move data registers. */
  invokee->trapFrame.r1 = invokerProc->trapFrame.r4;	/* snd_code */
  /* Current process's address map is still loaded. */
  uva_t const entryMessage = invokerProc->trapFrame.r0;
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w1),
                              &invokee->trapFrame.r2))
    fatal("Error loading w1-w3");	/* do we need to handle this? */
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w2),
                              &invokee->trapFrame.r3))
    fatal("Error loading w1-w3");
  if (! LoadWordFromUserSpace(entryMessage + offsetof(Message, snd_w3),
                              &invokee->trapFrame.r4))
    fatal("Error loading w1-w3");

  /* Set returned length to zero. */
  invokee->trapFrame.r14 = 0;
  /* Migrate the Activity. */
  Activity * act = invokerProc->curActivity;
  invokee->curActivity = act;
  invokerProc->curActivity = 0;
  act->context = invokee;
  act->readyQ = invokee->readyQ;

  /* Leave invokee's PC advanced. */
  proc_AdjustInvocationPC(invokerProc);	/* back up invoker PC */

  invokerProc->runState = invokerFinalState;
  /* keyR_ZapResumeKeys is only needed if the invoked key was KKT_Resume.
     If it was KKT_Start, there should already be no resume keys. */
  keyR_ZapResumeKeys(&invokee->keyRing);

  /* FIX:  Gotta wake everybody on my own stall queue here! */

  invokee->runState = RS_Running;

  act_Reschedule();	/* Correct? */
  proc_Resume();	/* does not return */
  assert(false);

general_path0:
  if (snd_len > EROS_MESSAGE_LIMIT) goto badInvocation;
general_path1:
  
  assert(irq_DisableDepth == 0);
  irq_DisableDepth = 1;	/* disabled by the exception */

  /* Enable IRQ interrupts. */
  irq_ENABLE();

  assert(act_CurContext()->faultCode == FC_NoFault);

  proc_DoKeyInvocation(act_CurContext());

  assert( irq_DISABLE_DEPTH() == 0 );
  irq_DISABLE();

  act_Reschedule();
  proc_Resume();	/* does not return */

return;

badInvocation:
  fatal("snd_len too big.\n");	// FIXME
}
