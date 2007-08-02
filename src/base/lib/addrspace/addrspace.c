/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

#include <eros/target.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>

#include <domain/Runtime.h>

#include "addrspace.h"

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1 - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
}

/* Convenience routine for buying a new GPT with specified lss. */
uint32_t 
addrspace_new_space(uint32_t kr_bank, uint16_t lss, uint32_t kr_new)
{
  uint32_t result = capros_SpaceBank_alloc1(kr_bank,
                      capros_Range_otGPT, kr_new);
  if (result != RC_OK)
    return result;

  return capros_GPT_setL2v(kr_new, BlssToL2v(lss));
}

/* Make room in this domain's address space for mapping subspaces
   corresponding to client windows */
uint32_t 
addrspace_prep_for_mapping(uint32_t kr_self, uint32_t kr_bank, 
			   uint32_t kr_tmp, uint32_t kr_new_GPT)
{
  uint32_t result;

  if (EROS_NODE_SIZE != 32)
    return RC_capros_key_RequestError;

  /* Get the current ProcAddrSpace key */
  result = process_copy(kr_self, ProcAddrSpace, kr_tmp);
  if (result != RC_OK)
    return result;

  /* Make a GPT with max lss */
  result = addrspace_new_space(kr_bank, EROS_ADDRESS_LSS, kr_new_GPT);
  if (result != RC_OK)
    return result;

  /* Patch up kr_new_GPT as follows:
     slot 0 = capability for original ProcAddrSpace
     slots 1-15 = local window keys for ProcAddrSpace
     slots 16-31 = any needed mapped spaces
  */
  result = capros_GPT_setSlot(kr_new_GPT, 0, kr_tmp);
  if (result != RC_OK)
    return result;

  uint32_t slot;
  for (slot = 1; slot < 16; slot++) {
    /* insert the window key at the appropriate slot */
    result = capros_GPT_setWindow(kr_new_GPT, slot, 0, 0,
               ((uint64_t)slot) << BlssToL2v(EROS_ADDRESS_LSS) );
    if (result != RC_OK)
      return result;
  }

  /* Finally, patch up the ProcAddrSpace register */
  return process_swap(kr_self, ProcAddrSpace, kr_new_GPT, KR_VOID);
}

uint32_t 
addrspace_insert_lwk(cap_t GPT, uint32_t base_slot, uint32_t lwk_slot, 
		     uint16_t lss_of_base)
{
  return capros_GPT_setWindow(GPT, lwk_slot, base_slot, 0,
           ((uint64_t)(lwk_slot - base_slot)) << BlssToL2v(lss_of_base) );
}

