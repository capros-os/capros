#ifndef __IRQ_H__
#define __IRQ_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

#include <kerninc/Process-inline.h>
#include <arch-kerninc/SaveArea.h>
#include <kerninc/StallQueue.h>
#include <arch-kerninc/IRQ-inline.h>
#include <arch-kerninc/Process-inline.h>

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

/* When a process traps to the kernel, the IRQ interrupt is disabled
by the hardware.
When we are ready to enable IRQ interrupts again. call irq_ENABLE(). */
INLINE void
irq_ENABLE(void)
{
  Process * p = proc_Current();
  if (p && proc_HasIRQDisabled(p))
    return;	// If the interrupted process had IRQ disabled, keep it disabled
  raw_local_irq_enable();
}

/* When an IRQ interrupt occurs,
the IRQ interrupt, which was enabled, is disabled by the hardware.
When we are ready to enable IRQ interrupts again. call irq_ENABLE_for_IRQ(). */
INLINE void
irq_ENABLE_for_IRQ(void)
{
  raw_local_irq_enable();
}

/* Having called irq_ENABLE[_for_IRQ] to enable interrupts,
call irq_DISABLE() to disable interrupts before exiting the kernel. */
INLINE void
irq_DISABLE(void)
{
  raw_local_irq_disable();
}

#endif /* __IRQ_H__ */
