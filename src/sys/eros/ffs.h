#ifndef _EROS_FFS_H_
#define _EROS_FFS_H_

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

/*
 * ffs32 - find first (least-significant) 1-bit in word.
 * The least-significant bit is bit 0 (note this is different from Unix ffs).
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static inline unsigned int ffs32(uint32_t word)
{
  int num;

  if ((word & 0xffff) == 0) {
    num = 16;
    word >>= 16;
  }
  else num = 0;
  if ((word & 0xff) == 0) {
    num += 8;
    word >>= 8;
  }
  if ((word & 0xf) == 0) {
    num += 4;
    word >>= 4;
  }
  if ((word & 0x3) == 0) {
    num += 2;
    word >>= 2;
  }
  if ((word & 0x1) == 0)
    num += 1;
  return num;
}

/*
 * ffs64 - find first (least-significant) 1-bit in word.
 * The least-significant bit is bit 0 (note this is different from Unix ffs).
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static inline unsigned int ffs64(uint64_t word)
{
  if ((word & 0xffffffff) == 0) {
    return 32 + ffs32(word >>= 32);
  }
  else return ffs32((uint32_t)word);
}

#endif // _EROS_FFS_H_
