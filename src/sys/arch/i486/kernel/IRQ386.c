/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#include <kerninc/kernel.h>
#include <kerninc/IRQ.h>
#include "IDT.h"
#include <eros/i486/io.h>

uint8_t pic1_mask = 0xffu;
uint8_t pic2_mask = 0xffu;

uint32_t irq_DisableDepth = 1;	/* interrupts disabled on kernel entry */
uint32_t irq_enableMask = 0;


/* #define INTRDEBUG */

#if 0
/* The X86 PIC uses disable mask rather than enable mask.  We use
 * enable logic in the kernel because it is easier to initialize
 * things that way, so...
 */
uint32_t
IRQ::GetEnableMaskFromPIC()
{
  uint32_t disableMask = pic2_mask;
  disableMask <<= 8;
  disableMask |= pic1_mask;

  return ~disableMask;
}
#endif

void
irq_Enable(uint32_t irq)
{
  irq_enableMask |= (1u << irq);

  irq_DISABLE();

  if (irq >= 8) {
    pic2_mask &= ~(1u << (irq-8));
    old_outb(0xa1, pic2_mask);
    /* Make sure that the cascade entry on PIC1 is enabled as well (I
     * got caught by this at one point)
     */
    if (pic1_mask & (1u << irq_Cascade))
      irq = irq_Cascade;
  }
  
  if (irq < 8) {
    pic1_mask &= ~(1u << irq);
    old_outb(0x21, pic1_mask);
  }

#ifdef INTRDEBUG
  if (irq == 1) {
    uint8_t truemask = in8(0x21);
  
    printf("Enable IRQ line %d true=0x%x pic=0x%x\n", irq,
		   truemask, pic1_mask);
  }
#endif

  irq_ENABLE();
}

void
irq_Disable(uint32_t irq)
{
  irq_enableMask &= ~(1u << irq);

  irq_DISABLE();

  if (irq < 8) {
    pic1_mask |= (1u << irq);
    old_outb(0x21, pic1_mask);
  }
  else {
    pic2_mask |= (1u << (irq-8));
    old_outb(0xa1, pic2_mask);
  }

#ifdef INTRDEBUG
  if (irq == 1) printf("Disable IRQ line %d\n", irq);
#endif

  irq_ENABLE();
}

