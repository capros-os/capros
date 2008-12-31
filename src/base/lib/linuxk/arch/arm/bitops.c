/*
 * Copyright 1995, Russell King.
 * Various bits and pieces copyrights include:
 *  Linus Torvalds (test_bit).
 * Big endian support: Copyright 2001, Nicolas Pitre
 *  reworked by rmk.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

//#include <linuxk/linux-emul.h>
#include <asm-arm/bitops.h>

/*
 * bit 0 is the LSB of an "unsigned long" quantity.
 */

//#include <linux/compiler.h>
//#include <asm/system.h>
#include <eros/machine/atomic.h>

#define atomic_begin \
  unsigned long oldVal = *p, newVal; \
  do {

#define atomic_end \
    newVal = capros_atomic32_cmpxchg(p, oldVal, newVal); \
    if (newVal == oldVal) break; \
    oldVal = newVal; \
  } while (1);

/*
 * These functions are the basis of our bit ops.
 *
 * The atomic bitops. These use native endian.
 */
void _set_bit_le(unsigned int bit, volatile unsigned long * p)
{
  unsigned long mask = 1UL << (bit & 0x1f);

  p += bit >> 5;

  atomic_begin
    newVal = oldVal | mask;
  atomic_end
}

void _clear_bit_le(unsigned int bit, volatile unsigned long * p)
{
  unsigned long mask = 1UL << (bit & 0x1f);

  p += bit >> 5;

  atomic_begin
    newVal = oldVal & ~mask;
  atomic_end
}

void _change_bit_le(unsigned int bit, volatile unsigned long * p)
{
  unsigned long mask = 1UL << (bit & 0x1f);

  p += bit >> 5;

  atomic_begin
    newVal = oldVal ^ mask;
  atomic_end
}

int
_test_and_set_bit_le(unsigned int bit, volatile unsigned long * p)
{
  unsigned long mask = 1UL << (bit & 0x1f);

  p += bit >> 5;

  atomic_begin
    newVal = oldVal | mask;
  atomic_end

  return oldVal & mask;
}

int
_test_and_clear_bit_le(unsigned int bit, volatile unsigned long * p)
{
  unsigned long mask = 1UL << (bit & 0x1f);

  p += bit >> 5;

  atomic_begin
    newVal = oldVal & ~mask;
  atomic_end

  return oldVal & mask;
}

int
_test_and_change_bit_le(unsigned int bit, volatile unsigned long * p)
{
  unsigned long mask = 1UL << (bit & 0x1f);

  p += bit >> 5;

  atomic_begin
    newVal = oldVal ^ mask;
  atomic_end

  return oldVal & mask;
}
