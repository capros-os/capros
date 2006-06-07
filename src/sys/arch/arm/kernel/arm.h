#ifndef __ARM_H__
#define __ARM_H__
/*
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

#include <eros/machine/target-asm.h>

/* Modes in the CPSR */
#define CPSRMode_User       0x10	// 0b10000
#define CPSRMode_FIQ        0x11	// 0b10001
#define CPSRMode_IRQ        0x12	// 0b10010
#define CPSRMode_Supervisor 0x13	// 0b10011
#define CPSRMode_Abort      0x17	// 0b10111
#define CPSRMode_Undefined  0x1b	// 0b11011
#define CPSRMode_System     0x1f	// 0b11111

#define MASK_CPSR_Thumb             0x00000020
#define MASK_CPSR_FIQDisable        0x00000040
#define MASK_CPSR_IRQDisable        0x00000080
#define MASK_CPSR_Q                 0x08000000
#define MASK_CPSR_Overflow          0x10000000
#define MASK_CPSR_Carry             0x20000000
#define MASK_CPSR_Zero              0x40000000
#define MASK_CPSR_Sign              0x80000000

/* Size of each process block in the Fast Context Switch Extension */
#define FCSE_Range 0x02000000

#ifndef __ASSEMBLER__
#endif /* __ASSEMBLER__  */
#endif /* __ARM_H__ */
