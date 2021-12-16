#ifndef __MACHINE_ATOMIC_H__
#define __MACHINE_ATOMIC_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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
 * Functions to support atomic memory manipulation.  Since in some cases
 * this involves kernel-implemented pseudo instructions, the
 * associated header is part of the kernel tree.
 */

#include <eros/target.h>

typedef volatile uint32_t capros_atomic32;

#define capros_atomic32_Initializer(i) (i)

INLINE uint32_t
capros_atomic32_read(capros_atomic32 * a)
{
  return *a;
}

// Following are for uniprocessor:
INLINE void
capros_atomic32_set(capros_atomic32 * a, uint32_t v)
{
  *a = v;
}

/*
 * The effect of SWI SWI_CSwap32 is to execute the following block of code as
 * an atomic instruction
   BUT it is not atomic with respect to memory modified in an FIQ handler

unsigned long current_val = * p_word;
if (current_val == oldVal)
  * p_word = newVal;
return current_val;

 */

INLINE uint32_t
capros_atomic32_cmpxchg(capros_atomic32 * p_word,
  uint32_t oldVal, uint32_t newVal)
{
  register uint32_t r0 __asm__("r0") = (uint32_t) p_word;
  register uint32_t r1 __asm__("r1") = oldVal;
  register uint32_t r2 __asm__("r2") = newVal;
  __asm__ __volatile__ ("swi 1"	// SWI_CSwap32
         : "=r" (r0)
         : "0" (r0), "r" (r1), "r" (r2)
         : "memory" );
  return r0;
}

// Add i to *p_word and return the result.
INLINE uint32_t
capros_atomic32_add_return(capros_atomic32 * p_word, uint32_t i)
{
  uint32_t oldVal;
  uint32_t val = *p_word;
  do {
    oldVal = val;
    val = capros_atomic32_cmpxchg(p_word, val, val + i);
  } while (val != oldVal);
  return val + i;
}

INLINE bool
ATOMIC_SWAP32(capros_atomic32 * p_word, uint32_t old, uint32_t newVal)
{
  uint32_t old_val = capros_atomic32_cmpxchg(p_word, old, newVal);
  return (old == old_val);
}

INLINE void
ATOMIC_INC32(capros_atomic32 * p_word)
{
  (void) capros_atomic32_add_return(p_word, 1);
}

INLINE void
ATOMIC_DEC32(capros_atomic32 * p_word)
{
  (void) capros_atomic32_add_return(p_word, -1);
}

#endif /* __MACHINE_ATOMIC_H__ */
