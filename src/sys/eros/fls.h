#ifndef _EROS_FLS_H_
#define _EROS_FLS_H_

/*
 * Copyright (C) 2008, Strawberry Development Group.
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

/**
 * fls32 - find last (most-significant) bit set
 * @x: the word to search
 *
 * Note fls(0) = 0, fls(1) = 1, fls(0x80000000) = 32.
 */

static inline unsigned int fls32(uint32_t x)
{
  unsigned int r;

  if (!x)
    return 0;
  r = 1;
  if (x & 0xffff0000u) {
    r += 16;
    x >>= 16;
  }
  if (x & 0xff00u) {
    r += 8;
    x >>= 8;
  }
  if (x & 0xf0u) {
    x >>= 4;
    r += 4;
  }
  if (x & 0xcu) {
    x >>= 2;
    r += 2;
  }
  if (x & 0x2u) {
    r += 1;
  }
  return r;
}

static inline unsigned int fls64(uint64_t x)
{
  uint32_t h = x >> 32;
  if (h)
    return fls32(h) + 32;
  return fls32(x);
}

#endif /* _EROS_FLS_H_ */
