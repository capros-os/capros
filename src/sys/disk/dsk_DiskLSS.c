/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* All of this is in a separate file so that it won't get linked into
 * the kernel:
 */

#include <eros/target.h>
#include <disk/DiskLSS.h>

uint32_t
lss_SlotNdx(uint64_t offset, uint32_t blss)
{
  uint32_t bits_to_shift;

  if (blss <= EROS_PAGE_BLSS)
    return 0;

#if (EROS_PAGE_ADDR_BITS == (EROS_PAGE_BLSS * EROS_NODE_LGSIZE))
  bits_to_shift = blss * EROS_NODE_LGSIZE;
#else
  bits_to_shift =
    (blss - EROS_PAGE_BLSS - 1) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS; 
#endif

  if (bits_to_shift >= UINT64_BITS)
    return 0;

  return offset >> bits_to_shift;
}

uint64_t
lss_Mask(uint32_t blss)
{
  uint32_t bits_to_shift;
  uint64_t mask;

  if (blss < EROS_PAGE_BLSS)
    return 0ull;

#if (EROS_PAGE_ADDR_BITS == (EROS_PAGE_BLSS * EROS_NODE_LGSIZE))
  bits_to_shift = blss * EROS_NODE_LGSIZE;
#else
  bits_to_shift =
    (blss - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS; 
#endif

  if (bits_to_shift >= UINT64_BITS)
    return (uint64_t) -1;	/* all 1's */

  mask = (1ull << bits_to_shift);
  mask -= 1ull;
  
  return mask;
}

#ifndef __KERNEL__
/* biased LSS - compute cieling(log_ns) of the byte address and subtract 1, but
 * never return less than 2, where ns is EROS_NODE_LGSIZE;
 */

uint32_t
lss_BiasedLSS(uint64_t offset)
{
  /* Shouldn't this be using fcs()? */
  uint32_t bits = 0;
  uint32_t w0;
  
  static uint32_t hexbits[16] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
  };

  /* Run a decision tree: */
  if (offset >= 0x100000000ull) {
    bits += 32;
    w0 = (offset >> 32);
  } else {
    w0 = (uint32_t) offset;
  }

  if (w0 >= 0x10000u) {
    bits += 16;
    w0 >>= 16;
  }
  if (w0 >= 0x100u) {
    bits += 8;
    w0 >>= 8;
  }
  if (w0 >= 0x10u) {
    bits += 4;
    w0 >>= 4;
  }

  /* Table lookup for the last part: */
  bits += hexbits[w0];
  // offset fits in bits bits.
  
  if (bits < EROS_PAGE_ADDR_BITS)
    return EROS_PAGE_BLSS;

  bits -= EROS_PAGE_ADDR_BITS;

  bits += (EROS_NODE_LGSIZE - 1);
  bits /= EROS_NODE_LGSIZE;
  bits += EROS_PAGE_BLSS;
  
  return bits;
}
#endif  /* !__KERNEL__ */
