/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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

/* #define GK_DEBUG */

/* May Yield. */
void
GateKey(Invocation* inv /*@ not null @*/)
{
  Process *invokee = inv->invokee;
  
#ifdef GK_DEBUG
  printf("Enter GateKey(), invokedKey=0x%08x\n", inv->key);
#endif

  assert(! keyBits_IsType(inv->key, KKT_Resume)
         || invokee->runState == RS_Waiting );
  
  proc_SetupExitString(inv->invokee, inv, inv->entry.len);

  COMMIT_POINT();

#ifdef OPTION_KERN_STATS
  KernStats.nGateJmp++;
#endif

  assert(inv->invokee);	// FIXME: what about invoking gate key to malformed?

  /* Make sure it isn't later overwritten by the general delivery
   * mechanism.  */
  inv->suppressXfer = true;

  if (! proc_IsExpectingMsg(inv->invokee)) {
#if 0
    dprintf(true, "Invoking fault key, code=%d\n", inv->entry.code);
#endif
    // Invokee not expecting a message.
    // FIXME: want the following behavior if returning to fault key too.
    
    if (inv->entry.code) {
	/* Regardless of what the fault code may be, there is no
	 * action that a keeper can take via the fault key that can
	 * require an uncleared fault demanding slow-path validation
	 * is already required for some other reason.  It is therefore 
	 * safe to use 'false' here.
	 */
	proc_SetFault(invokee, invokee->faultCode, invokee->faultInfo,
			  false);
    }
    else
	proc_SetFault(invokee, FC_NoFault, 0, false);
    return;
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
	proc_BuildResumeKey(act_CurContext(), inv->exit.pKey[RESUME_SLOT]);
      }
      else
	key_NH_Set(inv->exit.pKey[RESUME_SLOT], inv->entry.key[RESUME_SLOT]);
    }
  }

  proc_DeliverGateResult(invokee, inv, false);
  
  return;
}
