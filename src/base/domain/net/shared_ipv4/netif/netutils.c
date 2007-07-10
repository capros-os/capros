/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
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
#include <stddef.h>

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/NetKey.h>

#include <string.h>

#include "constituents.h"
#include "enetkeys.h"
#include "enet.h"

#include "netutils.h"

#define DEBUG_NETUTILS  if(0)

/* Place the newly constructed "mapped memory" tree into the process's
 * address space. */
void
patch_addrspace(uint16_t dma_lss)
{
  /* Assumptions:  winsys main has already prep'd address space for
   * mapping, so next available slot in ProcAddrSpace node is 16 */
  uint32_t next_slot = 16;
  
  /* Stash the current ProcAddrSpace capability */
  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  node_swap(KR_SCRATCH, next_slot, KR_DMA, KR_VOID);

  return;
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

  DEBUG_NETUTILS kprintf(KR_OSTREAM, "lance: init mapped memory complete.");
}

/* Start the helper thread & then pass it our start key so that
 * the helper can notify us of interrupts */
result_t
StartHelper(uint32_t irq) 
{
  Message msg;

  /* Make a start key for the helper. The helper uses this key to notify
   * us of IRQ5 events */
  process_make_start_key(KR_SELF,ENET_HELPER_INTERFACE,
			 KR_SCRATCH);
  
  memset(&msg,0,sizeof(Message));
  /* Pass KR_HELPER_TYPE start key to the helper process */
  msg.snd_invKey = KR_HELPER;
  msg.snd_code = OC_netdriver_key;
  msg.snd_w1 = irq;
  msg.snd_key0 = KR_SCRATCH;
  CALL(&msg);

  DEBUG_NETUTILS kprintf(KR_OSTREAM, "enet:sendingkey+IRQ ... [SUCCESS]");
    
  return RC_OK;
}
