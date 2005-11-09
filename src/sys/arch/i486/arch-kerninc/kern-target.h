#ifndef __KERN_TARGET_I486_H__
#define __KERN_TARGET_I486_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
 *
 * This file is part of the EROS Operating System runtime library.
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

/* Basic type declarations for the target platform, used pervasively
   within the kernel code.
 
   This file is included from both C and C++ code, so it needs to be
   handled carefully.  */

#if !(defined(i386) || defined(i486))
#error "Wrong target file included"
#endif

#define USES_MAPPING_PAGES
#define MAPPING_ENTRIES_PER_PAGE 1024

typedef uint32_t	io_t;	/* io address */
typedef uint32_t	klva_t;	/* kernel linear virtual address */
typedef uint32_t        kva_t;	/* kernel virtual address */
typedef uint64_t	kpa_t;	/* kernel physical address */
typedef uint64_t	kpsize_t; /* kernel physical address range
				   * size */
typedef uint32_t        uva_t;	/* user virtual address */
typedef uint32_t        ula_t;	/* user virtual address */

/* On most architectures, kpmap_t == kpa_t, but on the x86, the
   processor may be in PAE mode, in which case physical addresses are
   extended to 36 bits, but the physical addresses of mapping
   directories must still be representable within a 32 bit value. */
typedef uint32_t        kpmap_t; /* mapping table physical address */

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
#define PtoKPA(x) ((kpa_t)(unsigned long) (x))
#define KPAtoP(ty,x) ((ty)(unsigned long) (x))
#define VtoKVA(x) ((kva_t)(unsigned long) (x))
#define KVAtoV(ty,x) ((ty)(unsigned long) (x))
/* Otherwise: */
/* #define KPAtoP(ty,x) ((ty) (x)) */

/* The current version of the x86 kernel uses the segment mechanism to
   remap the kernel.  We will want it that way for the windowing
   tricks later anyway.

   On the plus side, this means that a kernel virtual address is also
   a kernel physical address. On the minus side, this means that
   domain virtual addresses must now be referenced through the domain
   segment selector. */
#define VTOP(va) ((uint32_t) (va))
#define PTOV(pa) ((uint32_t) (pa))
#define KVTOL(kva) (kva + KVA)

#if defined(__EROS__)
typedef unsigned int   size_t;	/* should be 32 bits */
#endif

#define IRQ_FROM_EXCEPTION(vector) ((vector) - 0x20u)

#endif /* __KERN_TARGET_I486_H__ */
