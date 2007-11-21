#ifndef __MACHINE_ATOMIC_H__
#define __MACHINE_ATOMIC_H__

/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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
 * Functions to support atomic memory manipulation.  Since in some cases
 * this involves kernel-implemented pseudo instructions, the
 * associated header is part of the kernel tree.
 */

#include <eros/target.h>

/*
 * The effect of CSWAP32 is to execute the following block of code as
 * an atomic instruction: 
 *
 * if (*p_word==old_val)
 *   *p_word=new_val;
 *   return old_val;
 * else
 *   return *p_word;
 *
 * On the Pentium and later, this generates a write cycle regardless
 * of the old value.
 *
 * On the 386 and earlier chips, this instruction is not implemented,
 * and must be emulated by the kernel.  THIS IS NOT SUPPORTED.
 */

INLINE uint32_t
capros_atomic_cmpxchg32(volatile uint32_t * p_word,
  uint32_t oldVal, uint32_t newVal)
{
  __asm__ __volatile__ (
			"lock\n\t"
			"cmpxchgl %3,%4\n\t"
			: "=a" (oldVal), "=m" (*(p_word))   
			: "0" (oldVal), "r" (newVal), "m" (*(p_word)) 
			: "cc");
  return oldVal;
}

// Add i to *p_word and return the result.
INLINE uint32_t
capros_atomic_add32_return(uint32_t i, volatile uint32_t * p_word)
{
  uint32_t oldVal;
  uint32_t val = *p_word;
  do {
    oldVal = val;
    val = capros_atomic_cmpxchg32(p_word, val, val + i);
  } while (val != oldVal);
  return val + i;
}

INLINE bool
ATOMIC_SWAP32(volatile uint32_t* p_word, uint32_t old, uint32_t newVal)
{
  uint32_t old_val = capros_atomic_cmpxchg32(p_word, old, newVal);
  return (old == old_val);
}

INLINE void
ATOMIC_INC32(volatile uint32_t * p_word)
{
  (void) capros_atomic_add32_return(1, p_word);
}

INLINE void
ATOMIC_DEC32(volatile uint32_t * p_word)
{
  (void) capros_atomic_add32_return(-1, p_word);
}

#endif /* __MACHINE_ATOMIC_H__ */
