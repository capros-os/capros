#ifndef __ATOMIC_H__
#define __ATOMIC_H__
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

/*
 * Functions to support atomic memory manipulation.
 */

/*
 * The effect of SWI SWI_CSwap32 is to execute the following block of code as
 * an atomic instruction
   BUT it is not atomic with respect to memory modified in an FIQ handler
 *
 * if (*p_word==old_val)
 *   *p_word=new_val;
 *   return old_val;
 * else
 *   return *p_word;
 */

INLINE int
ATOMIC_SWAP32(volatile uint32_t* p_word, uint32_t old, uint32_t new)
{
  uint32_t old_val = old;

  __asm__ __volatile__ ("swi 1");	/* SWI_CSwap32 */

  return (old == old_val);
}


/* There is no corresponding library function -- the prototype is
   provided solely for the benefit of lint-like tools. */
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
