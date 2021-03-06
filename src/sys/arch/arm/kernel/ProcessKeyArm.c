/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <kerninc/Key-inline.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/arch/arm/Process.h>

bool proc_GetRegs32(Process * thisPtr,
       struct capros_arch_arm_Process_Registers * regs);

void proc_SetRegs32(Process * thisPtr,
       struct capros_arch_arm_Process_Registers * regs);

/* May Yield. */
void
ProcessKey(Invocation * inv)
{
  if (key_PrepareForInv(inv->key))
    return;

  inv_GetReturnee(inv);

  assert(keyBits_IsPreparedObjectKey(inv->key));
  Process * proc = inv->key->u.gk.pContext;

  switch (inv->entry.code) {

  case OC_capros_arch_arm_Process_getRegisters:
    assert( proc_IsRunnable(inv->invokee) );

    {
      struct capros_arch_arm_Process_Registers regs;
      
      proc_Prepare(proc);

      proc_SetupExitString(inv->invokee, inv, sizeof(regs));

      COMMIT_POINT();

      bool b = proc_GetRegs32(proc, &regs);
      assert(b);	// because we prepared the process above
      (void)b;		// keep the compiler happy

      inv_CopyOut(inv, sizeof(regs), &regs);
      inv->exit.code = RC_OK;
      break;
    }
  
  case OC_capros_arch_arm_Process_setRegisters:
    {
      struct capros_arch_arm_Process_Registers regs;
      
      if ( inv->entry.len != sizeof(regs) ) {
	inv->exit.code = RC_capros_key_RequestError;
	COMMIT_POINT();
      
	break;
      }
      
      proc_Prepare(proc);

      COMMIT_POINT();

      inv_CopyIn(inv, sizeof(regs), &regs);

      proc_SetRegs32(proc, &regs);

      inv->exit.code = RC_OK;
      break;
    }

  default:
    return ProcessKeyCommon(inv, proc);
  }
  ReturnMessage(inv);
}
