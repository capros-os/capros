/*
 * Copyright (C) 1998, 1999, 2001, Jonathan Adams.
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

#include <string.h>
#include <eros/target.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/GPT.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "misc.h"
#include "debug.h"
#include "spacebank.h"
#include "Bank.h"

static unsigned int
L2vToBlss(unsigned int l2v)
{
  return (l2v - EROS_PAGE_ADDR_BITS) / EROS_NODE_LGSIZE + 1 + EROS_PAGE_BLSS;
}

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1 - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
}

bool
heap_insert_page(uint32_t addr, uint32_t krPage)
{
  uint32_t krtree = KR_WALK0;
  uint32_t krwalk = KR_WALK1;
  uint32_t treelss = L2vToBlss(HEAP_TOP_LG + EROS_NODE_LGSIZE -1) -1;
		// adding (EROS_NODE_LGSIZE -1) to round up
  uint8_t keyL2v;
  uint16_t keylss;
  uint32_t result;
  uint32_t orig_addr = addr;

  process_copy(KR_SELF, ProcAddrSpace, krtree);

  DEBUG(heap){
    uint32_t result, keyType;
    
    result = capros_key_getType(krtree, &keyType);
    if (result != RC_OK || keyType != AKT_GPT)
      kpanic(KR_OSTREAM, "spacebank: Wrong key type in segtree on"
	     " path 0x%08x\n", addr);

    result = capros_GPT_getL2v(krtree, &keyL2v);
    keylss = L2vToBlss(keyL2v);
    if (keylss != treelss)
      kpanic(KR_OSTREAM, "spacebank: heap_insert_page(): Key lss != tree lss\n");
  }
    
  while ( treelss > (EROS_PAGE_BLSS + 1) ) {
    uint32_t kt;
    const unsigned shift_amt = BlssToL2v(treelss);
    uint32_t addrMask = ((1u << shift_amt) - 1u);
    const unsigned offset = (addr >> shift_amt);
    const unsigned slot = offset & EROS_NODE_SLOT_MASK;

    DEBUG(heap) kdprintf(KR_OSTREAM, "treelss %d addr 0x%08x shift %d ndx %d\n",
			 treelss, addr, shift_amt, slot);

    addr &= addrMask;

    result = capros_GPT_getSlot(krtree, slot, krwalk);

    result = capros_key_getType(krwalk, &kt);
    if (result == RC_capros_key_Void) {
      /* Insert a GPT.  /slot/ still holds the slot to put it in. */
      keylss = treelss - 1;

      DEBUG(heap)
	kdprintf(KR_OSTREAM,
		 "expanding tree between treelss %d and keylss %d at slot %d\n"
		 "  kt 0x%x addr 0x%08x\n",
		 treelss, keylss, slot, kt, orig_addr);
      
      OID oid;
      if (BankAllocObject(&bank0, capros_Range_otGPT, krwalk, &oid) != RC_OK) {
	DEBUG(heap)
	  kprintf(KR_OSTREAM, "spacebank: Intermediate tree node alloc failed!\n");
	return false;
      }

      result = capros_GPT_setL2v(krwalk, BlssToL2v(keylss));

      /* insert new node into tree. */
      result = capros_GPT_setSlot(krtree, slot, krwalk);
    } else {	// have a GPT
      if (kt != AKT_GPT)
        kpanic(KR_OSTREAM,
               "spacebank: heap_insert_page(): Bad key type 0x%x in segtree\n",
               kt);
    
      result = capros_GPT_getL2v(krwalk, &keyL2v);
      keylss = L2vToBlss(keyL2v);
      if (keylss != treelss - 1) {
        kpanic(KR_OSTREAM,
               "spacebank: heap_insert_page(): Bad lss 0x%x in segtree\n",
               keylss);
      }
    }
    
    {
      uint32_t tmp = krtree;
      krtree = krwalk;
      krwalk = tmp;
    }    

    treelss = keylss;
  }

  {
    /* We are now holding in krtree a GPT with l2v == EROS_PAGE_LGSIZE.
       Buy a new page and stick it in. */

    const unsigned slot = (addr >> EROS_PAGE_ADDR_BITS) & EROS_NODE_SLOT_MASK;

    DEBUG(heap)
      kdprintf(KR_OSTREAM,"inserting passed page treelss %d slot 0x%x addr 0x%08x\n",
	       treelss, slot, orig_addr);

    capros_GPT_setSlot(krtree, slot, krPage);
  }

  return true;
}
