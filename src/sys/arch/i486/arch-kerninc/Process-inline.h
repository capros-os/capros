#ifndef __MACHINE_PROCESS_INLINE_H__
#define __MACHINE_PROCESS_INLINE_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
 *
 * This file is part of the EROS Operating System.
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

#include <eros/Invoke.h>
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/Process.h>

/* Machine-specific inline helper functions for process operations: */

INLINE void 
proc_SetupEntryBlock(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  uint8_t *sndKeys = 0;
  uint32_t len = 0;
  
  /* Not hazarded because invocation key */
  inv->key = &thisPtr->keyReg[thisPtr->pseudoRegs.invKey];

  key_Prepare(inv->key);
#ifndef invKeyType
  inv->invKeyType = keyBits_GetType(inv->key);
#endif

  inv->invType = thisPtr->pseudoRegs.invType;
  inv->entry.code = thisPtr->trapFrame.EAX;
  inv->entry.w1 = thisPtr->trapFrame.EBX;
  inv->entry.w2 = thisPtr->trapFrame.ECX;
  inv->entry.w3 = thisPtr->trapFrame.EDX;

  sndKeys = (uint8_t *) &thisPtr->pseudoRegs.sndKeys;
  
  /* Not hazarded because invocation key */
  inv->entry.key[0] = &thisPtr->keyReg[sndKeys[0]];
  inv->entry.key[1] = &thisPtr->keyReg[sndKeys[1]];
  inv->entry.key[2] = &thisPtr->keyReg[sndKeys[2]];
  inv->entry.key[3] = &thisPtr->keyReg[sndKeys[3]];

  /* Figure out the string length: */
  
  len = thisPtr->pseudoRegs.sndLen;

  inv->entry.len = len;
  inv->sentLen = 0;		/* set in CopyOut */
}


/* NOTE that this can be called with /thisPtr/ == 0, and must guard
   against that possibility! */
INLINE void 
proc_SetupExitBlock(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
  uint8_t *rcvKeys = 0;
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
  rcvKeys = (uint8_t *) &thisPtr->pseudoRegs.rcvKeys;

  if (thisPtr->pseudoRegs.rcvKeys) {
    if (thisPtr->pseudoRegs.rcvKeys & 0xe0e0e0e0) {
      proc_SetFault(thisPtr, FC_BadExitBlock, 0, false);
      inv->suppressXfer = true;
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
  
  inv->validLen = thisPtr->trapFrame.ESI;

  /* Should this set a fault code? */
  if (inv->validLen > EROS_MESSAGE_LIMIT)
    inv->validLen = EROS_MESSAGE_LIMIT;

  assert( proc_IsRunnable(thisPtr) );
}



INLINE void 
proc_DeliverGateResult(Process* thisPtr, Invocation* inv /*@ not null @*/, bool wantFault)
{
  uint32_t rcvBase = 0;
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
  
  if (thisPtr->pseudoRegs.rcvKeys) {
    if (thisPtr->pseudoRegs.rcvKeys & 0x1f1f1fu) {
      if (inv->exit.pKey[0])
        key_NH_Set(inv->exit.pKey[0], inv->entry.key[0]);
      if (inv->exit.pKey[1])
	key_NH_Set(inv->exit.pKey[1], inv->entry.key[1]);
      if (inv->exit.pKey[2])
	key_NH_Set(inv->exit.pKey[2], inv->entry.key[2]);
    }
    
    if (inv->exit.pKey[RESUME_SLOT]) {
      if (inv->invType == IT_Call) {
	proc_BuildResumeKey(act_CurContext(), inv->exit.pKey[3]);
	if (wantFault)
	  inv->exit.pKey[RESUME_SLOT]->keyPerms = KPRM_FAULT;
      }
      else
	key_NH_Set(inv->exit.pKey[RESUME_SLOT], inv->entry.key[3]);
    }
  }

  /* copy return code and words */
  thisPtr->trapFrame.EAX = inv->entry.code;
  thisPtr->trapFrame.EBX = inv->entry.w1;
  thisPtr->trapFrame.ECX = inv->entry.w2;
  thisPtr->trapFrame.EDX = inv->entry.w3;

#ifdef OPTION_DDB
  /* Duplicate here so that debugger reporting is accurate. */
  inv->exit.code = inv->entry.code;
  inv->exit.w1 = inv->entry.w1;
  inv->exit.w2 = inv->entry.w2;
  inv->exit.w3 = inv->entry.w3;
#endif

  rcvBase = thisPtr->trapFrame.EDI;
  
  thisPtr->trapFrame.EDI = keyData;

  /* Data has already been copied out, so don't need to copy here.  DO
   * need to deliver the data length, however:
   */

  thisPtr->trapFrame.ESI = inv->sentLen;

  /* If the recipient specified an invalid receive area, though, they
   * are gonna get FC_ParmLack:
   */

  if (inv->validLen < inv->exit.len)
    proc_SetFault(thisPtr, FC_ParmLack, rcvBase + inv->validLen, false);


  /* Make sure it isn't later overwritten by the general delivery
   * mechanism.
   */
  inv->suppressXfer = true;
}


#ifdef ASM_VALIDATE_STRINGS
#error This is not the case.
/* The reason to do this is that until OPTION_PURE_ENTRY_STRINGS is removed
   there are a whole lot of places where this gets called.
   The net effect is that ASM_VALIDATE_STRINGS implies
   OPTION_PURE_ENTRY_STRINGS whether OPTION_PURE_ENTRY_STRINGS is set or not. */

INLINE void 
proc_SetupEntryString(Process* thisPtr, struct Invocation* inv /*@ not null @*/)
{
#ifndef OPTION_SMALL_SPACES
  const uint32_t bias = 0;
#endif
      
  uint32_t addr = (uint32_t) trapFrame.sndPtr + bias + KUVA;
  
  inv.entry.data = (uint8_t *) addr;
}

#endif

INLINE void 
proc_SetPC(Process* thisPtr, uint32_t oc)
{
  thisPtr->trapFrame.EIP = oc;
}

/* Called before AdjustInvocationPC() to capture the address of the
 * next instruction to run if the invocation is successful.
 */
INLINE uint32_t 
proc_CalcPostInvocationPC(Process* thisPtr)
{
  return thisPtr->trapFrame.EIP;
}

/* Called in the IPC path to reset the PC to point to the invocation
 * trap instruction...
 */
INLINE void 
proc_AdjustInvocationPC(Process* thisPtr)
{
  thisPtr->trapFrame.EIP -= 2;
}

INLINE uint32_t 
proc_GetPC(Process* thisPtr)
{
  return thisPtr->trapFrame.EIP;
}

INLINE void 
proc_ClearNextPC(Process* thisPtr)
{
  thisPtr->nextPC = 0xffffffff;	/* hopefully this PC value will trap if used */
}

INLINE void 
proc_SetInstrSingleStep(Process* thisPtr)
{
  thisPtr->hazards |= hz_SingleStep;
  thisPtr->saveArea = 0;
}


#endif /* __MACHINE_PROCESS_INLINE_H__ */
