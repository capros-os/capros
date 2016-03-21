/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2010, Strawberry Development Group.
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

/* Drivers for 386 protection faults */

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Debug.h>
#include <kerninc/Machine.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Process.h>
#include "IDT.h"
#include "asm.h"

bool
DeviceNotAvailException(savearea_t *sa)
{
#ifdef EROS_HAVE_FPU

  // We should always have EM=0 and MP=1,
  // in which case this exception can only happen if TS=1.
  assert((ReadCR0() & (CR0_TS | CR0_EM | CR0_MP)) == (CR0_TS | CR0_MP));

  if (sa_IsKernel(sa)) {
    dprintf(true, "Kernel using floating point\n");
    halt('f');
  }

#if 0
  printf("Fpu Not Avail fault, proc=%#x EIP=%#x fpuOwner=%#x.\n",
         act_CurContext(), act_CurContext()->trapFrame.EIP, proc_fpuOwner);
#endif

  assert(proc_fpuOwner != proc_Current());	// else we would have cleared TS

#if 0
  printf("Forcing FPU load for proc %#x.\n", proc_Current());
#endif
  proc_Current()->hazards |= hz_NumericsUnit;

#else
  Debugger();	// No FPU?
#endif

  return false;
}

bool
CoprocErrorFault(savearea_t* sa)
{
#ifdef EROS_HAVE_FPU

  /* The following can occur:
     1. Process A uses the FPU and generates an unmasked exception condition.
     2. Before process A executes its next FPU instruction,
        the kernel switches context to process B.
        The exception condition is still pending.
     3. Process B (or a subsequent process) attempts to execute
        an FPU instruction. Because process A is still the fpuOwner,
        the kernel saves process A's FPU state.
     4. The fsave instruction executed by the kernel causes the
        pending floating point exception to be recognized,
        which gets us here.
     That is why we allow an exception at FsaveInstruction.
  */

extern void FsaveInstruction();
  if (sa_IsKernel(sa)
      && sa->EIP != (uint32_t)FsaveInstruction) {
    dprintf(true, "Kernel floating point exception\n");
    halt('o');
  }

  // This fault belongs to proc_fpuOwner, not proc_Current().
  proc_SetFault(proc_fpuOwner, capros_Process_FC_FPFault, sa->EIP);
#if 0
  printf("Delayed FPU Exception\n");
  proc_DumpFloatRegs(proc_fpuOwner);
#endif

#else
  Debugger();	// No FPU?
#endif

  return false;
}
