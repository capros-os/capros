/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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

#include <stddef.h>

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/ProcessKey.h>

#include <idl/capros/key.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/MemmapKey.h>
#include <domain/drivers/NetKey.h>

#include <string.h>

#include "constituents.h"
#include "../keyring.h"

#include "netutils.h"

#define DEBUG_NETUTILS  if(0)

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1 - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
}

/* Convenience routine for buying a new node for use in expanding the
 * address space. */
uint32_t
make_new_addrspace(uint16_t lss, fixreg_t key)
{
  uint32_t result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, key);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM,"Error: make_new_addrspace: buying GPT "
	    "returned error code: %u.\n", result);
    return result;
  }

  result = capros_GPT_setL2v(key, BlssToL2v(lss));
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Error: make_new_addrspace: setL2v "
	    "returned error code: %u.\n", result);
    return result;
  }
  return RC_OK;
}

/* Place the newly constructed "mapped memory" tree into the process's
 * address space. */
void
patch_addrspace(uint16_t dma_lss)
{
  /* Stash the current ProcAddrSpace capability */
  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  
  /* Make a node with max lss */
  make_new_addrspace(EROS_ADDRESS_LSS, KR_ADDRSPC);
  
  /* Patch up KR_ADDRSPC as follows:
     slot 0 = capability for original ProcAddrSpace
     slots 1-15 = local window keys for ProcAddrSpace
     slot 16 = capability for FIFO
     slot 16 - ?? = local window keys for FIFO, as needed
     remaining slot(s) = capability for FRAMEBUF and any needed window keys
  */
  capros_GPT_setSlot(KR_ADDRSPC, 0, KR_SCRATCH);

  uint32_t next_slot = 0;
  for (next_slot = 1; next_slot < 16; next_slot++) {
    /* insert the window key at the appropriate slot */
    capros_GPT_setWindow(KR_ADDRSPC, next_slot, 0, 0,
        ((uint64_t)next_slot) << BlssToL2v(EROS_ADDRESS_LSS-1));
  }

  next_slot = 16;
  
  capros_GPT_setSlot(KR_ADDRSPC, next_slot, KR_MEMMAP_C);
  if (dma_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: lance(): no room for local window "
             "keys for DMA!");
  next_slot++;

  /* Finally, patch up the ProcAddrSpace register */
  process_swap(KR_SELF, ProcAddrSpace, KR_ADDRSPC, KR_VOID);
}

/* Generate address faults in the entire mapped region in order to
 * ensure entire address subspace is fabricated and populated with
 *  correct page keys. */
void
init_mapped_memory(uint32_t *base, uint32_t size)
{
  uint32_t u;

  kprintf(KR_OSTREAM,"lance: initing mapped memory at 0x%08x",(uint32_t)base);

  for (u=0; u < (size / (sizeof(uint32_t))); u=u+EROS_PAGE_SIZE)
    base[u] &= 0xffffffffu;

  kprintf(KR_OSTREAM, "lance: init mapped memory complete.");
}

/* Start the helper thread & then pass it our start key so that
 * the helper can notify us of interrupts */
uint32_t
StartHelper(uint32_t irq) 
{
  Message msg;
  
  memset(&msg,0,sizeof(Message));
  /* Pass KR_HELPER_TYPE start key to the helper process */
  msg.snd_invKey = KR_HELPER;
  msg.snd_code = OC_netdriver_key;
  msg.snd_w1 = irq;
  msg.snd_key0 = KR_HELPER_TYPE;
  CALL(&msg);

  DEBUG_NETUTILS kprintf(KR_OSTREAM, "enet:sendingkey+IRQ ... [SUCCESS]");
    
  return RC_OK;
 
}

