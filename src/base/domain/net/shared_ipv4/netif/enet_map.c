/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <domain/EnetKey.h>
#include <addrspace/addrspace.h>

#include "../memmgr/global.h"
#include "enet_map.h"
#include "enetkeys.h"

#define DEBUG_ENETMAP if(0)

/*FIX: Need to change the values to suit the shared mem.*/
#define FIRST_TOP_LEVEL_SLOT_AVAILABLE ((uint32_t)18)
#define TOP_LEVEL_SLOTS_AVAILABLE      ((uint32_t)6)
#define MB_PER_TOP_LEVEL_SLOT  ((uint32_t)128)
#define BYTES_PER_TOP_LEVEL_SLOT ((MB_PER_TOP_LEVEL_SLOT * (uint32_t)1024 * \
                                                           (uint32_t)1024))
#define MMGR_SPACE_WIDTH  ((uint32_t)2048)
#define MMGR_SPACE_HEIGHT ((uint32_t)1024)
#define MMGR_SPACE_DEPTH     ((uint32_t)SHARED_BUFFER_DEPTH)
#define SPACE_OFFSET (MMGR_SPACE_WIDTH * MMGR_SPACE_HEIGHT * MMGR_SPACE_DEPTH)
#define MB_PER_SUBSPACE (SPACE_OFFSET / ((uint32_t)1024) / ((uint32_t)1024))
#define SUBSPACES_PER_TOP_LEVEL_SLOT (MB_PER_TOP_LEVEL_SLOT / MB_PER_SUBSPACE)
#define SLOTS_PER_SUBSPACE ((uint32_t)2)
#define MAX_SPACES (TOP_LEVEL_SLOTS_AVAILABLE * SUBSPACES_PER_TOP_LEVEL_SLOT)
#define SUBSPACE_START_ADDRESS (FIRST_TOP_LEVEL_SLOT_AVAILABLE * \
                                BYTES_PER_TOP_LEVEL_SLOT)
static bool space_indexes[MAX_SPACES] = { false };


uint32_t 
enet_MapClient_single_buff(cap_t kr_bank,cap_t kr_newspace,
			   uint32_t *buffer_addr)
{
  result_t result = RC_capros_key_RequestError; /* until proven otherwise */
  uint32_t u;
  uint32_t top_level_slot = 0;
  uint32_t buffer_id;
  
  if (buffer_addr == NULL)
    return RC_capros_key_RequestError;

  result = process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  if (result != RC_OK)
    return result;

  /* FIX: for now, insert the new subspace into the lss 3 layer of
     nodes. */
  for (u = 0; u < MAX_SPACES; u++)
    if (space_indexes[u] == false)
      break;

  if (u == MAX_SPACES)
    kdprintf(KR_OSTREAM, "** Enet_MapClient(): need paging algorithm here!\n");

  buffer_id = u;

  top_level_slot = u / SUBSPACES_PER_TOP_LEVEL_SLOT;

  DEBUG_ENETMAP
    kprintf(KR_OSTREAM,"Enet_MapClient():copying node key for top level slot\n"
	    " [%u] to kr_tmp with u = %u and tlslots = %u\n",
	    top_level_slot + FIRST_TOP_LEVEL_SLOT_AVAILABLE, u, 
	    TOP_LEVEL_SLOTS_AVAILABLE);
  
  result = node_copy(KR_SCRATCH, 
		     top_level_slot + FIRST_TOP_LEVEL_SLOT_AVAILABLE,
		     KR_SCRATCH);
  if (result != RC_OK)
    return result;
 
  {
    uint32_t s = (u % SUBSPACES_PER_TOP_LEVEL_SLOT) * SLOTS_PER_SUBSPACE;
    
    result = node_swap(KR_SCRATCH, s, kr_newspace, KR_VOID);
    if (result != RC_OK) { 
      kprintf(KR_OSTREAM,"Swap failed for %d",s);
      return result;
    }

    /* ** VERY IMPORTANT **: Insert a local window key in the next
       slot to access this space properly! */
    result = addrspace_insert_lwk(KR_SCRATCH, s, s+1, 3);
    if (result != RC_OK)
      return result;
  }
  
  DEBUG_ENETMAP
    kprintf(KR_OSTREAM, "enet_map::enet_MapClient(): inserted subspace "
	    "for id %u \n   into slot [%u] of lss three node [%u]\n",
	    u,u % 16,top_level_slot + FIRST_TOP_LEVEL_SLOT_AVAILABLE);
  
  space_indexes[u] = true;
  
  /* The buffer is mapped with the network stack here */
  *buffer_addr = (SUBSPACE_START_ADDRESS + buffer_id*SPACE_OFFSET);
  
  DEBUG_ENETMAP
    kprintf(KR_OSTREAM,"enet_map:: Memory mapped at %x",
	    *buffer_addr);
  
  return RC_OK;
}


uint32_t 
enet_MapClient(cap_t krBank,cap_t key1,cap_t key2,cap_t key3,cap_t key4,
	       uint32_t *addr1,uint32_t *addr2,
	       uint32_t *addr3,uint32_t *addr4)
{
  result_t result;	

  /*Map sector 1 buffer*/
  result = enet_MapClient_single_buff(krBank,key1,addr1);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Enet unsuccessfully mapped client %d",result);
    return RC_ENET_client_space_full;
  }
	
  /* Map sector 2 buffer */
  result = enet_MapClient_single_buff(krBank,key2,addr2);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Enet unsuccessfully mapped client %d",result);
    return RC_ENET_client_space_full;
  }
  
  /*Map sector 3 buffer*/
  result = enet_MapClient_single_buff(krBank,key3,addr3);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Enet unsuccessfully mapped client %d",result);
    return RC_ENET_client_space_full;
  }
	
  /* Map sector 4 buffer */
  result = enet_MapClient_single_buff(krBank,key4,addr4);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Enet unsuccessfully mapped client %d",result);
    return RC_ENET_client_space_full;
  }

  return RC_OK;
}

uint32_t 
enet_MapStack(cap_t krBank,cap_t key1,cap_t key2,
	      uint32_t *addr1,uint32_t *addr2)
{
  result_t result;
  
  result = enet_MapClient_single_buff(krBank,key1,addr1);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Enet unsuccessfully mapped stack %d",result);
    return RC_ENET_stack_space_full;
  }
  
  result = enet_MapClient_single_buff(krBank,key2,addr2);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Enet unsuccessfully mapped stack %d",result);
    return RC_ENET_stack_space_full;
  }
  return RC_OK;
}
