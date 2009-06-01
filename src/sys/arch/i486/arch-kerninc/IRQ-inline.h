#ifndef __IRQ_INLINE_H__
#define __IRQ_INLINE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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

#include <arch-kerninc/SaveArea.h>
#include <kerninc/StallQueue.h>

typedef void (*InterruptHandler)(savearea_t*);

struct UserIrq {
  bool       isPending;	/* only valid if isAlloc */
  bool       isAlloc;
  StallQueue sleepers;
};
extern struct UserIrq UserIrqEntries[NUM_HW_INTERRUPT];

INLINE void
raw_local_irq_disable(void)
{
  GNU_INLINE_ASM ("cli");
}

INLINE void
raw_local_irq_enable(void)
{
  GNU_INLINE_ASM ("sti");
}

typedef unsigned long irqFlags_t;

INLINE irqFlags_t
raw_local_get_flags(void)
{
  irqFlags_t ret;
  __asm__ __volatile__(
        "pushfl ; popl %0"
  : "=g" (ret) : );
  return ret;
}

INLINE bool
local_irq_disabled(void)
{
  return ! (raw_local_get_flags() & MASK_EFLAGS_Interrupt);
}

// Disable IRQ and return the old flags.
INLINE irqFlags_t
mach_local_irq_save(void)
{
  irqFlags_t ret = raw_local_get_flags();
  raw_local_irq_disable();
  return ret;
}

// Restore flags saved by mach_local_irq_save.
INLINE void
mach_local_irq_restore(irqFlags_t flags)
{
  __asm__ __volatile__(
	"pushl %0 ; popfl"
  :
  : "g" (flags)
  : "memory", "cc" );
}

#endif // __IRQ_INLINE_H__
