/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/Number.h>

#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>

#include "addrspace.h"

/* Following is used to compute 32 ^ lss for patching together address
   space */
#define LWK_FACTOR(lss) (mult(EROS_NODE_SIZE, lss) * EROS_PAGE_SIZE)
static uint32_t
mult(uint32_t base, uint32_t exponent)
{
  uint32_t u;
  int32_t result = 1u;

  if (exponent == 0)
    return result;

  for (u = 0; u < exponent; u++)
    result = result * base;

  return result;
}


/* Convenience routine for buying a new node for use in expanding the
   address space. */
uint32_t 
addrspace_new_space(uint32_t kr_bank, uint16_t lss, uint32_t kr_new)
{
  uint32_t result = spcbank_buy_nodes(kr_bank, 1, kr_new, KR_VOID, KR_VOID);
  if (result != RC_OK)
    return result;

  return node_make_node_key(kr_new, lss, 0, kr_new);
}

/* Make room in this domain's address space for mapping subspaces
   corresponding to client windows */
uint32_t 
addrspace_prep_for_mapping(uint32_t kr_self, uint32_t kr_bank, 
			   uint32_t kr_tmp, uint32_t kr_new_node)
{
  capros_Number_value window_key;
  uint32_t slot;
  uint32_t result = RC_OK;

  if (EROS_NODE_SIZE != 32)
    return RC_capros_key_RequestError;

  /* Stash the current ProcAddrSpace key */
  result = process_copy(kr_self, ProcAddrSpace, kr_tmp);
  if (result != RC_OK)
    return result;

  /* Make a node with max lss */
  result = addrspace_new_space(kr_bank, EROS_ADDRESS_LSS, kr_new_node);
  if (result != RC_OK)
    return result;

  /* Patch up KR_ADDRSPC as follows:
     slot 0 = capability for original ProcAddrSpace
     slots 1-15 = local window keys for ProcAddrSpace
     slot 16 - ?? = any needed mapped spaces
  */
  result = node_swap(kr_new_node, 0, kr_tmp, KR_VOID);
  if (result != RC_OK)
    return result;

  for (slot = 1; slot < 16; slot++) {
    window_key.value[2] = 0;	/* slot 0 of local node */
    window_key.value[1] = 0;	/* high order 32 bits of address
				   offset */

    /* low order 32 bits: multiple of EROS_NODE_SIZE ^ (LSS) pages */
    window_key.value[0] = slot * LWK_FACTOR(EROS_ADDRESS_LSS); 

    /* insert the window key at the appropriate slot */
    result = node_write_number(kr_new_node, slot, &window_key); 
    if (result != RC_OK)
      return result;
  }

  /* Finally, patch up the ProcAddrSpace register */
  return process_swap(kr_self, ProcAddrSpace, kr_new_node, KR_VOID);
}

uint32_t 
addrspace_insert_lwk(cap_t node, uint32_t base_slot, uint32_t lwk_slot, 
		     uint16_t lss_of_base)
{
  capros_Number_value lwk;

  lwk.value[2] = base_slot << EROS_NODE_LGSIZE;
  lwk.value[1] = 0;  /* local window key high order bits are zero  */
  lwk.value[0] = (lwk_slot - base_slot) * LWK_FACTOR(lss_of_base);

  return node_write_number(node, lwk_slot, &lwk);
}

