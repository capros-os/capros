/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2006, Strawberry Development Group.
 *
 * This file is part of the EROS Operating System.
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

#include <string.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>
#include <idl/eros/Range.h>
#include <idl/eros/Number.h>

#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>

#include "assert.h"
#include "misc.h"
#include "debug.h"
#include "spacebank.h"
#include "ObjSpace.h"
#include "Bank.h"

typedef struct Range_s {
  OID     startOID;
  OID     endOID;
  uint64_t length;
  uint32_t    allocBase;	/* starting FRAME in range map for
				   this range */
  uint32_t*   srmBase;

  /* for optimization */
  uint32_t    nFrames;		/* number of frames this range covers    */
  uint64_t nAvailFrames;	/* number of AVAILABLE frames */
  uint32_t    nSubmaps;		/* number of SubRangeMaps covering
				   this range */
} Range;

Range RangeTable[MAX_RANGES];

#define MAX_CACHE_ENT 64
#define NCACHE 128
typedef struct AllocCache_s {
  uint32_t ndx;
  uint32_t top;
  OID  oid[MAX_CACHE_ENT];
} AllocCache;

/* The node frame cache is segregated to capture recently DE-allocated
   nodes.  It exists to reduce the number of retags, as switching from
   nodes to pages is expensive if any of the dead nodes remain live in
   the checkpoint area. */
AllocCache page_cache[NCACHE];
AllocCache node_cache[NCACHE];

static uint32_t curRanges = 0;

uint32_t NextRangeSrmFrame = 0;

static uint32_t range_install(uint32_t kr);

/* Format of the volsize node:
   Slot                  Contents
   eros_Range_otPage     Number key containing number of data pages 
                         used by mkimage
   eros_Range_otNode     Number key containing number of nodes 
                         used by mkimage
 */

static void
mark_allocated_frames(Range *range0)
{
  int i;
  uint64_t nNode;
  uint64_t nDataPage;
  uint32_t nFrames = 0;
  uint32_t nNodeFrames = 0;
  uint32_t nDataPageFrames = 0;

  OID firstNodeOid = range0->nSubmaps * EROS_OBJECTS_PER_FRAME;
  OID firstDataPageOid;
  
  node_copy(KR_VOLSIZE, eros_Range_otNode, KR_TMP);
  eros_Number_getDoubleWord(KR_TMP, &nNode);

  nNodeFrames = DIVRNDUP(nNode, objects_per_frame[eros_Range_otNode]);

  firstDataPageOid = firstNodeOid + (nNodeFrames * EROS_OBJECTS_PER_FRAME);
  
  node_copy(KR_VOLSIZE, eros_Range_otPage, KR_TMP);
  eros_Number_getDoubleWord(KR_TMP, &nDataPage);

  nDataPageFrames += DIVRNDUP(nDataPage, objects_per_frame[eros_Range_otPage]);

  nFrames = nDataPageFrames + nNodeFrames;
  
  if (nFrames + range0->nSubmaps > RangeTable[0].nFrames)
    kpanic(KR_OSTREAM, "Too many preallocated frames.\n");
  
  DEBUG(init)
    kdprintf(KR_OSTREAM,
	     "Marking %d submaps allocated\n",
	     range0->nSubmaps);
	   
  for (i = 0; i < nFrames; i++) {
    uint32_t frame = i + range0->nSubmaps;
    
    uint32_t bit = frame % UINT32_BITS;
    range0->srmBase[frame / UINT32_BITS] &= ~(1u << bit);
    range0->nAvailFrames--;
  }

  /* Now add all of the allocate storage to the prime bank's allocated
     tree.  Note that we mark the entire frame allocated in the tree;
     the unused entries lie in the residual word. */

  BankPreallType(&primebank, eros_Range_otNode,     firstNodeOid, nNode);
  BankPreallType(&primebank, eros_Range_otPage, firstDataPageOid, nDataPage);
}

void
ob_init(void)
{
  int i;

  DEBUG(init) kdprintf(KR_OSTREAM, "Installing range keys\n");

  /* search through all the keys in the volsize, looking for
   * range keys
   */
  for (i = 0; i < EROS_NODE_SIZE; i++) {
    uint32_t result, keyType;
    node_copy(KR_VOLSIZE, i, KR_TMP);
    result = eros_key_getType(KR_TMP, &keyType);
    if (result == RC_OK && keyType == AKT_Range)
      range_install(KR_TMP);
  }
  
  DEBUG(init) kdprintf(KR_OSTREAM, "Initializing allocation cache\n");

  for (i = 0; i < NCACHE; i++) {
    page_cache[i].top = 0;
    page_cache[i].ndx = 0;
    node_cache[i].top = 0;
    node_cache[i].ndx = 0;
  }
    
  DEBUG(init) kdprintf(KR_OSTREAM, "Marking allocated frames\n");

  mark_allocated_frames(&RangeTable[0]);
  
  DEBUG(init) kdprintf(KR_OSTREAM, "Allocated frames are marked\n");

#if 0
  init_cache();
#endif
}

static bool
fill_cache(AllocCache* ac)
{
  int i;
  uint32_t nWords;
  Range *r;
  uint32_t *map;

  for (i = 0; i < MAX_RANGES; i++) {
    if (RangeTable[i].nAvailFrames)
      break;
  }

  if (i == MAX_RANGES)
    return false;

  r = &RangeTable[i];
  map = RangeTable[i].srmBase;
  
  nWords = RangeTable[i].nSubmaps * (EROS_PAGE_SIZE/sizeof(uint32_t));

  assert (ac->ndx == ac->top);
  
  ac->ndx = 0;
  ac->top = 0;
  
  for (i = 0; i < nWords && ac->top < MAX_CACHE_ENT; i++) {
    if (map[i] == 0)
      continue;

    while (map[i] && ac->top < MAX_CACHE_ENT) {
      uint32_t w = ffs(map[i]) - 1;
      uint32_t frame = i * UINT32_BITS + w;
      ac->oid[ac->top++] = r->startOID + (frame * EROS_OBJECTS_PER_FRAME);
      map[i] &= ~(1u << w);
      r->nAvailFrames--;  /* We've taken one */
    }
  }

  return (ac->top == 0) ? false : true;
}

uint32_t
ob_AllocFrame(Bank *bank, OID *oid, bool wantNode)
{
  int i;
  uint32_t ndx = ((uint32_t) bank) % NCACHE;
  AllocCache *cache = wantNode ? node_cache : page_cache;
  AllocCache *ac = &cache[ndx];

  assert (ac->ndx <= ac->top);

  if ( (ac->ndx != ac->top) || fill_cache(ac) ) {
    *oid = ac->oid[ac->ndx++];
    return RC_OK;
  }

  for (i = 1; i < NCACHE; i++) {
    ac = &cache[(i+ndx)%NCACHE];
    if (ac->ndx != ac->top) {
      *oid = ac->oid[ac->ndx++];
      return RC_OK;
    }
  }

  return RC_SB_LimitReached;
}

void
ob_ReleaseFrame(Bank *bank, OID oid, bool isNode)
{
  int i;
  uint32_t bit;
  uint64_t offset;
  
  for (i = 0; i < MAX_RANGES; i++) {
    if (RangeTable[i].startOID <= oid && oid < RangeTable[i].endOID)
      break;
  }

  if (i == MAX_RANGES)
    kpanic(KR_OSTREAM, "Released OID too large!\n");


  /* Try returning this frame to the allocation cache to encourage
     rapid reallocation of recently demolished objects. */
  {
    uint32_t ndx = ((uint32_t) bank) % NCACHE;
    AllocCache *ac = isNode ? &node_cache[ndx] : &page_cache[ndx];

    assert (ac->ndx <= ac->top);

    if (ac->ndx > 0) {
      ac->ndx--;
      ac->oid[ac->ndx] = oid;
      return;
    }

    DEBUG(realloc)
      kprintf(KR_OSTREAM, "Cannot reuse returned frame!\n");
    /* In the ideal universe we would like to return the OLDEST frame
       to the bitmap if the cache is full, but SHAP isn't up to
       thinking through rotating the line at the moment. */
  }

  /* If that doesn't work, chuck the frame back into the general
     pool. */
  offset = (oid - RangeTable[i].startOID) / EROS_OBJECTS_PER_FRAME;

  bit = offset % UINT32_BITS;
  
  if ((RangeTable[i].srmBase[offset / UINT32_BITS] & (1u<<bit)) != 0) {
    /* BAD -- we are trying to deallocate something that is already
       marked as allocated -- DUMP anything cogent. */
    kpanic(KR_OSTREAM, 
	   "Ack! ob_ReleaseFrame trying to release a non-allocated frame!\n"
	   "       OID: 0x"DW_HEX"\n"
           "RangeTable[i]: 0x%08x i: %02x\n"
	   "startOID: 0x"DW_HEX"  endOID: 0x"DW_HEX"\n"
	   "      srmBase: 0x%08x  offset/UINT32_BITS: 0x%08x  bit: 0x%02x\n"
	   "srmBase[offset/UINT32_BITS] = %08x\n",
	   DW_HEX_ARG(oid),
	   &RangeTable[i], i, DW_HEX_ARG(RangeTable[i].startOID),
	    DW_HEX_ARG(RangeTable[i].endOID),
	   RangeTable[i].srmBase, offset/UINT32_BITS, bit,
	   RangeTable[i].srmBase[offset/UINT32_BITS]);	   
  }

  RangeTable[i].srmBase[offset / UINT32_BITS] |= (1u << bit);
  RangeTable[i].nAvailFrames++;
}

static void
map_range(Range *range)
{
  uint32_t map;
  uint32_t addr = (uint32_t) range->srmBase;
  for (map = 0; map < range->nSubmaps; map++, addr += EROS_PAGE_SIZE) {
    OID oid = range->startOID + (map * EROS_OBJECTS_PER_FRAME);

    if (eros_Range_waitPageKey(KR_SRANGE, oid, KR_TMP) != RC_OK)
      kpanic(KR_OSTREAM, "Couldn't get page key for submap\n");

    DEBUG(init) kdprintf(KR_OSTREAM,
                         "Mapping submap oid=0x%08x%08x at 0x%08x\n",
                         (uint32_t) (oid >> 32), (uint32_t) oid, addr);

    /* note from JWA: If I were really paranoid, I might rescind
       then recreate the key, to be sure everything was koscher. */

    if ( heap_insert_page(addr, KR_TMP) == false)
      kpanic(KR_OSTREAM, "Couldn't insert submap into heap\n");
  }

  DEBUG(init) kdprintf(KR_OSTREAM, "Range is mapped at 0x%08x...\n",
		       range->srmBase); 
}

static void
init_range_map(Range *range)
{
  uint32_t frame;
  uint32_t *addr = range->srmBase;
  
  DEBUG(init) kdprintf(KR_OSTREAM, "Marking submap frame availability\n");
    
  bzero(range->srmBase, range->nSubmaps * EROS_PAGE_SIZE);

  /* the submaps are never marked available. */
  for (frame = range->nSubmaps; frame < range->nFrames; frame++) {
    uint32_t bit = frame % UINT32_BITS;
    addr[frame / UINT32_BITS] |= (1u << bit);
  }

  DEBUG(init) kdprintf(KR_OSTREAM, "  ... done marking submap frame avail\n");
}

uint32_t
range_install(uint32_t kr)
{
  uint64_t len;
  uint64_t oid, endOID;
  uint32_t nFrames;
  uint32_t nSubMaps;

  Range *myRange;
  
  if (curRanges == MAX_RANGES)
    return RC_SB_LimitReached;

  /* Divine the length of the range: */
  if (eros_Range_query(kr, &len) != RC_OK)
    kpanic(KR_OSTREAM, "Range refused query!\n");

  /* And its start OID: */
  if (eros_Range_compare(KR_SRANGE, kr, 0, &oid) != RC_OK)
    kpanic(KR_OSTREAM, "Range refused inclusion test!\n");

  endOID = oid + len;

#if 0 /* JWA note:  This might be potentially usefull code later */
  {
    int index;
    /* check for duplicates or overlaps 
       Overlaps are fine, as long as they are entirely within one previously 
       inserted range*/
    for (index = 0; index < curRanges; index++) {
      if (RangeTable[index].startOID < oid
	  && RangeTable[index].endOID > oid) {
	/* this range contains our beginning */
	if (RangeTable[index].endOID <= endOID) {
	  return RC_OK; /* we've already covered that area */
	} else {
	  return RC_SB_LimitReached; /* we cannot currently extend ranges */
	}
      } else if (RangeTable[index].startOID == oid) {
	/* begin at the same place */
	if (RangeTable[index].endOID <= endOID) {
	  return RC_OK; /* we've already covered that area */
	} else {
	  return RC_SB_LimitReached; /* we cannot currently extend ranges */
	}
      } else if (RangeTable[index].startOID > oid
		 && RangeTable[index].startOID < endOID) {
	/* we contain their start -- this is bad */
	return RC_SB_LimitReached; /* we cannot currently extend ranges */
      } 
    }  
  }
#endif
  
  nFrames = len / EROS_OBJECTS_PER_FRAME;
  nSubMaps = DIVRNDUP(nFrames, EROS_PAGE_SIZE * 8);

  if (NextRangeSrmFrame + nSubMaps > (SRM_TOP - SRM_BASE) / EROS_PAGE_SIZE)
    kpanic(KR_OSTREAM, "No more room at SRM_BASE!\n");

  myRange = &RangeTable[curRanges++]; /* already checked bounds */
  
  myRange->startOID = oid;
  myRange->endOID = endOID;
  myRange->length = len;
  myRange->allocBase = NextRangeSrmFrame;
  myRange->srmBase
    = (uint32_t *) (SRM_BASE + (NextRangeSrmFrame * EROS_PAGE_SIZE));
  myRange->nFrames = nFrames;
  myRange->nAvailFrames = nFrames - nSubMaps;
  myRange->nSubmaps = nSubMaps;

  DEBUG(init) kdprintf(KR_OSTREAM, "Installed range [0x%08x%08x,0x%08x%08x)\n",
	   (uint32_t) (oid>>32), (uint32_t) oid,
	   (uint32_t) (endOID>>32), (uint32_t) endOID);
  
  map_range(myRange);
  init_range_map(myRange);
  
  bank0.limit += nFrames - nSubMaps;
  
  NextRangeSrmFrame += nSubMaps;
  
  return RC_OK;
}
