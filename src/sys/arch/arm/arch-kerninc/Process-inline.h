#ifndef __MACHINE_PROCESS_INLINE_H__
#define __MACHINE_PROCESS_INLINE_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <arch-kerninc/Process.h>

/* Machine-specific inline helper functions for process operations: */

INLINE void 
proc_SetPC(Process* thisPtr, uint32_t oc)
{
  thisPtr->trapFrame.r15 = oc;
}

/* Called in the IPC path to back up the PC to point to the invocation
 * trap instruction. */
INLINE void 
proc_AdjustInvocationPC(Process* thisPtr)
{
  thisPtr->trapFrame.r15 -= 
    (thisPtr->trapFrame.CPSR & MASK_CPSR_Thumb) ? 2 : 4;
#if 0
  dprintf(false,"Backing up pc, proc=0x%08x pc now 0x%x\n",
          thisPtr, thisPtr->trapFrame.r15);
#endif
}

/* and this advances it: */
INLINE void
proc_AdvancePostInvocationPC(Process * thisPtr)
{
#if 0
  dprintf(false,"Advancing pc, proc=0x%08x pc was 0x%x\n",
          thisPtr, thisPtr->trapFrame.r15);
#endif
  thisPtr->trapFrame.r15 += 
    (thisPtr->trapFrame.CPSR & MASK_CPSR_Thumb) ? 2 : 4;
}

INLINE uint32_t 
proc_GetPC(Process* thisPtr)
{
  return thisPtr->trapFrame.r15;
}

INLINE void 
proc_SetInstrSingleStep(Process* thisPtr)
{
  printf("proc_SetInstrSingleStep not supported\n");
}

#endif /* __MACHINE_PROCESS_INLINE_H__ */
