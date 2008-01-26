/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <arch-kerninc/Process-inline.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <disk/DiskNodeStruct.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>

static void
prockey_getSlot(Invocation * inv, Node * theNode, uint32_t slot)
{
  COMMIT_POINT();

  if (keyBits_IsRdHazard(node_GetKeyAtSlot(theNode, slot)))
	node_ClearHazard(theNode, slot);
  inv_SetExitKey(inv, 0, &theNode->slot[slot]);

  inv->exit.code = RC_OK;
  return;
}

static void
prockey_swapSlotCommitted(Invocation * inv, Node * theNode, uint32_t slot)
{
  node_ClearHazard(theNode, slot);

  Key k;		/* temporary in case send and receive */
			/* slots are the same. */
  keyBits_InitToVoid(&k);
        
  key_NH_Set(&k, &theNode->slot[slot]);

  key_NH_Set(node_GetKeyAtSlot(theNode, slot), inv->entry.key[0]);
  inv_SetExitKey(inv, 0, &k);
        
  /* Unchain, but do not unprepare -- the objects do not have
     on-disk keys. */
  key_NH_Unchain(&k);

  act_Prepare(act_Current());
      
  inv->exit.code = RC_OK;
  return;
}

static void
prockey_swapSlot(Invocation * inv, Node * theNode, uint32_t slot)
{
  node_MakeDirty(theNode);

  COMMIT_POINT();

  return prockey_swapSlotCommitted(inv, theNode, slot);
}

/* May Yield. */
void
ProcessKeyCommon(Invocation * inv, Node * theNode)
{
  Process * p;

  switch (inv->entry.code) {
  case OC_capros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Process;
      return;
    }

  case OC_capros_Process_getSchedule:
    return prockey_getSlot(inv, theNode, ProcSched);
  case OC_capros_Process_swapSchedule:
    if (inv->invokee && theNode == inv->invokee->procRoot)
      dprintf(false, "Modifying invokee domain root\n");

    return prockey_swapSlot(inv, theNode, ProcSched);

  case OC_capros_Process_getAddrSpace:
    return prockey_getSlot(inv, theNode, ProcAddrSpace);
  case OC_capros_Process_swapAddrSpace:
    return prockey_swapSlot(inv, theNode, ProcAddrSpace);
  
  case OC_capros_Process_swapAddrSpaceAndPC32Proto:
      inv->exit.w1 = inv->entry.w2;
  case OC_capros_Process_swapAddrSpaceAndPC32:
    {
      Process * ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      node_MakeDirty(theNode);

      COMMIT_POINT();

      proc_SetPC(ac, inv->entry.w1);
      if (proc_IsExpectingMsg(ac)) {
        /* If the process is expecting a message, then when it becomes
        Running, its PC will be incremented. 
        Counteract that here, so the process will start executing
        at the specified PC. */
        proc_AdjustInvocationPC(ac);
      }
      
      return prockey_swapSlotCommitted(inv, theNode, ProcAddrSpace);
    }

  case OC_capros_Process_setIOSpace:
    node_MakeDirty(theNode);

    COMMIT_POINT();

    node_ClearHazard(theNode, ProcIoSpace);

    key_NH_Set(node_GetKeyAtSlot(theNode, ProcIoSpace), inv->entry.key[0]);
        
    act_Prepare(act_Current());
      
    inv->exit.code = RC_OK;
    return;
  
  case OC_capros_Process_getKeeper:
    return prockey_getSlot(inv, theNode, ProcKeeper);
  case OC_capros_Process_swapKeeper:
    return prockey_swapSlot(inv, theNode, ProcKeeper);

  case OC_capros_Process_getSymSpace:
    return prockey_getSlot(inv, theNode, ProcSymSpace);
  case OC_capros_Process_swapSymSpace:
    return prockey_swapSlot(inv, theNode, ProcSymSpace);

  case OC_capros_Process_getKeyReg:
    {
      Process* ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      COMMIT_POINT();

      uint32_t slot = inv->entry.w1;

      if (slot < EROS_NODE_SIZE) {
	inv_SetExitKey(inv, 0, &ac->keyReg[slot]);
	inv->exit.code = RC_OK;
      }
      else {
	inv->exit.code = RC_capros_key_RequestError;
      }

      return;
    }

  case OC_capros_Process_swapKeyReg:
    {
      Process * ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      COMMIT_POINT();

      uint32_t slot = inv->entry.w1;

      if (slot >= EROS_NODE_SIZE) {
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      Key k;		/* temporary in case send and receive */
			/* slots are the same. */
      keyBits_InitToVoid(&k);
        
      Key * targetSlot = &ac->keyReg[slot];
      key_NH_Set(&k, targetSlot);

      if (slot != KR_VOID) {
	/* FIX: verify that the damn thing HAD key registers?? */
        key_NH_Set(targetSlot, inv->entry.key[0]);
      }

      inv_SetExitKey(inv, 0, &k);
        
      /* Unchain, but do not unprepare -- the objects do not have
         on-disk keys. */
      key_NH_Unchain(&k);

      inv->exit.code = RC_OK;
      return;
    }

  case OC_capros_Process_makeStartKey:
    {
      p = node_GetDomainContext(theNode);
      proc_Prepare(p);

      COMMIT_POINT();
      
      uint32_t keyData = inv->entry.w1;
      
      if ( keyData > EROS_KEYDATA_MAX ) {
	inv->exit.code = RC_capros_key_RequestError;
	dprintf(true, "Value 0x%08x is out of range\n",	keyData);
      
	return;
      }
      
      Key * k = inv->exit.pKey[0];
      if (k) {		// the key is being received
	key_NH_Unchain(k);
        keyBits_InitType(k, KKT_Start);
	k->keyData = keyData;

        /* Prepare the key, for performance.
           Be careful, as gate keys do not go on the same key chain as
           process keys. */
	k->u.gk.pContext = p;
	link_insertAfter(&p->keyRing, &k->u.gk.kr);
	keyBits_SetPrepared(k);
      }

      inv->exit.code = RC_OK;
      return;
    }

  case OC_capros_Process_makeResumeKey:
    p = node_GetDomainContext(theNode);
    proc_Prepare(p);

    COMMIT_POINT();

    /* we should zap other resume keys here.
     Note, if a process Calls a key to itself, that should invalidate the
     invokee. */

    // If there is a resume key, state must be Waiting.
    /* There are several cases to consider:
      A. p == invoker
        A1. Invocation is a Call, invoker == invokee
           Step 1: invoker calls, becomes Waiting.
                   Kernel holds a Resume key to invoker.
           Step 2: key operation happens, process is already Waiting.
                   Resume key held by kernel is zapped. 
           Step 3: resume key that was produced by the Call is gone,
                   so there is no return. Invoker remains Waiting.
        A2. Invocation is a Return
           Invoker was running, therefore there can be no resume keys
           to invoker, therefore passed resume key cannot be to invoker. 
           Step 1. Invoker Returns, becomes Available, expectingMsg.
           Step 2. Key operation happens. Process is Available,
                   changed to Waiting.
           Step 3. passed resume key, if any, is invoked
        A3. Invocation is a Send.
           Invoker was running, therefore there can be no resume keys
           to invoker, therefore passed resume key cannot be to invoker. 
           Step 1. Invoker Sends, remains Running. 
           Step 2. key operation happens, process is Running,
                   changed to Waiting. We must make sure the invoker does
                   not run.
           Step 3. passed resume key, if any, is invoked
      B. p != invoker
        B1. Invocation is a Call, invoker == invokee
           No problem.
        B2. Invocation is a Send or Return
           Invoker was running, therefore there can be no resume keys
           to invoker, therefore invoker != invokee.
           B2a. p == invokee
             Step 1. Invoker invokes. 
             Step 2. Key operation happens. Invokee must be Waiting,
                     remains Waiting.
                     Resume key to invokee is zapped.
             Step 3. No invokee, so no return.
           B2b. p != invokee
             No problem.
    */

    p->runState = RS_Waiting;

    if (p->curActivity) {
      if (p == act_CurContext())
        if (p->runState == RS_Running) {
          // Case A3. Invokee will get allocatedActivity, so kill p->curActivity
          // act_ForceResched(); ?
          proc_ClearActivity(p);
        } else {
          // Cases A1 and A2. 
          // Save p->curActivity, as it will be needed for the invokee.
        }
      else {
        // p is now Waiting, so it mustn't have an Activity.
        proc_ClearActivity(p);
      }
    }

    keyR_ZapResumeKeys(&p->keyRing);

    if (p == inv->invokee) {	// Case A1 or B2a
      inv->invokee = NULL;
      return;
    }

    if (inv->exit.pKey[0]) {
      inv_SetExitKey(inv, 0, inv->key);
      /* Unprepare, because gate keys are chained to the Process,
         while Process keys are chained to the root node.
         (Fix this, or use key_MakeUnpreparedCopy instead.) */
      key_NH_Unprepare(inv->exit.pKey[0]);
      keyBits_InitType(inv->exit.pKey[0], KKT_Resume);
      inv->exit.pKey[0]->keyPerms = 0;
    }

    inv->exit.code = RC_OK;

    return;

  case OC_capros_Process_getRegisters32:
    assert( proc_IsRunnable(inv->invokee) );

    {
      struct capros_Process_CommonRegisters32 regs;

      p = node_GetDomainContext(theNode);
      proc_Prepare(p);

      proc_SetupExitString(inv->invokee, inv, sizeof(regs));

      COMMIT_POINT();

      proc_GetCommonRegs32(p, &regs);

      regs.len = sizeof(regs);

      inv_CopyOut(inv, sizeof(regs), &regs);
      inv->exit.code = RC_OK;
      return;
    }

  case OC_capros_Process_setRegisters32:
    {
      struct capros_Process_CommonRegisters32 regs;

      if ( inv->entry.len < sizeof(regs) ) {
        inv->exit.code = RC_capros_key_RequestError;
        COMMIT_POINT();

        return;
      }

      p = node_GetDomainContext(theNode);
      proc_Prepare(p);

      COMMIT_POINT();

      inv_CopyIn(inv, sizeof(regs), &regs);

      proc_SetCommonRegs32(p, &regs);

      inv->exit.code = RC_OK;
      return;
    }

  default:
    COMMIT_POINT();
      
    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }
}
