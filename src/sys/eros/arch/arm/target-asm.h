#ifndef __TARGET_ASM_ARM_H__
#define __TARGET_ASM_ARM_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

/*
 * Basic type declarations for the target platform, used pervasively
 * within the kernel code and the volume construction utilities.
 *
 * This file is included from Assembler, C and C++ code, 
   and from both host and target code, so it needs
 * to be handled very carefully.  In particular, parentheses mean
 * something in assembler and should be avoided.
 */

#if !(defined(EROS_TARGET_arm))
#error "Inappropriate target file included"
#endif

#if 0
#define EROS_KSTACK_SIZE	0x2000 /* two pages */
#else
#define EROS_KSTACK_SIZE	0x1000 /* one page */
#endif

#define EROS_PAGE_SIZE		0x1000
#define EROS_PAGE_LGSIZE        12
#define EROS_PAGE_ADDR_BITS     EROS_PAGE_LGSIZE
#define EROS_PAGE_MASK          (EROS_PAGE_SIZE-1)

#define EROS_MESSAGE_LIMIT	0x10000
#define EROS_SECTOR_SIZE	512
#define EROS_PAGE_SECTORS	8 /* Page_size / sector_size */

#define EROS_PAGE_BLSS 		0

#define EROS_NODE_SIZE		0x20
#define EROS_NODE_LGSIZE	0x5
#define EROS_NODE_SLOT_MASK	0x1f
#define EROS_PROCESS_KEYREGS    32

/* The following should be a multiple of EROS_PAGE_SECTORS */
#define DISK_BOOTSTRAP_SECTORS	64

#define EROS_ADDRESS_BITS	32
#define EROS_FIXREG_BITS	32 /* size of native fixreg */


/* Following MUST be a power of 2!!! */
#define EROS_OBJECTS_PER_FRAME  256

/* This is for 32 slot nodes and 64 bit address spaces */
/* EROS_PAGE_BLSS == 0 */
#define MAX_BLSS       11	/* 2^96 byte space */
#define MAX_RED_BLSS   12	/* above + keeper */
#define EROS_ADDRESS_BLSS	4

#define EROS_ADDRESS_LSS (EROS_ADDRESS_BLSS - EROS_PAGE_BLSS)

#endif /* __TARGET_ASM_ARM_H__ */
