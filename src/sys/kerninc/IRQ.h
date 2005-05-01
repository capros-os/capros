#ifndef __IRQ_H__
#define __IRQ_H__
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

#include <arch-kerninc/SaveArea.h>

/*struct fixregs_t;*/
typedef void (*InterruptHandler)(savearea_t*);


#include <arch-kerninc/IRQ-inline.h>

/* Former member functions of IRQ */

INLINE bool 
irq_INTERRUPTS_ENABLED()
{
  return (irq_DisableDepth == 0) ? true : false;
}

INLINE uint32_t 
irq_DISABLE_DEPTH()
{
  return irq_DisableDepth;
}

INLINE bool 
irq_InterruptsAreEnabled()
{
  return (irq_DisableDepth ==0 ) ? true : false;
}

  /* Interrupt initialization */
void irq_SetHandler(uint32_t irq, InterruptHandler);
InterruptHandler irq_GetHandler(uint32_t irq);
void irq_UnsetHandler(uint32_t irq);

void irq_Enable(uint32_t irq);
void irq_Disable(uint32_t irq);

INLINE bool 
irq_IsEnabled(uint32_t irq)
{
  return (irq_enableMask & (1u << irq)) ? true : false;
}

void irq_UnboundInterrupt(savearea_t *);

#endif /* __IRQ_H__ */
