/*
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

#include <eros/Invoke.h>
#include <domain/assert.h>

#include <idl/capros/Node.h>

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <asm/io.h>
#include <linux/mutex.h>
#include <domain/CMTEMaps.h>

void __iomem *
__arm_ioremap(unsigned long offset, size_t size, unsigned int flags)
{
  result_t result;
  int i;

  uint32_t beg = offset & ~ EROS_PAGE_MASK;	// round down to page boundary
  uint32_t end = ((offset + size) + (EROS_PAGE_SIZE - 1)) & ~ EROS_PAGE_MASK;
                   // round up to page boundary
  unsigned long nPages = (end - beg) >> EROS_PAGE_LGSIZE;

  assert(flags == MT_DEVICE);	// others not implemented

  // Find the page capability(s).
  capros_Node_getSlotExtended(KR_LINUX_EMUL, LE_IOMEM, KR_TEMP3);
  int slot;
  for (slot = 0; ; ) {
    unsigned long w0, w1, w2;
    result = capros_Node_getSlotExtended(KR_TEMP3, slot, KR_TEMP2);
    assert(result == RC_OK);
    result = capros_Number_get(KR_TEMP2, &w0, &w1, &w2);
    if (result == RC_capros_key_Void)
      return NULL;	// page keys not found
    assert(result == RC_OK);
    // w0 has number of pages
    // w1 has starting physical address
    // (w2 reserved for larger physical addresses)
    uint32_t rangeEnd = w1 + (w0 << EROS_PAGE_LGSIZE);
    if (w1 <= beg && rangeEnd >= end) {
      // Found the keys we need.
      slot++;		// skip the number key
      slot += (beg - w1) >> EROS_PAGE_LGSIZE;
      break;		// continue below with better indenting
    }
    slot += w0 + 1;	// skip the number key and pages
  }
  // slot is the first of the keys we need.

  long blockStart = maps_reserve(nPages);
  if (blockStart < 0) return NULL;	// too bad

  uint32_t pgoffset;
  for (i = 0, pgoffset = blockStart; i < nPages; i++, pgoffset++) {
    // Copy one key.
    result = capros_Node_getSlotExtended(KR_TEMP3, slot++, KR_TEMP2);
    assert(result == RC_OK);
    result = maps_mapPage(pgoffset, KR_TEMP2);
    if (result != RC_OK) {
      // It's OK to leave any partial map in place; we shouldn't use it.
      maps_liberate(blockStart, nPages);
      return NULL;
    }
  }

  return (void __iomem *) (maps_pgOffsetToAddr(blockStart)
                           + offset - beg );
}

void
__iounmap(volatile void __iomem * addr)
{
}
