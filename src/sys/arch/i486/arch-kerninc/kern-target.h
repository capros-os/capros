#ifndef __KERN_TARGET_I486_H__
#define __KERN_TARGET_I486_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2008, 2009, Strawberry Development Group.
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

/* Basic type declarations for the target platform, used pervasively
   within the kernel code.
 
   This file is included from both C and C++ code, so it needs to be
   handled carefully.  */

#if !(defined(i386) || defined(i486))
#error "Wrong target file included"
#endif

#include <inttypes.h>

extern const char _start,	/* start of kernel text section */
  etext,			/* end of kernel (text and rodata) */
  end;				/* end of kernel (data and bss) */

#define MAPPING_ENTRIES_PER_PAGE 1024

typedef uint32_t	klva_t;	/* kernel linear virtual address */
typedef uint32_t        kva_t;	/* kernel virtual address */
typedef uint64_t	kpa_t;	/* kernel physical address */
#define PRIxkpa PRIx64
/* Change kpg_t to uint64_t to support more than 2**44 bytes of
physical memory. */
typedef uint32_t	kpg_t;	/* a physical page number
				(physical address / EROS_PAGE_SIZE) */
typedef uint64_t	kpsize_t; /* kernel physical address range
				   * size */
#define PRIxkpsize PRIx64
typedef uint32_t        uva_t;	/* user logical (virtual) address */
typedef uint32_t        ula_t;	/* user linear address */

/* On most architectures, kpmap_t == kpa_t, but on the x86, the
   processor may be in PAE mode, in which case physical addresses are
   extended to 36 bits, but the physical addresses of mapping
   directories must still be representable within a 32 bit value. */
typedef uint32_t        kpmap_t; /* mapping table physical address */

/* The current version of the x86 kernel uses the segment mechanism to
   remap the kernel.  We will want it that way for the windowing
   tricks later anyway.

   On the plus side, this means that a kernel virtual address is also
   a kernel physical address. On the minus side, this means that
   domain virtual addresses must now be referenced through the domain
   segment selector. */

/* There is a portability problem with casting pointers to kpa_t if
 * they are of different size. The compiler complains. This impedes
 * portability, and it's a REALLY dumb idea. Therefore, you should
 * adjust the following macro according to whether
 *
 *     sizeof(kpa_t)==sizeof(void*)
 *
 * or not:
 */
/* If sizeof(kpa_t)!=sizeof(void*) */
/* Convert kernel virtual address to physical address. */
#define VTOP(va) ((kpa_t) (kva_t) (va))
/* Convert physical address to kernel virtual address. */
#define PTOV(pa) ((kva_t) (pa))
#define KVTOL(kva) (kva + KVA)
#define PtoKPA(x) ((kpa_t)(unsigned long) (x))
#define VtoKVA(x) ((kva_t)(unsigned long) (x))
#define KVAtoV(ty,x) ((ty)(unsigned long) (x))
/* Otherwise: */
/* #define PTOV(pa) (pa) */

/* (Kernel) Physical Address to Pointer (typed kernel virtual address) */
#define KPAtoP(ty,x) ((ty) (PTOV(x)))

#endif /* __KERN_TARGET_I486_H__ */
