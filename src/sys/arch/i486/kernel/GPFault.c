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
#include <kerninc/Machine.h>
#include <kerninc/Debug.h>
#include <kerninc/Process.h>
#include <kerninc/Node.h>
#include <idl/capros/arch/i386/Process.h>
#include "Process486.h"
#include <arch-kerninc/Process.h>
#include "Segment.h"
#include "IDT.h"
#include "asm.h"

bool
GPFault(savearea_t *sa)
{
  fixreg_t iopl;

  if ( sa_IsKernel(sa) ) {
    printf("Kernel GP fault. curctxt=0x%08x error=0x%x eip=0x%08x\n",
		   act_CurContext(),
		   sa->Error, sa->EIP);
#if 1
    DumpFixRegs(sa);
#endif
#if 0
    Debug::Backtrace();
#endif
    halt('t');
  }

  /* If this is GP(0), and the domain holds the DevicePrivs key in a
   * register, but it's privilege level is not appropriate for I/O
   * access, escalate it's privilege level. */


  iopl = (act_CurContext()->trapFrame.EFLAGS & MASK_EFLAGS_IOPL) >> SHIFT_EFLAGS_IOPL;


  if ( act_CurContext()->trapFrame.Error == 0 &&
       proc_HasDevicePrivileges(act_CurContext()) &&
       iopl != 3 ) {
    /* I don't think this can happen, now that we set IOPL in
    proc_ValidateRegValues. */
    act_CurContext()->trapFrame.EFLAGS |= MASK_EFLAGS_IOPL;

#if 0
    dprintf(true, "IO privileges now esclated. EFLAGS=0x%x\n", Thread::CurContext()->trapFrame.EFLAGS);
#endif
    return false;
  }
  
  /* If the general protection fault is on a domain marked as a small
   * space domain, upgrade the domain to a large space domain and let
   * it retry the reference before concluding that we should take this
   * fault seriously.
   */
#ifdef OPTION_SMALL_SPACES

  if (act_CurContext()->md.smallPTE) {
#if 0
    dprintf(true, "Small Space domain takes GP fault\n");
#endif

    proc_SwitchToLargeSpace(act_CurContext());

    return false;
  }

#endif
  
  printf("Process %#x takes GP fault. error=%#x eip=%#x\n",
	      act_CurContext(), sa->Error, sa->EIP);

#if 0
  sa->Dump();
#endif

  if (act_CurContext())
    proc_SetFault(act_CurContext(), capros_arch_i386_Process_FC_GeneralProtection, sa->EIP);

  return false;
}
