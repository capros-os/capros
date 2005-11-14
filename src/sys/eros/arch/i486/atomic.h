#ifndef __ATOMIC_H__
#define __ATOMIC_H__

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
 * Macros to support atomic memory manipulation.  Since in some cases
 * this involves kernel-implemented pseudo instructions, the
 * associated header is part of the kernel tree.
 */

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
 * and must be emulated by the kernel.  THIS IS CURRENTLY NOT
 * IMPLEMENTED BY EROS.
 *
 * There is no corresponding library function -- the prototype is
 * provided solely for the benefit of lint-like tools.
 */

INLINE int
ATOMIC_SWAP32(volatile uint32_t* p_word, uint32_t old, uint32_t new)
{
  uint32_t old_val = old;

  __asm__ __volatile__ (
			"lock\n\t"
			"cmpxchgl %3,%4\n\t"
			: "=a" (old_val), "=m" (*(p_word))   
			: "0" (old_val), "r" (new), "m" (*(p_word)) 
			: "cc");

  if (old == old_val)
    return 1;
  return 0;
}


extern void ATOMIC_INC32(volatile uint32_t* p_word);
#define ATOMIC_INC32(p_word)                            \
  for(;;) {                                             \
    uint32_t count = *p_word;                           \
    if (ATOMIC_SWAP32(p_word, count, count+1))          \
      break;                                            \
  }

extern void ATOMIC_DEC32(volatile uint32_t* p_word);
#define ATOMIC_DEC32(p_word)                            \
  for(;;) {                                             \
    uint32_t count = *p_word;                           \
    if (ATOMIC_SWAP32(p_word, count, count-1))          \
      break;                                            \
  }

#endif /* __ATOMIC_H__ */
