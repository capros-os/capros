#ifndef __TARGET_ASM_I486_H__
#define __TARGET_ASM_I486_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#ifndef __STDKEYTYPE_H__
#include <eros/StdKeyType.h>
#endif

/*
 * Basic type declarations for the target platform, used pervasively
 * within the kernel code and the volume construction utilities.
 *
 * This file is included from Assembler, C and C++ code, so it needs
 * to be handled very carefully.  In particular, parentheses mean
 * something in assembler and should be avoided.
 */

#if !(defined(i386) || defined(i486))
#error "Inappropriate target file"
#endif

#ifndef _ASM_U
#define _ASM_U(x) x##u
#endif

#define EROS_REG_MAX  UINT32_MAX
#define EROS_SREG_MAX INT32_MAX
#define EROS_SREG_MIN INT32_MIN
#define EROS_REG_BITS UINT32_BITS

#if 0
#define EROS_KSTACK_SIZE	_ASM_U(0x2000) /* two pages */
#define EROS_KSTACK_MASK	_ASM_U(0xffffe000)
#else
#define EROS_KSTACK_SIZE	_ASM_U(0x1000) /* two pages */
#define EROS_KSTACK_MASK	_ASM_U(0xfffff000)
#endif

#define EROS_PAGE_SIZE		_ASM_U(0x1000)
#define EROS_MESSAGE_LIMIT	_ASM_U(0x10000)
#define EROS_SECTOR_SIZE	512
#define EROS_PAGE_SECTORS	8 /* Page_size / sector_size */

#define EROS_PAGE_ADDR_BITS 12
#define EROS_PAGE_MASK _ASM_U(0xfff)
#define EROS_L0ADDR_MASK _ASM_U(0x3fffff)

/* Trying to get this down to 0... */
#define EROS_PAGE_BLSS 		0

#define EROS_NODE_SIZE		_ASM_U(0x20)
#define EROS_NODE_LGSIZE	_ASM_U(0x5)
#define EROS_NODE_SLOT_MASK	_ASM_U(0x1f)
#define EROS_PROCESS_KEYREGS    _ASM_U(32)

/* The following should be a multiple of EROS_PAGE_SECTORS */
#define DISK_BOOTSTRAP_SECTORS	64

#define EROS_ADDRESS_BITS	32
#define EROS_FIXREG_BITS	32 /* size of native fixreg */


/* Following MUST be a power of 2!!! */
#define EROS_OBJECTS_PER_FRAME  256

#define EROS_NODES_PER_FRAME	7

/* This is for 32 slot nodes and 64 bit address spaces */
#if EROS_PAGE_BLSS == 0
#define MAX_BLSS       11	/* 2^96 byte space */
#define MAX_RED_BLSS   12	/* above + keeper */
#define EROS_ADDRESS_BLSS	4
#elif EROS_PAGE_BLSS == 1
#define MAX_BLSS       12	/* 2^96 byte space */
#define MAX_RED_BLSS   13	/* above + keeper */
#define EROS_ADDRESS_BLSS	5
#elif EROS_PAGE_BLSS == 2
#define MAX_BLSS       13	/* 2^96 byte space */
#define MAX_RED_BLSS   14	/* above + keeper */
#define EROS_ADDRESS_BLSS	6
#else
#error "Bad page BLSS value!"
#endif

#define EROS_ADDRESS_LSS (EROS_ADDRESS_BLSS - EROS_PAGE_BLSS)

#ifdef __KERNEL__

/* This must be defined here for the benefit of the bootstrap code,
   which must agree with the kernel about the loaded virtual address.
   Actually, this is no longer really necessary, and we should perhaps
   consider removing that dependency from the bootstrap code.

   2/3/98 -- the bootstrap no longer knows or cares about kernel
   virtual address.  It attempts (minimally) to honor the load
   address, but otherwise pays no attention to kernel binary
   addresses. */

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

/* #undef KVA_PTEBUF */
#endif /* __TARGET_ASM_I486_H__ */
