/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006-2008, 2010, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/Key-inline.h>
#include <kerninc/Activity.h>
#include <kerninc/Node.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/arch/i386/Process.h>

#include "Process486.h"

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

  case OC_capros_arch_i386_Process_getRegisters:
    assert( proc_IsRunnable(inv->invokee) );

    {
      struct capros_arch_i386_Process_Registers regs;
      
      proc_Prepare(proc);

      proc_SetupExitString(inv->invokee, inv, sizeof(regs));

      COMMIT_POINT();

      bool b = proc_GetRegs32(proc, &regs);
      (void)b;		// avoid compiler warning
      assert(b);	// because we prepared the process above

      inv_CopyOut(inv, sizeof(regs), &regs);
      inv->exit.code = RC_OK;
      break;
    }
  
  case OC_capros_arch_i386_Process_setRegisters:
    {
      struct capros_arch_i386_Process_Registers regs;
      
      if ( inv->entry.len != sizeof(regs) ) {
	inv->exit.code = RC_capros_key_RequestError;
	COMMIT_POINT();
      
	break;
      }
      
      proc_Prepare(proc);

      COMMIT_POINT();

      inv_CopyIn(inv, sizeof(regs), &regs);

      proc_SetCommonRegs32(proc,
                           (struct capros_Process_CommonRegisters32 *) &regs);
      
      proc->trapFrame.EDI    = regs.EDI;
      proc->trapFrame.ESI    = regs.ESI;
      proc->trapFrame.EBP    = regs.EBP;
      proc->trapFrame.EBX    = regs.EBX;
      proc->trapFrame.EDX    = regs.EDX;
      proc->trapFrame.ECX    = regs.ECX;
      proc->trapFrame.EAX    = regs.EAX;
      proc->trapFrame.EFLAGS = regs.EFLAGS;
      proc->trapFrame.CS     = regs.CS;
      proc->trapFrame.SS     = regs.SS;
      proc->trapFrame.ES     = regs.ES;
      proc->trapFrame.DS     = regs.DS;
      proc->trapFrame.FS     = regs.FS;
      proc->trapFrame.GS     = regs.GS;

#ifdef EROS_HAVE_FPU
      proc->fpuRegs.f_ctrl	= regs.FPU_ControlWord;
      proc->fpuRegs.f_status	= regs.FPU_StatusWord;
      proc->fpuRegs.f_tag	= regs.FPU_TagWord;
      proc->fpuRegs.f_ip	= regs.FPU_InstructionPointer;
      proc->fpuRegs.f_cs	= regs.FPU_InstructionPointerSelector;
      proc->fpuRegs.f_opcode	= regs.FPU_Opcode;
      proc->fpuRegs.f_dp	= regs.FPU_OperandPointer;
      proc->fpuRegs.f_ds	= regs.FPU_OperandPointerSelector;
      // f_r0 through f_r7 are consecutive and contiguous.
      memcpy(&proc->fpuRegs.f_r0, &regs.FPU_Data[0], 80);
#endif

      proc_ValidateRegValues(proc);

      inv->exit.code = RC_OK;
      break;
    }

  default:
    return ProcessKeyCommon(inv, proc);
  }
  ReturnMessage(inv);
}
