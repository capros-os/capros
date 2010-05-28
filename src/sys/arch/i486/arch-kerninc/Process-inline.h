#ifndef __MACHINE_PROCESS_INLINE_H__
#define __MACHINE_PROCESS_INLINE_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2010, Strawberry Development Group.
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

#include <eros/Invoke.h>
#include <kerninc/Invocation.h>
#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <arch-kerninc/Process.h>

/* Machine-specific inline helper functions for process operations: */

INLINE bool
proc_HasIRQDisabled(Process * p)
{
  return ! (p->trapFrame.EFLAGS & MASK_EFLAGS_Interrupt);
}

INLINE uint32_t
proc_GetRcvKeys(Process * thisPtr)
{
  return thisPtr->pseudoRegs.rcvKeys;
}

INLINE void 
proc_SetPC(Process* thisPtr, uint32_t oc)
{
  thisPtr->trapFrame.EIP = oc;
}

/* Called in the IPC path to back up the PC to point to the invocation
 * trap instruction. */
INLINE void 
proc_AdjustInvocationPC(Process* thisPtr)
{
  thisPtr->trapFrame.EIP -= 2;
}

/* and this advances it: */
INLINE void 
proc_AdvancePostInvocationPC(Process * thisPtr)
{
  thisPtr->trapFrame.EIP += 2;
}

INLINE uint32_t 
proc_GetPC(Process* thisPtr)
{
  return thisPtr->trapFrame.EIP;
}

INLINE void 
proc_SetInstrSingleStep(Process* thisPtr)
{
  thisPtr->hazards |= hz_SingleStep;
}


#endif /* __MACHINE_PROCESS_INLINE_H__ */
