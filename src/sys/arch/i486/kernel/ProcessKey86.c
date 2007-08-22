/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
#include <eros/Invoke.h>
#include <disk/DiskNodeStruct.h>

#include <idl/capros/key.h>
#include <idl/capros/arch/i386/Process.h>

#include "Process486.h"

void proc_SetRegs32(Process * thisPtr,
       struct capros_arch_i386_Process_Registers * regs);

/* May Yield. */
void
ProcessKey(Invocation * inv)
{
  Node * theNode = (Node *) key_GetObjectPtr(inv->key);

  switch (inv->entry.code) {

  case OC_capros_arch_i386_Process_setIoSpace:
    node_MakeDirty(theNode);

    COMMIT_POINT();

    node_ClearHazard(theNode, ProcIoSpace);

    key_NH_Set(node_GetKeyAtSlot(theNode, ProcIoSpace), inv->entry.key[0]);

    act_Prepare(act_Current());
      
    inv->exit.code = RC_OK;
    return;

  case OC_capros_arch_i386_Process_getRegisters:
    assert( proc_IsRunnable(inv->invokee) );

    {
      struct capros_arch_i386_Process_Registers regs;
      
      Process * ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      proc_SetupExitString(inv->invokee, inv, sizeof(regs));

      COMMIT_POINT();

      bool b = proc_GetRegs32(ac, &regs);
      assert(b);	// because we prepared the process above

      inv_CopyOut(inv, sizeof(regs), &regs);
      inv->exit.code = RC_OK;
      return;
    }
  
  case OC_capros_arch_i386_Process_setRegisters:
    {
      struct capros_arch_i386_Process_Registers regs;
      
      if ( inv->entry.len != sizeof(regs) ) {
	inv->exit.code = RC_capros_key_RequestError;
	COMMIT_POINT();
      
	return;
      }
      
      Process * ac = node_GetDomainContext(theNode);
      proc_Prepare(ac);

      COMMIT_POINT();

      inv_CopyIn(inv, sizeof(regs), &regs);

      proc_SetRegs32(ac, &regs);

      inv->exit.code = RC_OK;
      return;
    }

  default:
    return ProcessKeyCommon(inv, theNode);
  }
}
