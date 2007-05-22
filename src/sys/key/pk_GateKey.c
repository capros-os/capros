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

  if (keyBits_IsType(inv->key, KKT_Resume)) {
    if (inv->key->keyPerms == KPRM_FAULT) {
      inv->suppressXfer = true;

      COMMIT_POINT();
    
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
    
    assert (invokee->runState == RS_Waiting);
  }

#ifdef GK_DEBUG
  printf("Gate: Copying keys\n");
#endif
  
#ifndef OPTION_PURE_ENTRY_STRINGS
  proc_SetupEntryString();
#endif
#ifndef OPTION_PURE_EXIT_STRINGS
  proc_SetupExitString(inv->invokee, inv, inv->entry.len);
#endif

  COMMIT_POINT();

  /* Transfer the data: */
#ifdef GK_DEBUG
  printf("Send %d bytes, Valid Rcv %d bytes\n", inv->entry.len, inv->validLen);
#endif
  
  inv_CopyOut(inv, inv->entry.len, inv->entry.data);

#ifdef OPTION_KERN_STATS
  KernStats.nGateJmp++;
  KernStats.bytesMoved += inv->exit.len;
#endif

  /* Note that the following will NOT work in the returning to self
   * case, which is presently messed up anyway!!!
   */

  proc_DeliverGateResult(invokee, inv, false);
  
  return;
}
