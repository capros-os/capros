/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */
     
#include <eros/target.h>
#include <disk/Key-inline.h>
#include <eros/ffs.h>

// For memory and node keys:
uint64_t
key_GetGuard(const KeyBits * thisPtr)
{
  unsigned int l2g = keyBits_GetL2g(thisPtr);
  if (l2g >= 64)
    return 0;
  else {
    uint64_t guard = keyBits_GetGuard(thisPtr);
    return guard << l2g;
  }
}

// For memory and node keys:
// Returns true iff successful.
bool
key_CalcGuard(uint64_t guard, struct GuardData * gd)
{
  if (guard == 0) {
    gd->guard = 0;
    gd->l2g = 0;
    return true;
  } else {
    // Find lowest 1 bit:
    unsigned int fs = ffs64(guard);
    guard >>= fs;
    unsigned int guardTrunc = guard & 0xff;
    if (guardTrunc != guard) {	// it won't fit in 8 bits
      return false;
    } else {
      gd->guard = guardTrunc;
      gd->l2g = fs;
      return true;
    }
  }
}

