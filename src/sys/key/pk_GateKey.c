/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
#include <kerninc/Invocation.h>
#include <kerninc/Activity.h>
#include <kerninc/KernStats.h>
#include <arch-kerninc/Process-inline.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <disk/DiskNodeStruct.h>

/* #define GK_DEBUG */

/* May Yield. */
void
GateKey(Invocation* inv /*@ not null @*/)
{
  /* Make a local copy (so the compiler can optimize it) */
  Process * invokee = inv->key->u.gk.pContext;
  if (! proc_IsRunnable(invokee)) {
    proc_DoPrepare(invokee);		/* may yield */

    if (! proc_IsWellFormed(invokee)) {
#ifndef NDEBUG
      printf("Jumpee malformed\n");
#endif

      /* Pretend we invoked a void key. */
      // No need to set inv->key and inv->invKeyType.
      VoidKey(inv);
      return;
    }
  }
  // Runnable implies well-formed.
  assert(proc_IsWellFormed(invokee));
  inv->invokee = invokee;

  // Check invokee's state.
  assert(! keyBits_IsType(inv->key, KKT_Resume)
         || invokee->runState == RS_Waiting );

  if (inv->invKeyType == KKT_Start && invokee->runState != RS_Available) {
  
#ifdef GATEDEBUG
    dprintf(GATEDEBUG>2, "Start key, not Available\n");
#endif
  
    act_SleepOn(&invokee->stallQ);
    act_Yield();
  }

  Process * invoker = act_CurContext();
  assert(invokee != invoker);

  inv_SetupExitBlock(inv);

#ifdef GK_DEBUG
  printf("Enter GateKey(), invokedKey=0x%08x\n", inv->key);
#endif
  
  proc_SetupExitString(invokee, inv, inv->entry.len);

  COMMIT_POINT();

#ifdef OPTION_KERN_STATS
  KernStats.nGateJmp++;
#endif

  assert(inv->invokee);
  assert(proc_IsRunnable(invokee));

  /* We copy the message here, not calling ReturnMessage(). */

  if (inv->invKeyType == KKT_Resume)
    keyR_ZapResumeKeys(&invokee->keyRing);
  act_AssignTo(allocatedActivity, invokee);

  if (inv->invType == IT_Send) {
    act_Wakeup(allocatedActivity);
  }

  invokee->runState = RS_Running;

  if (! proc_IsExpectingMsg(invokee)) {
    /* A segment keeper or process keeper is invoking the resume key
    it received. 
    A zero order code means clear the fault.
    A nonzero order code means send any fault to the process keeper. */

#if 0
    dprintf(true, "Invoking fault key, code=%d\n", inv->entry.code);
#endif
    
    if (inv->entry.code && invokee->faultCode) {
      /* Send the fault to the process keeper (not the segment keeper). */
      invokee->processFlags |= capros_Process_PF_FaultToProcessKeeper;
    }
    else
      proc_ClearFault(invokee);
    goto exit;
  }

  /* Transfer the data: */
#ifdef GK_DEBUG
  dprintf(true, "Send %d bytes at 0x%08x, Valid Rcv %d bytes at 0x%08x\n",
         inv->entry.len, inv->entry.data,
         inv->validLen, inv->exit.data);
#endif
  
  inv_CopyOut(inv, inv->entry.len, inv->entry.data);

  /* Note that the following will NOT work in the returning to self
   * case, which is presently messed up anyway!!!
   */

#ifdef GK_DEBUG
  printf("Gate: Copying keys\n");
#endif
  
  if (proc_GetRcvKeys(invokee)) {
    if (proc_GetRcvKeys(invokee) & 0x1f1f1fu) {
      if (inv->exit.pKey[0])
        key_NH_Set(inv->exit.pKey[0], inv->entry.key[0]);
      if (inv->exit.pKey[1])
	key_NH_Set(inv->exit.pKey[1], inv->entry.key[1]);
      if (inv->exit.pKey[2])
	key_NH_Set(inv->exit.pKey[2], inv->entry.key[2]);
    }
    
    if (inv->exit.pKey[RESUME_SLOT]) {
      if (invType_IsCall(inv->invType)) {
	proc_BuildResumeKey(invoker, inv->exit.pKey[RESUME_SLOT]);
      }
      else
	key_NH_Set(inv->exit.pKey[RESUME_SLOT], inv->entry.key[RESUME_SLOT]);
    }
  }

  proc_DeliverGateResult(invokee, inv, false);
  proc_AdvancePostInvocationPC(invokee);
  
exit:
#ifndef NDEBUG
  allocatedActivity = 0;
  InvocationCommitted = false;
#endif
  return;
}
