/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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

#include <string.h>
#include <disk/DiskNodeStruct.h>
#include <kerninc/kernel.h>
#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/PTE.h>
#include "Segment.h"
#include "lostart.h"

#if 1
/* We need to allow I/O for at least the main() thread so that it can
 * perform driver initialization
 */
#define KERN_EFLAGS 0x1200;	/* allow IO for kernel threads */
#else
#define KERN_EFLAGS 0x0200;	/* disallow IO for kernel threads */
#endif

/* Initialize a 386 kernel thread: */

Process *
kproc_Init(
           const char *myName,
           Activity* theActivity,
	   Priority prio,
           ReadyQueue *rq,
           void (*pc)(),
           uint32_t * stkBottom, uint32_t * stkTop)
{
  /* If you are confused about what's going on here, remember that
   * the class members grow upwards while the stack grows downwards.
   * We are constructing a save area by hand.  Have a look at the
   * comments in switch.S to see what that must look like.
   */
  int i = 0;
  Process *p = proc_allocate(false);

  p->hazards = 0;
#ifdef OPTION_SMALL_SPACES
  p->md.smallPTE = 0;	/* to satisfy logic in various places */
  p->md.bias = 0;
#endif
  /*p->priority = prio;*/
  p->readyQ = rq;
  /*
    p->prioQ.queue = rq->queue;
    p->prioQ.mask = rq->mask;
    p->prioQ.other = rq->other;
    p->prioQ.enqueue = rq->enqueue;
  */
  
  for (i = 0; i < 8; i++)
    p->name[i] = myName[i];
  p->name[7] = 0;

  p->curActivity = theActivity;
  p->runState = RS_Running;

  memset(&p->trapFrame, 0, sizeof(p->trapFrame));
  
  p->md.MappingTable = KernPageDir_pa; /* kern procs run in kernel space */
  p->trapFrame.EFLAGS = KERN_EFLAGS;

  p->trapFrame.CS = sel_KProcCode;
  p->trapFrame.DS = sel_KProcData;
  p->trapFrame.ES = sel_KProcData;
  p->trapFrame.FS = sel_KProcData;
  p->trapFrame.GS = sel_KProcData;
  p->trapFrame.SS = sel_KProcData;

  p->trapFrame.EIP = (uint32_t) pc;

  /* YES the first of these is dead code.  It suppresses the unused
   * argument warning if DBG_WILD_PTR is not enabled.
   */
  p->trapFrame.ESP = (uint32_t) stkBottom;
  p->trapFrame.ESP = (uint32_t) stkTop;
    
  return p;
}

extern void resume_from_kernel_interrupt(savearea_t *) NORETURN;
