/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
#include <kerninc/Machine.h>
#include <kerninc/Debug.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Process.h>
#include <kerninc/ObjectCache.h>
#include <arch-kerninc/Process.h>
#include "IDT.h"
#include "asm.h"

#define pi_copy_cap  0		/* handled in fast path */
#define pi_xchg_cap  1		/* handled in fast path */
/* At the moment, none are handled here. */

#define dbg_capstore	0x1
#define dbg_capload	0x2

/* Following should be an OR of some of the above */
#define dbg_flags	( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

bool
PseudoInstrException(savearea_t *sa)
{
  Process* ctxt = 0;
  if ( sa_IsKernel(sa) ) {
    halt('p');
  }

  ctxt = (Process*) act_CurContext();

  /* Because this path calls the page fault handler, we must establish
   * a thread recovery block here.
   */

  assert(& ctxt->trapFrame == sa);

  objH_BeginTransaction();

  switch (sa->EAX) {
    
  default:
    proc_SetFault(ctxt, capros_Process_FC_BadOpcode, sa->EIP);
    break;
  }

  return false;
}
