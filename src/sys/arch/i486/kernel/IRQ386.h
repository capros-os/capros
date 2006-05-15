#ifndef __IRQ486_H__
#define __IRQ486_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <arch-kerninc/SaveArea.h>
#include <arch-kerninc/IRQ-inline.h>

  /* Interrupt initialization */
void irq_SetHandler(uint32_t irq, InterruptHandler);
InterruptHandler irq_GetHandler(uint32_t irq);
void irq_UnsetHandler(uint32_t irq);

void irq_Enable(uint32_t irq);
void irq_Disable(uint32_t irq);

void irq_UnboundInterrupt(savearea_t *);

void DoUsermodeInterrupt(savearea_t *ia);

#endif /* __IRQ486_H__ */
