#ifndef __KERN_TARGET_ASM_I486_H__
#define __KERN_TARGET_ASM_I486_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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

/*
 * Basic type declarations for the target platform, used pervasively
 * within the kernel code.
 *
 * This file is included from Assembler, C and C++ code, so it needs
 * to be handled very carefully.  In particular, parentheses mean
 * something in assembler and should be avoided.
 */

#if !(defined(i386) || defined(i486))
#error "Inappropriate target file"
#endif

/* Layout of the linear address space (virtual address space). */

/* Addresses from 0 to UMSGTOP are for a large space. */
#define LARGE_SPACE_PAGES 0xC0000         /* 3 Gbytes */
#define UMSGTOP         0xC0000000

/* Addresses above UMSGTOP are reserved for small spaces and the kernel. */

#ifdef OPTION_SMALL_SPACES

/* At UMSGTOP is the address space for small spaces.
The amount of space is KTUNE_NCONTEXT * SMALL_SPACE_PAGES * EROS_PAGE_SIZE. */
#define SMALL_SPACE_PAGES 32         /* 128 Kbytes */

#define KVA		0xD0000000 /* physical memory is mapped at this
			linear address, from physical address 0 to
			physMem_PhysicalPageBound. */
#define KUVA		0x30000000 /* user va 0 as kernel sees it */

#define KVA_FROMSPACE   0x2e800000 /* kernel-readable page tables */
#define KVA_TOSPACE     0x2ec00000
#define KVA_FSTBUF	0x2f000000 /* reserved */
/* The addresses starting at KVA_FSTBUF are for the fast path to map
 * the recipient page DIRECTORY. Two slots might be needed, so this
 * occupies 0x400000 of address space.
 */
#define KVA_BZEROBUF	0x2f800000 /* reserved */
/* The mappings starting at KVA_BZEROBUF will be used for zeroing pages
 * that are not currently mapped into the kernel. The page is
 * temporarily mapped at this address, zeroed, and then the mapping is
 * released. There are multiple mappings here. For a given processor,
 * the mapping used is the mapping at (KVA_ZEROBUF + CPU_NO * EROS_PAGE_SIZE)
 */
#define KVA_PTEBUF	0x2fc00000 /* used to map receive buffers */

#else /* OPTION_SMALL_SPACES */

#define KVA		0xC0000000
#define KUVA		0x40000000 /* user va 0 as kernel sees it */

#define KVA_FROMSPACE   0x3e800000
#define KVA_TOSPACE     0x3ec00000
#define KVA_FSTBUF	0x3f000000
#define KVA_BZEROBUF	0x3f800000
#define KVA_PTEBUF	0x3fc00000

#endif /* OPTION_SMALL_SPACES */

/*
At KVA: physical RAM is mapped here beginning at physical address 0.
At KVA + physMem_PhysicalPageBound: nothing, padding to 4MB boundary
Then: the kernel heap (a whole number of pages)
Then: proc_ContextCacheRegion.
 */

#define KERNPBASE	0x00100000 /* phys addr where kernel is loaded */

#endif /* __KERN_TARGET_ASM_I486_H__ */
