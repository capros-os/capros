/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2010, Strawberry Development Group.
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
#include <kerninc/Process.h>
#include <idl/capros/arch/i386/Process.h>
#include "Process486.h"
#include <arch-kerninc/Process.h>
#include "IDT.h"
#include "asm.h"

bool
SSFault(savearea_t* sa)
{
  if ( sa_IsKernel(sa) ) {
    debug_Backtrace(0, true);
    halt('s');
  }

  /* If the stack segment fault is on a domain marked as a small space
   * domain, upgrade the domain to a large space domain and let it
   * retry the reference before concluding that we should take this
   * fault seriously.
   */
#ifdef OPTION_SMALL_SPACES

  if (act_CurContext()->md.smallPTE) {
#if 0
    dprintf(true, "Small Space domain takes SS fault\n");
#endif

    proc_SwitchToLargeSpace(act_CurContext());

    return false;
  }

#endif
  
  /* Should this have a dedicated error code? */

  proc_SetFault(act_CurContext(), capros_arch_i386_Process_FC_StackSeg, sa->EIP);


  return false;
}
