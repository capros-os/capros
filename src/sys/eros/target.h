#ifndef __TARGET_H__
#define __TARGET_H__

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

/*
 * Basic type declarations for the target platform, used pervasively
 * within the kernel code and the volume construction utilities.
 *
 * This file is included from both C and C++ code, so it needs
 * to be handled carefully.
 */

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

/*#if CONVERSION*/
#ifndef __cplusplus
/* FIX: This is a temporary expedient. We need to add stdbool.h to the 
   system header file tree. We also need to switch over to using
   stdint.h throughout. -- shap */

/* FIX (again): Looks like ANSI screwed up big time, in that
   their _Bool type as defined is not size-compatible with the
   corresponding C++ bool type. Somebody ought to rant at them
   about this. As a result, we can't use the ANSI type until we
   are completely done with C++. */

#ifndef bool

#define false 0
#define true 1

typedef unsigned char bool;

#endif

#endif

#define BOOL(x) ((x) ? true : false)

/*#endif CONVERSION*/

#include "target-asm.h"

#ifdef __ASSEMBLER__
#error "This file should no longer be included by assembly language code"
#endif

#include "machine/target.h"

#define OID_RESERVED_PHYSRANGE 0xff00000000000000ull
#define EROS_FRAME_FROM_OID(oid) (oid & ~(EROS_OBJECTS_PER_FRAME-1))
#define EROS_OBNDX_IN_FRAME(oid) (oid & (EROS_OBJECTS_PER_FRAME-1))

#ifdef __KERNEL__
/* If sizeof(kpa_t)!=sizeof(void*) */
#define PtoKPA(x) ((kpa_t)(unsigned long) (x))
#define KPAtoP(ty,x) ((ty)(unsigned long) (x))
#define VtoKVA(x) ((kva_t)(unsigned long) (x))
#define KVAtoV(ty,x) ((ty)(unsigned long) (x))
/* Otherwise: */
/* #define KPA(x) ((kpa_t) (x)) */
/* #define KPAtoP(ty,x) ((ty) (x)) */
#endif

typedef unsigned long cap_t;
typedef fixreg_t result_t;

#endif /* __TARGET_H__ */
