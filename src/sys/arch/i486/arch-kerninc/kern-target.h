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

#ifdef __KERNEL__

#define USES_MAPPING_PAGES
#define MAPPING_ENTRIES_PER_PAGE 1024

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

#endif /* __KERNEL__ */

#if defined(__EROS__) && defined(__KERNEL__)
typedef unsigned int   size_t;	/* should be 32 bits */
#endif

#ifdef __KERNEL__
#define IRQ_FROM_EXCEPTION(vector) ((vector) - 0x20u)
#endif

#endif /* __KERN_TARGET_I486_H__ */
