#ifndef __KERN_TARGET_ARM_H__
#define __KERN_TARGET_ARM_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2008, 2009, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

/* Basic type declarations for the target platform, used pervasively
   within the kernel code.
 
   This file is included from both C and C++ code, so it needs to be
   handled carefully.  */

#if !(defined(__arm__))
#error "Wrong target file included"
#endif

#include <inttypes.h>

extern const char _start,	/* start of kernel text section */
  _etext,			/* end of kernel (text and rodata) */
  __data_start,			/* start of kernel data section */
  _edata,			/* end of kernel data section and
				beginning of bss */
  _end;				/* end of bss */

typedef uint32_t        kva_t;	/* kernel virtual address */
typedef uint32_t	kpa_t;	/* physical address */
#define PRIxkpa PRIx32
typedef uint32_t	kpg_t;	/* a physical page number
				(physical address / EROS_PAGE_SIZE) */
typedef uint32_t	kpsize_t; /* kernel physical address range
				   * size */
#define PRIxkpsize PRIx32
typedef uint32_t        uva_t;	/* user (unmodified) virtual address */
typedef uint32_t        ula_t;	/* user modified virtual address */

/* Convert kernel virtual address to physical address. */
INLINE kpa_t
VTOP(kva_t va)
{
  return va - PhysMapVA + FirstRAMAddr;
}

/* Convert physical address to kernel virtual address. */
INLINE kva_t
PTOV(kpa_t pa)
{
  return pa - FirstRAMAddr + PhysMapVA;
}

/* (Kernel) Physical Address to Pointer (typed kernel virtual address) */
#define KPAtoP(ty,x) ((ty) (PTOV(x)))

#endif /* __KERN_TARGET_ARM_H__ */
