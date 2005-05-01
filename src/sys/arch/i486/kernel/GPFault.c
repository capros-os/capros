/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

/* Drivers for 386 protection faults */

#include <eros/target.h>
#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/Debug.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Process.h>
#include "Segment.h"
#include "IDT.h"
#include "lostart.h"

extern void halt(char);

bool
GPFault(savearea_t *sa)
{
  fixreg_t iopl;
  Node *domRoot = 0;

  if ( sa_IsKernel(sa) ) {
    printf("Kernel GP fault. curctxt=0x%08x error=0x%x eip=0x%08x\n",
		   act_CurContext(),
		   sa->Error, sa->EIP);
#if 0
    printf("domRoot=0x%08x, keyRegs=0x%08x\n",
    		   Thread::CurContext()
		   ? ((ArchContext*)act_CurContext())->procRoot
		   : 0,
    		   Thread::CurContext()
		   ? ((ArchContext*)act_CurContext())->keyRegs
		   : 0);
    printf("0x%x Ctxt hzrd=0x%08x 0x%x hzrd rlu=0x%08x\n",
		   sa->EDX,
		   ((ArchContext*) sa->EDX)->hazards,
		   sa->EDI,
		   ((ArchContext*) sa->EDI)->hazards);
    printf("0x%x Ctxt rlu=0x%08x 0x%x ctxt rlu=0x%08x\n",
		   sa->EDX,
		   ((ArchContext*) sa->EDX)->fixRegs.ReloadUnits,
		   sa->EDI,
		   ((ArchContext*) sa->EDI)->fixRegs.ReloadUnits);
    printf("0x%x Ctxt keys=0x%08x 0x%x ctxt keys=0x%08x\n",
		   sa->EDX,
		   ((ArchContext*) sa->EDX)->keyRegs,
		   sa->EDI,
		   ((ArchContext*) sa->EDI)->keyRegs);
    printf("0x%x Ctxt EDX=0x%08x (offset %u)\n",
		   sa->EDX,
		   ((ArchContext*) sa->EDX)->fixRegs.EDX,
		   & ((ArchContext*) 0)->fixRegs.EDX );
#endif
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
       proc_HasDevicePriveleges(act_CurContext()) &&
       iopl != 3 ) {
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

  if (act_CurContext()->smallPTE) {
#if 0
    dprintf(true, "Small Space domain takes GP fault\n");
#endif

    proc_SwitchToLargeSpace(act_CurContext());

    return false;
  }

#endif
  

  domRoot = ((Process*) act_CurContext())->procRoot;
  printf("Domain ");
  printOid(domRoot->node_ObjHdr.kt_u.ob.oid);
  printf(" takes GP fault. error=0x%x eip=0x%08x\n",
	      sa->Error, sa->EIP);

#if 0
  sa->Dump();
#endif

  if (act_CurContext())
    proc_SetFault(((Process*) act_CurContext()), FC_GenProtection,
						sa->EIP, false);

  return false;
}
