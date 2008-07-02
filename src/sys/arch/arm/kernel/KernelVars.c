/*
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/KernStats.h>

/* Here we gather some kernel variables.
   Keeping them together helps reduce their cache footprint. */

/* act_curActivity must be initialized (to 0) early, because during
initialization, we call malloc, which under some circumstances can
call act_Yield. We catch that problem by checking for a nonzero
value in act_curActivity. */
/* Eventually act_curActivity should be per-CPU. */
struct Activity * act_curActivity = 0;

/* proc_curProcess is always act_curActivity->context
   (or NULL if act_curActivity is NULL). */
struct Process * proc_curProcess = 0;

// A bool is sufficient, but an int is more efficient.
unsigned int act_yieldState = 0;

// A bool is sufficient, but an int is more efficient.
unsigned int timerWork = 0;

/* mapWork serves two purposes.
   1. It says whether the TLB needs to be flushed and/or the cache cleaned
      or invalidated. This allows us to defer, and thus possibly combine,
      TLB and cache operations. The procedure UpdateTLB()
      checks mapWork; if true, it does the required work and clears mapWork.
   2. It says whether some mapped memory may have been invalidated 
      during the dry run of some kernel operation.
   These two uses are compatible because UpdateTLB is only called right
   before returning to user mode. 
   If you are considering calling it elsewhere, because you want the TLB
   up to date, do this instead: if mapWork, act_Yield (which will retry
   the operation). 

   Bits in mapWork are defined in PTEarm.h.
 */
unsigned int mapWork = 0;

#ifdef OPTION_KERN_STATS
struct KernStats_s  KernStats;
#endif
