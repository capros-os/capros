/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/PTE.h>

/* Here we gather some kernel variables.
   Keeping them together helps reduce their cache footprint. */

/* PteZapped serves two purposes.
   1. It says whether the TLB needs to be flushed. This allows us to defer,
      and thus possibly combine, TLB flushes. The procedure UpdateTLB()
      checks PteZapped; if true, it flushes the cache and clears PteZapped.
   2. It says whether some mapped memory may have been invalidated 
      during the dry run of some kernel operation.
   These two uses are compatible because UpdateTLB is only called right
   before returning to user mode. 
   If you are considering calling it elsewhere, because you want the TLB
   up to date, do this instead: if PteZapped, act_Yield (which will retry
   the operation). 
 */
bool PteZapped = false;

/* act_curActivity must be initialized (to 0) early, because during
initialization, we call malloc, which under some circumstances can
call act_Yield. We catch that problem by checking for a nonzero
value in act_curActivity. */
/* Eventually act_curActivity should be per-CPU. */
Activity * act_curActivity __attribute__((aligned(4),
					   section(".data"))) = 0;

/* proc_curProcess is always act_curActivity->context
   (or NULL if act_curActivity is NULL). */
struct Process * proc_curProcess __attribute__((aligned(4),
                                           section(".data"))) = 0;

unsigned int deferredWork __attribute__((aligned(4),
				        section(".data"))) = 0;

#ifdef OPTION_KERN_TIMING_STATS
uint32_t inv_delta_reset __attribute__((aligned(4),
					section(".data")));
uint64_t inv_delta_cy __attribute__((aligned(4),
				     section(".data")));
uint64_t inv_handler_cy __attribute__((aligned(4),
				       section(".data")));
uint64_t pf_delta_cy __attribute__((aligned(4),
				    section(".data")));
uint64_t kpr_delta_cy __attribute__((aligned(4),
				    section(".data")));
#ifdef OPTION_KERN_EVENT_TRACING
uint64_t inv_delta_cnt0 __attribute__((aligned(4),
				       section(".data")));
uint64_t inv_delta_cnt1 __attribute__((aligned(4),
				       section(".data")));
uint64_t pf_delta_cnt0 __attribute__((aligned(4),
				      section(".data")));
uint64_t pf_delta_cnt1 __attribute__((aligned(4),
				      section(".data")));
uint64_t kpr_delta_cnt0 __attribute__((aligned(4),
				      section(".data")));
uint64_t kpr_delta_cnt1 __attribute__((aligned(4),
				      section(".data")));
#endif
#endif
PTE* pte_kern_fstbuf  __attribute__((aligned(4),
				      section(".data")));
PTE* pte_kern_ptebuf  __attribute__((aligned(4),
				      section(".data")));

/* Allocate the kernel stack in the bss section. */
uint32_t kernelStack[EROS_KSTACK_SIZE/sizeof(uint32_t)];
