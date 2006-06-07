#ifndef __MACHINE_PROCESS_INLINE_H__
#define __MACHINE_PROCESS_INLINE_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <arch-kerninc/Process.h>

/* Machine-specific inline helper functions for process operations: */

INLINE void 
proc_SetPC(Process* thisPtr, uint32_t oc)
{
  thisPtr->trapFrame.r15 = oc;
}

/* Called before AdjustInvocationPC() to capture the address of the
 * next instruction to run if the invocation is successful.
 */
INLINE uint32_t 
proc_CalcPostInvocationPC(Process* thisPtr)
{
  return thisPtr->trapFrame.r15;
}

/* Called in the IPC path to reset the PC to point to the invocation
 * trap instruction...
 */
INLINE void 
proc_AdjustInvocationPC(Process* thisPtr)
{
  thisPtr->trapFrame.r15 -= 
    (thisPtr->trapFrame.CPSR & MASK_CPSR_Thumb) ? 2 : 4;
}

INLINE uint32_t 
proc_GetPC(Process* thisPtr)
{
  return thisPtr->trapFrame.r15;
}

INLINE void 
proc_ClearNextPC(Process* thisPtr)
{
  thisPtr->nextPC = 0xffffffff;	/* hopefully this PC value will trap if used */
}

INLINE void 
proc_SetInstrSingleStep(Process* thisPtr)
{
  printf("proc_SetInstrSingleStep not supported\n");
}

#endif /* __MACHINE_PROCESS_INLINE_H__ */
