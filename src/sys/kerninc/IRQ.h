#ifndef __IRQ_H__
#define __IRQ_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <arch-kerninc/SaveArea.h>
#include <kerninc/StallQueue.h>
#include <arch-kerninc/IRQ-inline.h>

// Disable IRQ and return the old flags.
INLINE irqFlags_t
local_irq_save(void)
{
  return mach_local_irq_save();
}

// Restore flags saved by mach_local_irq_save.
INLINE void
local_irq_restore(irqFlags_t flags)
{
  mach_local_irq_restore(flags);
}

/* irq_DisableDepth

While the kernel is running, irq_DisableDepth has the net number of times
that the kernel has disabled IRQ.

Whenever the kernel is running and irq_DisableDepth is nonzero,
IRQ is disabled.

Whenever the kernel is running and irq_DisableDepth is zero,
IRQ is enabled or disabled the same as it is in the current process's CPSR.
Thus, a process with IRQ disabled can make a simple invocation of a
kernel capability without ever enabling IRQ, 
as long as the invocation doesn't Yield.

While the kernel is not running, irq_DisableDepth has 1.
(Logically, it would be 0, but it is not examined while the kernel
is not running.)

On an exception from a user process, IRQ is disabled by the exception,
and irq_DisableDepth is 1 as it should be.

An exception from the kernel usually is an interrupt.
In this case, obviously immediately before the interrupt,
IRQ was enabled and irq_DisableDepth was zero.
IRQ is disabled by the exception.
We set irq_DisableDepth to 1 on the exception 
and reset it to 0 before returning.
*/

#endif /* __IRQ_H__ */
