#ifndef __KERN_TARGET_ASM_I486_H__
#define __KERN_TARGET_ASM_I486_H__

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

#ifdef __KERNEL__

/* The addresses starting at KVA_FSTBUF are for the fast path to map
 * the recipient page DIRECTORY. Two slots might be needed, so this
 * occupies 3F000000 through 3F400000 (inclusive).
 *
 * The mappings starting at KVA_PTEBUF are used to map the receive
 * buffers AS PAGES. The Machine.cxx setup code ensures that there is
 * a valid page directory at this location.
 *
 * The mappings starting at KVA_BZEROBUF are used for zeroing pages
 * that are not currently mapped into the kernel. The page is
 * temporarily mapped at this address, zeroed, and then the mapping is
 * released. There are multiple mappings here. For a given processor,
 * the mapping used is the mapping at (KVA_ZEROBUF + CPU_NO * EROS_PAGE_SIZE)
 */
#ifdef OPTION_SMALL_SPACES

/* Reserve 0.25 Gbytes of address space from 0xC000000..0xCFFFFFF for
   use as small spaces */
#define KVA		_ASM_U(0xD0000000)
#define KUVA		_ASM_U(0x30000000) /* user va 0 as kernel sees it */

#define KVA_FROMSPACE   _ASM_U(0x2e800000) /* kernel-readable page tables */
#define KVA_TOSPACE     _ASM_U(0x2ec00000)
#define KVA_FSTBUF	_ASM_U(0x2f000000) /* top kva - 4096 pages.  Used 
					   to map page *directories* */
#define KVA_BZEROBUF	_ASM_U(0x2f800000) /* top kva - 2048 pages */
#define KVA_PTEBUF	_ASM_U(0x2fc00000) /* top kva - 1024 pages */
#define SMALL_SPACE_PAGES 32         /* 128 Kbytes */

#else /* OPTION_SMALL_SPACES */

#define KVA		_ASM_U(0xC0000000)
#define KUVA		_ASM_U(0x40000000) /* user va 0 as kernel sees it */

#define KVA_FROMSPACE   _ASM_U(0x3e800000) /* kernel-readable page tables */
#define KVA_TOSPACE     _ASM_U(0x3ec00000)
#define KVA_FSTBUF	_ASM_U(0x3f000000) /* top kva - 4096 pages.  Used 
					   to map page *directories* */
#define KVA_BZEROBUF	_ASM_U(0x3f800000) /* top kva - 2048 pages */
#define KVA_PTEBUF	_ASM_U(0x3fc00000) /* top kva - 1024 pages */

#endif /* OPTION_SMALL_SPACES */

#define UMSGTOP         _ASM_U(0xC0000000)
#define LARGE_SPACE_PAGES 0xC0000         /* 3 Gbytes */

#define KERNPBASE	_ASM_U(0x00100000) /* phys addr where kernel is loaded */
#endif

#endif /* __KERN_TARGET_ASM_I486_H__ */
