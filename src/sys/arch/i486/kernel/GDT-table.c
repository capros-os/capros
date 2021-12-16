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
#include <arch-kerninc/kern-target-asm.h>
#include "GDT.h"
#include "TSS.h"
#include <eros/arch/i486/pseudoregs.h>

#define GDTDescriptor(type, dpl, base, gran, limit) \
  (((base) & 0xffff)<<16) + ((limit) & 0xffff), \
  ((base) & 0xff000000) \
  + (gran) \
  + (((type) & 0x10) << 18) /* d/b same as s */ \
  /* L and AVL always zero */ \
  + ((limit) & 0xf0000) \
  + 0x8000	/* always P */ \
  + ((dpl) << 13) \
  + ((type) << 8) /* type includes the S bit */ \
  + (((base) & 0xff0000) >> 16)

#define GDTDescriptorByte(type, dpl, base, size) \
  GDTDescriptor(type, dpl, base, 0, (size)-1)
#define GDTDescriptorPage(type, dpl, base, size) \
  GDTDescriptor(type, dpl, base, 0x800000, ((size)>>12)-1)

enum GDTDescriptorType { /* includes the S bit */
  DescTSS32 = 0x09,	// 32-bit TSS (Available)
  DescData = 0x13,	// Data Read/Write, accessed
  DescCode = 0x1b,	// Code Execute/Read, accessed
};

uint32_t gdt_GdtTable[GDT_ENTRIES*2] = {
	/* Entry 0 - Null Segment */
  0x0, 0x0,

	/* Entry 1 - Kernel text */
  GDTDescriptorPage(DescCode, 0, KVA, 0 - KVA),

	/* Entry 2 - Kernel data/stack */
  GDTDescriptorPage(DescData, 0, KVA, 0x00000000 /* 4GB */),
	// note, limit wraps

	/* Entry 3 - TSS descriptor for current domain. */
  GDTDescriptorByte(DescTSS32, 0, 0x00000000, sizeof(i386TSS)),

	/* Entry 4 - Domain code */
  GDTDescriptorPage(DescCode, 3, 0x00000000, UMSGTOP),
	// On dispatch, base and limit get set for small/large space

	/* Entry 5 - Domain data */
  GDTDescriptorPage(DescData, 3, 0x00000000, UMSGTOP),
	// On dispatch, base and limit get set for small/large space

	/* Entry 6 - Domain Pseudo Regs */
  GDTDescriptorByte(DescData, 3, 0, sizeof(pseudoregs_t)),
	// On dispatch, base gets set

	/* Entry 7 - kernel process text */
  GDTDescriptorPage(DescCode, 1, KVA, 0 - KVA),

	/* Entry 8 - Kernel process data/stack */
  GDTDescriptorPage(DescData, 1, KVA, 0x00000000 /* 4GB */),
	// note, limit wraps
};
