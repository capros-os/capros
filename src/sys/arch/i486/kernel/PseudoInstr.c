/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Machine.h>
#include <kerninc/Debug.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Process.h>
#include <kerninc/ObjectCache.h>
#include <arch-kerninc/Process.h>
#include "IDT.h"
#include "lostart.h"

#define pi_copy_cap  0		/* handled in fast path */
#define pi_xchg_cap  1		/* handled in fast path */
/* At the moment, none are handled here. */

#define dbg_capstore	0x1
#define dbg_capload	0x2

/* Following should be an OR of some of the above */
#define dbg_flags	( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

extern bool PteZapped;

extern void halt(char);

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

  PteZapped = false;
  objH_BeginTransaction();

  switch (sa->EAX) {
    
  default:
    proc_SetFault(ctxt, FC_BadOpcode, sa->EIP, false);
    break;
  }

  return false;
}
