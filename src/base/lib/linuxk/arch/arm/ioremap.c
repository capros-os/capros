/*
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

#include <eros/Invoke.h>
#include <domain/assert.h>

#include <idl/capros/Void.h>
#include <idl/capros/Node.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <asm/io.h>
#include <linux/mutex.h>

static DEFINE_MUTEX(iomapLock);

bool ioremapHaveGPT17 = false;
uint32_t ioremapNextPageOffset = 0;

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

  capros_Node_getSlotExtended(KR_LINUX_EMUL, LE_IOMEM, KR_TEMP1);
  int slot;
  for (slot = 0; ; ) {
    unsigned long w0, w1, w2;
    result = capros_Node_getSlotExtended(KR_TEMP1, slot, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_Number_get(KR_TEMP0, &w0, &w1, &w2);
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

  mutex_lock(&iomapLock);

  if (! ioremapHaveGPT17) {
    // First use of ioremap(). Create the GPT17 and the first GPT12.
    result = capros_SpaceBank_alloc2(KR_BANK,
               capros_Range_otGPT + (capros_Range_otGPT << 8),
               KR_TEMP2, KR_TEMP3);
    if (result != RC_OK)
      assert(false);	//// FIXME: clean up, return error

    result = capros_GPT_setL2v(KR_TEMP3,
                               EROS_PAGE_LGSIZE + 2 * capros_GPT_l2nSlots);
    assert(result == RC_OK);
    capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_MAPS_GPT, KR_TEMP3, KR_VOID);

    result = capros_GPT_setL2v(KR_TEMP2,
                               EROS_PAGE_LGSIZE + capros_GPT_l2nSlots);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_TEMP3, 0, KR_TEMP2);
    assert(result == RC_OK);

    // Put it in our address space.
    result = capros_Process_getAddrSpace(KR_SELF, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_TEMP0, LK_MAPS_BASE >> 22, KR_TEMP3);
    assert(result == RC_OK);

    ioremapHaveGPT17 = true;
  } else {
    capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_MAPS_GPT, KR_TEMP3);
  }

  /* Using an overly simple means of allocating space in the ioremap area: */
  uint32_t pgoffset = ioremapNextPageOffset;
  int gpt12have = -1;	// slot number of the GPT key in KR_TEMP2 if any
  for (i = 0; i < nPages; i++, pgoffset++) {
    int gpt12need = pgoffset / capros_GPT_nSlots;
    int gpt12slot = pgoffset % capros_GPT_nSlots;
    if (gpt12have != gpt12need) {
      // Get the l2v==12 GPT.
      result = capros_GPT_getSlot(KR_TEMP3, gpt12need, KR_TEMP2);
      assert(result == RC_OK);
      gpt12have = gpt12need;
    }
    // Copy one key.
    result = capros_Node_getSlotExtended(KR_TEMP1, slot++, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_GPT_setSlot(KR_TEMP2, gpt12slot, KR_TEMP0);
    if (result == RC_capros_key_Void) {
      // Need to create the l2v == 12 GPT
      // (We never free this GPT, even if all the space in it is unmapped.)
      result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_TEMP2);
      if (result != RC_OK)
        assert(false);	//// FIXME: clean up, return error
      result = capros_GPT_setL2v(KR_TEMP2,
                                 EROS_PAGE_LGSIZE + capros_GPT_l2nSlots);
      assert(result == RC_OK);
      result = capros_GPT_setSlot(KR_TEMP3, gpt12need, KR_TEMP2);
      assert(result == RC_OK);
    }
    else
      assert(result == RC_OK);
  }

  void __iomem * p = (void __iomem *)
                     ((ioremapNextPageOffset << EROS_PAGE_LGSIZE)
                     + LK_MAPS_BASE
                     + offset - beg );
  ioremapNextPageOffset += nPages;
  mutex_unlock(&iomapLock);
  return p;
}

void
__iounmap(volatile void __iomem * addr)
{
}
