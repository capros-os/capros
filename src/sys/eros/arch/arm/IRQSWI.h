#ifndef __MACHINE_IRQSWI_H__
#define __MACHINE_IRQSWI_H__
/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/*
 * Functions to support IRQ enabling and disabling.
   To execute these, the process must have I/O privileges
   (DevicePrivs in ProcIoSpace).
 */

#include <eros/target.h>

/* Disable the IRQ interrupt.
   Returns the old value of CPSR.  */

INLINE uint32_t
capros_irq_disable(void)
{
  register uint32_t r0 __asm__("r0");
  __asm__ __volatile__ ("swi 6"	// SWI_DisableIRQ
         : "=r" (r0)
         : );
  return r0;
}

/* Enable the IRQ interrupt. */

INLINE void
capros_irq_enable(void)
{
  __asm__ __volatile__ ("swi 7"	// SWI_EnableIRQ
         : : );
}

/* Set the IRQ interrupt enable from a CPSR value. */

INLINE void
capros_irq_put(uint32_t psr)
{
  register uint32_t r0 __asm__("r0") = psr;
  __asm__ __volatile__ ("swi 5"	// SWI_PutIRQ
         : "=r" (r0)	// this SWI may clobber r0
         : "0" (r0)	// input in r0
         );
}

#endif /* __MACHINE_IRQSWI_H__ */
