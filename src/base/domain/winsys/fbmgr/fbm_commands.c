/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>

#include <idl/eros/key.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>

#include <graphics/color.h>
#include <addrspace/addrspace.h>

#include <stdlib.h>
#include <string.h>

#include "fbm_commands.h"
#include "drivers/video.h"
#include "../winsyskeys.h"
#include "../global.h"

/* FIX: The following macros are here for documentation more than
anything else. They are based on 32 slots per node and lss of top
level node = 4. When all is said and done, the winsys domain's address
space should be:

i) ProcAddrSpace holds key to top-level LSS=4 node with slot 0
containing key to original address space and slots 1 to 15 containing
local window keys into that original space.

ii) Top-level node slot 16 contains key to memory-mapped area for
video FIFO.

iii) Top-level node slot 17 contains key to memory-mapped area for
video framebuffer.

iv) Top-level node slots 18 to 23 each hold a key to an lss 3 node

v) Top-level node slots 24 to 31 are reserved since they reference the
top 1GB of address space which is for kernel use only.

vi) Each lss 3 node referenced in (iv) above holds the keys to the
shared client subspaces:  Currently one client subspace is 8MB, which
requires two slots in an lss 3 node.  Thus, the first slot is filled
with the key returned to the client on "new_window" calls and the
subsequent slot is filled with a local window key into that space.
Each client window contains a "buffer id" which maps to the specific
root of that window's shared buffer.
*/

#define FIRST_TOP_LEVEL_SLOT_AVAILABLE ((uint32_t)18)
#define TOP_LEVEL_SLOTS_AVAILABLE      ((uint32_t)6)
#define MB_PER_TOP_LEVEL_SLOT  ((uint32_t)128)
#define BYTES_PER_TOP_LEVEL_SLOT ((MB_PER_TOP_LEVEL_SLOT * (uint32_t)1024 * \
                                                           (uint32_t)1024))
#define FBM_SPACE_WIDTH  ((uint32_t)SHARED_BUFFER_WIDTH)
#define FBM_SPACE_HEIGHT ((uint32_t)SHARED_BUFFER_HEIGHT)
#define FBM_SPACE_DEPTH     ((uint32_t)SHARED_BUFFER_DEPTH)
#define SPACE_OFFSET (FBM_SPACE_WIDTH * FBM_SPACE_HEIGHT * FBM_SPACE_DEPTH)
#define MB_PER_SUBSPACE (SPACE_OFFSET / ((uint32_t)1024) / ((uint32_t)1024))
#define SUBSPACES_PER_TOP_LEVEL_SLOT (MB_PER_TOP_LEVEL_SLOT / MB_PER_SUBSPACE)
#define SLOTS_PER_SUBSPACE ((uint32_t)2)
#define MAX_SPACES (TOP_LEVEL_SLOTS_AVAILABLE * SUBSPACES_PER_TOP_LEVEL_SLOT)
#define SUBSPACE_START_ADDRESS (FIRST_TOP_LEVEL_SLOT_AVAILABLE * \
                                BYTES_PER_TOP_LEVEL_SLOT)
static uint8_t *subspaces = (uint8_t *)(SUBSPACE_START_ADDRESS);
static bool space_indexes[MAX_SPACES] = { false };

uint32_t
fbm_MapClient(cap_t kr_bank, cap_t kr_newspace,
	      /* out */ uint32_t *buffer_id)
{
  uint32_t result = RC_eros_key_RequestError; /* until proven otherwise */
  uint32_t u;
  uint32_t top_level_slot = 0;

  if (buffer_id == NULL)
    return RC_eros_key_RequestError;

  /* Now, we need to call the Zero Space Constructor (Factory) to
     create a new address space (on the client's dime) that will hold
     the contents of this new window. */
  if (constructor_request(KR_ZSC, kr_bank, KR_SCHED,
			  KR_VOID, kr_newspace) != RC_OK)
    kdprintf(KR_OSTREAM, 
	     "fbm_MapClient() call to constructor_request FAILED.\n");

  result = process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  if (result != RC_OK)
    return result;

  kprintf(KR_OSTREAM, 
    "fbm_MapClient() finding first free subspace index...\n");

  /* FIX: for now, insert the new subspace into the lss 3 layer of
     nodes. */
  for (u = 0; u < MAX_SPACES; u++)
    if (space_indexes[u] == false)
      break;

  if (u == MAX_SPACES)
    kdprintf(KR_OSTREAM, "** fbm_MapClient(): need paging algorithm here!\n");

  *buffer_id = u;

  top_level_slot = u / SUBSPACES_PER_TOP_LEVEL_SLOT;

  kprintf(KR_OSTREAM, "fbm_MapClient(): copying node key for top level slot\n"
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
    if (result != RC_OK)
      return result;

    /* ** VERY IMPORTANT **: Insert a local window key in the next
       slot to access this space properly! */
    result = addrspace_insert_lwk(KR_SCRATCH, s, s+1, 3);
    if (result != RC_OK)
      return result;
  }

  kprintf(KR_OSTREAM, "fbm_commands::fbm_MapClient(): inserted subspace "
	  "for id %u\n   into slot [%u] of lss three node [%u]\n",
	  u, u % 16, top_level_slot + FIRST_TOP_LEVEL_SLOT_AVAILABLE);

  space_indexes[u] = true;

  return RC_OK;
}

uint32_t
fbm_UnmapClient(uint32_t buffer_id)
{
  uint32_t tmp = 0;
  uint32_t result = RC_eros_key_RequestError; /* until proven otherwise */

  result = process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  if (result != RC_OK)
    return result;

  tmp = buffer_id / SUBSPACES_PER_TOP_LEVEL_SLOT;
  result = node_copy(KR_SCRATCH, tmp + FIRST_TOP_LEVEL_SLOT_AVAILABLE, 
		     KR_SCRATCH);
  if (result != RC_OK)
    return result;

  /* Bash the key to the root of the shared subspace */
  tmp = (buffer_id % SUBSPACES_PER_TOP_LEVEL_SLOT) * SLOTS_PER_SUBSPACE;
  result = node_swap(KR_SCRATCH, tmp,   KR_VOID, KR_VOID);
  if (result != RC_OK)
    return result;

  /* Bash the local window key, too */
  result = node_swap(KR_SCRATCH, tmp+1, KR_VOID, KR_VOID);
  if (result != RC_OK)
    return result;

  space_indexes[buffer_id] = false;

  return RC_OK;
}

void
fbm_doRedraw(uint32_t window_width, uint32_t buffer_id, rect_t bounds, 
	     point_t fb_start)
{
  /* Ask video subsystem to do the "memory copy" */
  if (!video_copy_to_fb((uint32_t *)((uint8_t *)subspaces + 
				     (buffer_id * SPACE_OFFSET)),
			bounds, fb_start, window_width)) {

    kprintf(KR_OSTREAM, "*** fbm_doRedraw(): time to kill an uncooperative "
	    "client!\n");
    /* Kill this client window if the process keeper gets invoked*/
    fbm_UnmapClient(buffer_id);
  }
}

uint32_t
fbm_RedrawClient(uint32_t window_width, uint32_t buffer_id, 
		 rect_t bounds, point_t fb_start)
{
  /* FIX: Use client's subspace key to verify that the subspace
     referenced by 'buffer_id' is the correct one ?? */

  /* Then do the actual redraw */
  fbm_doRedraw(window_width, buffer_id, bounds, fb_start);

  return RC_OK;
}
