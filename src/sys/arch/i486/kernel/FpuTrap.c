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

/* Drivers for 386 protection faults */

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Debug.h>
#include <kerninc/Machine.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Process.h>
#include "IDT.h"

extern void halt(char);

bool
DeviceNotAvailException(savearea_t *sa)
{
  if (sa_IsKernel(sa)) {
    dprintf(true, "Kernel floating point error\n");
    DumpFixRegs(sa);
    halt('f');
  }

#ifndef EROS_HAVE_FPU
#error not supported
#else

  if (proc_fpuOwner == act_CurContext()) {
    /* If the current process owns the FPU, then this fault should be
     * taken seriously:
     */

#if 0
    uint16_t msw;
    __asm__ __volatile__("smsw %0\n\t" : "=r" (msw));


    dprintf(false, "Active context 0x%08x fp fault pc=0x%08x"
		    " msw=0x%04x\n",
		    act_CurContext(), sa->EIP, msw);
    act_CurContext()->SaveFPU();
    act_CurContext()->DumpFloatRegs();
    
    dprintf(true, "Pausing...\n");
#endif
    
    if (act_CurContext())
      proc_SetFault(act_CurContext(), FC_FloatingPointError, sa->EIP);
  }
  else {
#if 0
    dprintf(false, "Forcing numerics load for ctxt 0x%08x...\n",
		    Thread::CurContext());
#endif
    act_CurContext()->hazards |= hz_NumericsUnit;
  }

#endif

  return false;
}
