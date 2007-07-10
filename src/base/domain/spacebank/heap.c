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
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "misc.h"
#include "debug.h"
#include "spacebank.h"
#include "Bank.h"

bool
heap_insert_page(uint32_t addr, uint32_t krPage)
{
  uint32_t krtree = KR_WALK0;
  uint32_t krwalk = KR_WALK1;
  uint32_t treelss = EROS_ADDRESS_BLSS;
  uint16_t keylss;
  uint32_t result;
  uint32_t orig_addr = addr;

  process_copy(KR_SELF, ProcAddrSpace, krtree);

  DEBUG(heap){
    uint32_t result, keyType;
    
    result = capros_key_getType(krtree, &keyType);
    if (result != RC_OK || keyType != AKT_Node)
      kpanic(KR_OSTREAM, "spacebank: Wrong key type in segtree on"
	     " path 0x%08x\n", addr);

    get_lss_and_perms(krtree, &keylss, 0);

    keylss &= SEGMODE_BLSS_MASK;
    if (keylss != treelss)
      kpanic(KR_OSTREAM, "spacebank: heap_insert_page(): Key lss != tree lss\n");
  }
    
  while ( treelss > (EROS_PAGE_BLSS + 1) ) {
    uint32_t kt;
    const unsigned shift_amt =
      (treelss - EROS_PAGE_BLSS - 1) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS; 
    uint32_t addrMask = ((1u << shift_amt) - 1u);
    const unsigned offset = (addr >> shift_amt);
    const unsigned slot = offset & EROS_NODE_SLOT_MASK;

    DEBUG(heap) kdprintf(KR_OSTREAM, "treelss %d addr 0x%08x shift %d ndx %d\n",
			 treelss, addr, shift_amt, slot);

    addr &= addrMask;

    node_copy(krtree, slot, krwalk);

    result = capros_key_getType(krwalk, &kt);
    keylss &= SEGMODE_BLSS_MASK;
  
    if (result != RC_capros_key_Void && kt != AKT_Node)
      kpanic(KR_OSTREAM,
             "spacebank: heap_insert_page(): Bad key type 0x%x in segtree\n",
             kt);
    
    get_lss_and_perms(krwalk, &keylss, 0);
#if 0
    if (keylss < (treelss - 1)) {
      /* Figure out if we need to insert a new intervening node by
	 checking if the slot offset would be too large if we just
	 used this one */
      const shiftlss = MAX(keylss, EROS_PAGE_BLSS+1);
      const shift_amt = shiftlss * EROS_NODE_LGSIZE;
      const offset = (addr >> shift_amt);

      kdprintf(KR_OSTREAM,"found keylss %d in treelss %d kt 0x%x addr 0x%08x offst 0x%x\n",
	       keylss, treelss, kt, orig_addr, offset);
      
      OID oid;
      if (offset > 0) {
	if (BankAllocObject(&bank0, capros_Range_otNode, krwalk, &oid) != RC_OK)
	  return false;

	keylss = treelss - 1;
	node_make_node_key(krwalk, keylss, krwalk);

	/* insert new node, and put old key in proper place under new
	   node. */
	node_swap(krtree, slot, krwalk, KR_TMP);
	node_swap(krwalk, 0, KR_TMP, KR_VOID);
      }
      else if (keylss == EROS_PAGE_BLSS) {
	DEBUG(heap)
	  kprintf(KR_OSTREAM, "swapping treelss %d addr 0x%08x shift %d ndx %d\n",
		  treelss, orig_addr, shift_amt, slot);
	node_swap(krtree, slot, krPage, KR_VOID);
	return true;
      }
    }
#else
    if (keylss < (treelss - 1)) {
      /* Insert an intermediate node.  /slot/ still holds the slot to
	 put it in. */

      DEBUG(heap)
	kdprintf(KR_OSTREAM,
		 "expanding tree between treelss %d and keylss %d at slot %d\n"
		 "  kt 0x%x addr 0x%08x\n",
		 treelss, keylss, slot, kt, orig_addr);
      
      OID oid;
      if (BankAllocObject(&bank0, capros_Range_otNode, krwalk, &oid) != RC_OK) {
	DEBUG(heap)
	  kprintf(KR_OSTREAM, "spacebank: Intermediate tree node alloc failed!\n");
	return false;
      }

      keylss = treelss - 1;
      node_make_node_key(krwalk, keylss, 0, krwalk);

      /* insert new node into tree. */
      node_swap(krtree, slot, krwalk, KR_VOID);
    }
#endif
    
    {
      uint32_t tmp = krtree;
      krtree = krwalk;
      krwalk = tmp;
    }    

    treelss = keylss;
  }

  {
    /* We are now holding in krtree an lss3 node. Buy a new page and
       stick it in. */

    const unsigned slot = (addr >> EROS_PAGE_ADDR_BITS) & EROS_NODE_SLOT_MASK;

    DEBUG(heap)
      kdprintf(KR_OSTREAM,"inserting passed page treelss %d slot 0x%x addr 0x%08x\n",
	       treelss, slot, orig_addr);

    node_swap(krtree, slot, krPage, KR_VOID);
  }

  return true;
}
