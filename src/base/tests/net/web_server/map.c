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

/** Map allocated space into process address space 
 */

#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>

#include <idl/eros/key.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <addrspace/addrspace.h>

#include "webs_keys.h"

#define DEBUG_MAP if (0)

/* Shared memory space dimensions */
#define SHARED_BUFFER_WIDTH   2048
#define SHARED_BUFFER_HEIGHT  1024
/*  (in bytes...) */
#define SHARED_BUFFER_DEPTH      4

/* macro for looking at the address of an object */
#define ADDRESS(w) ((uint32_t)w)

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
map_single_buff(cap_t kr_bank,cap_t kr_newspace,
		uint32_t *buffer_addr)
{
  result_t result = RC_eros_key_RequestError; /* until proven otherwise */
  uint32_t u;
  uint32_t top_level_slot = 0;
  uint32_t buffer_id;
  
  if (buffer_addr == NULL)
    return RC_eros_key_RequestError;

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

  DEBUG_MAP
    kprintf(KR_OSTREAM,"Map:copying node key for top level slot\n"
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
  
  DEBUG_MAP
    kprintf(KR_OSTREAM, "map::: inserted subspace "
	    "for id %u \n   into slot [%u] of lss three node [%u]\n",
	    u,u % 16,top_level_slot + FIRST_TOP_LEVEL_SLOT_AVAILABLE);
  
  space_indexes[u] = true;
  
  /* The buffer is mapped with the network stack here */
  *buffer_addr = (SUBSPACE_START_ADDRESS + buffer_id*SPACE_OFFSET);
  
  DEBUG_MAP
    kprintf(KR_OSTREAM,"map:: Memory mapped at %x",
	    *buffer_addr);
  
  return RC_OK;
}
