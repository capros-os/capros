#ifndef __TARGET_ARM_H__
#define __TARGET_ARM_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

/* Basic type declarations for the target platform, used pervasively
   within the kernel code and the volume construction utilities.
 
   This file is included from both C and C++ code, 
   and from both host and target code,
   so it needs to be handled carefully.  */

#if !(defined(EROS_TARGET_arm))
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
/***** Verify this! */

/*#define EROS_HAVE_FPU */

#ifndef NULL
#define NULL (0L)
#endif

#if 0
typedef struct uint80_t {
  uint16_t	v[5];
} uint80_t;

typedef uint80_t floatval_t;
#endif

typedef uint32_t fixreg_t; /* fixed-point natural register size */

/* Number of hardware interrupt lines (sources) */
#define NUM_HW_INTERRUPT 64

#endif /* __TARGET_ARM_H__ */
