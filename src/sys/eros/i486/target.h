#ifndef __TARGET_I486_H__
#define __TARGET_I486_H__

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
   within the kernel code and the volume construction utilities.
 
   This file is included from both C and C++ code, so it needs to be
   handled carefully.  */

#if !(defined(i386) || defined(i486))
#error "Wrong target file included"
#endif

#ifdef __GNUC__
#if __GNUC__ >= 2
#if __GNUC_MINOR__ >= 91
/* newer compilers do not want the __volatile__ qualifier */
#define GNU_INLINE_ASM __asm__
#endif
#endif

#ifndef GNU_INLINE_ASM
#define GNU_INLINE_ASM __asm__ __volatile__
#endif
#endif /* __GNUC__ */

#define NORETURN __attribute__ ((noreturn))

#define BITFIELD_PACK_LOW	/* bitfields pack low-order bits first */

#define EROS_HAVE_FPU

#ifndef NULL
#define NULL (0L)
#endif

/* This is needed for cross-build compatibility */
#ifndef __BIT_TYPES_DEFINED__
#if defined(__FreeBSD__) && defined(_MACHINE_TYPES_H_)
#define __BIT_TYPES_DEFINED__
#endif /* defined(__FreeBSD__) && defined(_MACHINE_TYPES_H_) */
#endif /* __BIT_TYPES_DEFINED__ */

#ifndef _STDINT_H

/* Basic EROS type definitions: */
#ifndef __BIT_TYPES_DEFINED__	
/* avoid conflict with linux hdrs in cross code: */
typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed long         int32_t;
typedef signed long long    int64_t;

/* EROS does not use these, but UNIX does.  Define them here so that
   we can just define __BIT_TYPES_DEFINED__ without conflict: */
typedef unsigned char       u_int8_t;
typedef unsigned short      u_int16_t;
typedef unsigned long       u_int32_t;
typedef unsigned long long  u_int64_t;
#define __BIT_TYPES_DEFINED__

#endif /* __BIT_TYPES_DEFINED__ */

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
/* The Linux weenies cannot make up their minds about how uint32_t
   should be defined.  The definition is conditionalized here in a
   linux-sensitive way because I'm tired of twiddling it one way or
   the other. */
#if defined(linux) && defined(_STDINT_H)
typedef unsigned int        uint32_t;
#else
typedef unsigned long       uint32_t;
#endif
typedef unsigned long long  uint64_t;

typedef unsigned char       bool_t;

typedef struct uint80_t {
  uint16_t	v[5];
}                           uint80_t;

typedef uint80_t floatval_t;

#endif /* !_STDINT_H */

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

typedef uint32_t	fixreg_t; /* fixed-point natural register size */

/* There is a portability problem with casting pointers to kpa_t if
 * they are of different size. The compiler complains. This impedes
 * portability, and it's a REALLY dumb idea. Therefore, you should
 * adjust the following macro according to whether
 *
 *     sizeof(kpa_t)==sizeof(void*)
 *
 * or not:
 */

/* Number of hardware interrupt lines */
#define NUM_HW_INTERRUPT 16

#define TARGET_LONG_MAX (2147483647) /* max value of a "long int" */
#define TARGET_LONG_MIN (-TARGET_LONG_MAX-1) /* min value of a "long int" */
#define TARGET_ULONG_MAX 4294967295u

#endif /* __TARGET_I486_H__ */
