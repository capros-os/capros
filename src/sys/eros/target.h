#ifndef __TARGET_H__
#define __TARGET_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, 2008, Strawberry Development Group.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/*
 * Basic type declarations for the target platform, used pervasively
 * within the kernel code and the volume construction utilities.
 *
 * This file is included from both C and C++ code, so it needs
 * to be handled carefully.
 */

#include <stdbool.h>
#include <stdint.h>

#ifndef __GNUC__
#error "This code uses GCC extensions. It is bloody unlikely to compile without GCC"
#endif

/* INLINE is also defined in /usr/include/bfd.h, but that isn't the 
   one we want. If INLINE is defined, undef it here before redefining
   it. */
#ifdef INLINE
#undef INLINE
#endif

#ifdef __cplusplus
#define INLINE inline
#else
#define INLINE static inline
#endif

#define BoolToBit(x) ((x) != 0)
#define BOOL(x) ((x) != 0)	// older name

#include "target-asm.h"

#ifdef __ASSEMBLER__
#error "This file should no longer be included by assembly language code"
#endif

#include "machine/target.h"

typedef uint32_t cap_t;
typedef fixreg_t result_t;

#endif /* __TARGET_H__ */
