#ifndef __IRQ_INLINE_H__
#define __IRQ_INLINE_H__
/*
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

#include "kernel/arm.h"

typedef unsigned long irqFlags_t;

INLINE void
raw_local_irq_disable(void)
{
  irqFlags_t temp;
  __asm__ __volatile__(
	"mrs %0,cpsr\n"
"	orr %0,%0,#0x80\n"
"	msr cpsr_c,%0"
  : "=r" (temp)
  :
  : "memory", "cc" );
}

INLINE void
raw_local_irq_enable(void)
{
  irqFlags_t temp;
  __asm__ __volatile__(
	"mrs %0,cpsr\n"
"	bic %0,%0,#0x80\n"
"	msr cpsr_c,%0"
  : "=r" (temp)
  :
  : "memory", "cc" );
}

INLINE bool
local_irq_disabled(void)
{
  irqFlags_t temp;
  __asm__ __volatile__(
	"mrs %0,cpsr"
  : "=r" (temp)
  :
  : "memory" );
  return temp & MASK_CPSR_IRQDisable;
}

// Disable IRQ and return the old flags.
INLINE irqFlags_t
mach_local_irq_save(void)
{
  irqFlags_t ret,temp;
  __asm__ __volatile__(
	"mrs %0,cpsr\n"
"	orr %1,%0,#0x80\n"
"	msr cpsr_c,%1"
  : "=r" (ret), "=r" (temp)
  :
  : "memory", "cc" );
  return ret;
}

// Restore flags saved by mach_local_irq_save.
INLINE void
mach_local_irq_restore(irqFlags_t flags)
{
  __asm__ __volatile__(
	"msr cpsr_c,%0"
  :
  : "r" (flags)
  : "memory", "cc" );
}

#endif // __IRQ_INLINE_H__
