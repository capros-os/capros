/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <disk/NPODescr.h>

#include <idl/capros/key.h>
#include <idl/capros/Range.h>
#include <idl/capros/Number.h>
#include <idl/capros/Node.h>
#include <idl/capros/Page.h>

#include <domain/Runtime.h>
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

  /* On a big bang, the space bank will clear every object the first time
   * it allocates it. Objects are always cleared when deallocated
   * (so they don't have to be saved on disk),
   * so they do not need to be cleared on subsequent allocations.
   * All objects in this range from startOID up to (not including)
   * endClearedOIDs have been cleared.
   *
   * This logic allows one to do a big bang without formatting the disk
   * to clear all the unallocated objects,
   * which is helpful during development. */
  OID endClearedOIDs;

  uint32_t firstSubmapFrame;
  uint32_t*   srmBase;

  /* for optimization */
  uint32_t    nFrames;		/* number of frames this range covers    */
  uint64_t nAvailFrames;	/* number of available frames.
				Does not include frames in caches. */
  uint32_t    nSubmaps;		/* number of SubRangeMaps covering
				   this range */
} Range;

Range RangeTable[MAX_RANGES];

#define MAX_CACHE_ENT 64
typedef struct AllocCache_s {
  uint32_t ndx;
  uint32_t top;
  OID  oid[MAX_CACHE_ENT];
} AllocCache;

/* NCACHE is the number of caches, and should be about the largest
 * number of distinct banks expected. */
#define NCACHE 64

/* The node frame cache is segregated to capture recently DE-allocated
   nodes.  It exists to reduce the number of retags, as switching from
   nodes to pages is expensive if any of the dead nodes remain live in
   the checkpoint area. */
AllocCache page_cache[NCACHE];
AllocCache node_cache[NCACHE];

AllocCache * caches[NUM_BASE_TYPES] = {
  [capros_Range_otPage] = page_cache,
  [capros_Range_otNode] = node_cache
};

static uint32_t curRanges = 0;

uint32_t NextRangeSrmFrame = 0;

static Range * range_install(uint32_t kr, uint32_t reservedFrames);

/* Format of the volsize node:
   Slot                  Contents
   capros_Range_otPage     Number key containing number of data pages 
                         used by mkimage
   capros_Range_otNode     Number key containing number of nodes 
                         used by mkimage
 */

uint64_t nNode;		// number of allocated nodes
uint32_t nNodeFrames;	// number of frames of allocated nodes
uint64_t nDataPage;	// number of allocated pages
uint32_t nDataPageFrames;	// number of frames of allocated pages
uint32_t nAllocFrames;	// nNodeFrames + nDataPageFrames

void
ob_init(void)
{
  int i, j;
  
  DEBUG(init) kdprintf(KR_OSTREAM, "Initializing allocation cache\n");

  for (j = 0; j < NUM_BASE_TYPES; j++) {
    for (i = 0; i < NCACHE; i++) {
      caches[j][i].top = 0;
      caches[j][i].ndx = 0;
    }
  }
  
  capros_Node_getSlot(KR_VOLSIZE, capros_Range_otNode, KR_TMP);
  capros_Number_get64(KR_TMP, &nNode);
  nNodeFrames = DIVRNDUP(nNode, objects_per_frame[capros_Range_otNode]);
  
  capros_Node_getSlot(KR_VOLSIZE, capros_Range_otPage, KR_TMP);
  capros_Number_get64(KR_TMP, &nDataPage);
  nDataPageFrames
    = DIVRNDUP(nDataPage, objects_per_frame[capros_Range_otPage]);

  nAllocFrames = nDataPageFrames + nNodeFrames;

  DEBUG(init) kdprintf(KR_OSTREAM, "Installing range keys\n");

  /* search through all the keys in the volsize, looking for
   * range keys
   */
  for (i = 0; i < EROS_NODE_SIZE; i++) {
    uint32_t result, keyType;
    capros_Node_getSlot(KR_VOLSIZE, i, KR_TMP);
    result = capros_key_getType(KR_TMP, &keyType);
    if (result == RC_OK && keyType == AKT_Range) {
      DEBUG(init) kprintf(KR_OSTREAM, "Range key in volsize slot %d\n", i);
      if (i == volsize_range) {
        // This is the range that includes the preallocated objects.
        Range * range = range_install(KR_TMP, nAllocFrames);
        assert(range);

        /* Now add all of the allocated storage to the prime bank's allocated
           tree.  Note that we mark the entire frame allocated in the tree;
           the unused entries lie in the residual word. */

        OID firstNodeOid = range->startOID;
        OID firstDataPageOid = firstNodeOid + FrameToOID(nNodeFrames);

        BankPreallType(&primebank, capros_Range_otNode,
                       firstNodeOid, nNode);
        BankPreallType(&primebank, capros_Range_otPage,
                       firstDataPageOid, nDataPage);
      } else {
        Range * range = range_install(KR_TMP, 0);
        assert(range);
        (void)range;	// not otherwise used
      }
    }
  }
  
  DEBUG(init) kdprintf(KR_OSTREAM,
      "Allocated frames are marked. RangeTable[0].nAvail=0x%llx\n",
      RangeTable[0].nAvailFrames);

#if 0
  init_cache();
#endif
}

static bool
fill_cache(AllocCache* ac)
{
  int i;

  for (i = 0; i < MAX_RANGES; i++) {
    if (RangeTable[i].nAvailFrames)
      goto found;
  }
  return false;	// no range has any available frames :-(

found: ;
  Range * r = &RangeTable[i];
  uint32_t * map = r->srmBase;
  uint32_t nWords = r->nSubmaps * (EROS_PAGE_SIZE/sizeof(uint32_t));

  assert (ac->ndx == ac->top);
  
  ac->ndx = 0;
  ac->top = 0;
  
  for (i = 0; i < nWords && ac->top < MAX_CACHE_ENT; i++) {
    if (map[i] == 0)
      continue;

    while (map[i] && ac->top < MAX_CACHE_ENT) {
      uint32_t w = ffs(map[i]) - 1;
      uint32_t frame = i * UINT32_BITS + w;
      OID oid = r->startOID + (frame * EROS_OBJECTS_PER_FRAME);
      // Ensure the frame has been cleared:
      while (r->endClearedOIDs <= oid) {
        result_t retval;
        DEBUG(cache) kprintf(KR_OSTREAM, "SB clearing %#llx\n",
                             r->endClearedOIDs);
        // It is easiest to clear it as a page.
        // It might be better to clear it as the type of this cache.
        retval = GetCapAndRetype(capros_Range_otPage,
                   r->endClearedOIDs, KR_TEMP0);
        assert(retval == RC_OK);
        retval = capros_Page_zero(KR_TEMP0);
        assert(retval == RC_OK);
        r->endClearedOIDs += EROS_OBJECTS_PER_FRAME;
      }
      ac->oid[ac->top++] = oid;
      map[i] &= ~(1u << w);
      r->nAvailFrames--;  /* We've taken one */
    }
  }

  DEBUG(cache) kdprintf(KR_OSTREAM, "Filled cache %#x, top=%d\n", ac, ac->top);

  return (ac->top == 0) ? false : true;
}

/* Allocates a new frame of the specified type.
 * If successful, returns RC_OK and returns the OID in *oid.
 * Otherwise returns RC_capros_SpaceBank_LimitReached.
 */
uint32_t
ob_AllocFrame(Bank * bank, OID * oid, unsigned int baseType)
{
  int i, j;
  const uint32_t ndx = ((uint32_t) bank) % NCACHE;
  AllocCache * cache = caches[baseType];
  AllocCache *ac = &cache[ndx];

  assert (ac->ndx <= ac->top);

  /* Look in this bank's cache of the same type,
  for locality on the disk. */
  if ( (ac->ndx != ac->top) || fill_cache(ac) ) {
    *oid = ac->oid[ac->ndx++];
    return RC_OK;
  }

  DEBUG(cache) kprintf(KR_OSTREAM,
                        "Bank %d type %d cache %#x is empty, top=%d\n",
                        ndx, baseType, ac, ac->top);

  /* Look in any bank's cache of the same type,
  to avoid retyping a frame. */
  for (i = 0; i < NCACHE; i++) {
    ac = &cache[(i+ndx)%NCACHE];
    if (ac->ndx != ac->top) {
      DEBUG(cache) kprintf(KR_OSTREAM, "Bank %d stealing %d from cache %d\n",
                            ndx, baseType, i+ndx);
      *oid = ac->oid[ac->ndx++];
      return RC_OK;
    }
  }

  /* Look in any bank's cache of any type. We're desperate. */
  for (j = 0; j < NUM_BASE_TYPES; j++) {
    if (j != baseType) {	// because baseType was searched above
      AllocCache * anyCache = caches[j];
      for (i = 0; i < NCACHE; i++) {
        ac = &anyCache[(i+ndx)%NCACHE];
        if (ac->ndx != ac->top) {
          DEBUG(cache) kprintf(KR_OSTREAM,
                                "Bank %d wants %d stealing %d from cache %d\n",
                                ndx, baseType, j, i+ndx);
          *oid = ac->oid[ac->ndx++];
          return RC_OK;
        }
      }
    }
    // else we've already searched this cache
  }

  DEBUG(limit)
    kprintf(KR_OSTREAM, "ob_AllocFrame failed. ac=0x%x rng=0x%x\n",
            ac, &RangeTable);

  return RC_capros_SpaceBank_LimitReached;
}

/* Mark the frame identified by oid as free,
 * updating subrange and range maps accordingly.
 * baseType is the object's base type. */
void
ob_ReleaseFrame(Bank *bank, OID oid, unsigned int baseType)
{
  int i;
  uint32_t bit;
  uint64_t offset;
  
  for (i = 0; i < MAX_RANGES; i++) {
    if (RangeTable[i].startOID <= oid && oid < RangeTable[i].endOID)
      goto found;
  }
  kpanic(KR_OSTREAM, "Released OID not in any known range!\n");

found: ;
  /* Try returning this frame to the allocation cache to encourage
     rapid reallocation of recently demolished objects. */
  {
    const uint32_t ndx = ((uint32_t) bank) % NCACHE;
    AllocCache * ac = &caches[baseType][ndx];

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
    OID oid = range->startOID + FrameToOID(range->firstSubmapFrame + map);

    if (GetCapAndRetype(capros_Range_otPage, oid, KR_TMP)
        != RC_OK)
      kpanic(KR_OSTREAM, "Couldn't get page key %#llx for submap, range=%#x\n",
             oid, range);

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
  
  DEBUG(init) kdprintf(KR_OSTREAM, "Marking %d submap frames\n",
                       range->nSubmaps);
    
  bzero(addr, range->nSubmaps * EROS_PAGE_SIZE);

  // If there are any allocated frames (between 0 and range->firstSubmapFrame),
  // leave them marked allocated.
  // The submaps are never marked available.
  for (frame = range->firstSubmapFrame + range->nSubmaps;
       frame < range->nFrames; frame++) {
    addr[frame / UINT32_BITS] |= (1u << (frame % UINT32_BITS));
  }

  DEBUG(init) kdprintf(KR_OSTREAM, "  ... done marking submap frame avail\n");
}

// Returns NULL if unsuccessful.
Range *
range_install(uint32_t kr, uint32_t reservedFrames)
{
  uint64_t len;
  uint64_t oid, endOID;
  uint32_t nFrames;
  uint32_t nSubMaps;

  Range *myRange;
  
  if (curRanges == MAX_RANGES)
    return NULL;

  /* Divine the length of the range: */
  if (capros_Range_query(kr, &len) != RC_OK)
    kpanic(KR_OSTREAM, "Range refused query!\n");

  /* And its start OID: */
  if (capros_Range_compare(KR_SRANGE, kr, 0, &oid) != RC_OK)
    kpanic(KR_OSTREAM, "Range refused inclusion test!\n");

  endOID = oid + len;

#if 0 /* JWA note:  This might be potentially usefull code later */
  {
    int index;
    /* check for duplicates or overlaps 
       Overlaps are fine, as long as they are entirely within one previously 
       inserted range*/
    for (index = 0; index < curRanges; index++) {
      if (RangeTable[index].startOID <= oid
	  && RangeTable[index].endOID > oid) {
	/* this range contains our beginning */
	return NULL;
      } else if (RangeTable[index].startOID > oid
		 && RangeTable[index].startOID < endOID) {
	/* we contain their start -- this is bad */
	return NULL; /* we cannot currently extend ranges */
      } 
    }  
  }
#endif
  
  nFrames = len / EROS_OBJECTS_PER_FRAME;
  nSubMaps = DIVRNDUP(nFrames, EROS_PAGE_SIZE * 8);

  if (NextRangeSrmFrame + nSubMaps > (SRM_TOP - SRM_BASE) / EROS_PAGE_SIZE)
    kpanic(KR_OSTREAM, "No more room at SRM_BASE!\n");

  if (reservedFrames + nSubMaps > nFrames)
    kpanic(KR_OSTREAM, "Too many preallocated frames.\n");

  myRange = &RangeTable[curRanges++]; /* already checked bounds */
  
  myRange->nAvailFrames = nFrames - reservedFrames - nSubMaps;
  myRange->startOID = oid;
  myRange->endOID = endOID;
  /* Reserved frames are initialized in preload_Init(),
   * and we initialize submaps. All others need to be initialized: */
  myRange->endClearedOIDs = oid + FrameToOID(reservedFrames + nSubMaps);
  myRange->length = len;
  myRange->firstSubmapFrame = reservedFrames;
  myRange->srmBase
    = (uint32_t *) (SRM_BASE + (NextRangeSrmFrame * EROS_PAGE_SIZE));
  myRange->nFrames = nFrames;
  myRange->nSubmaps = nSubMaps;

  DEBUG(init) kprintf(KR_OSTREAM,
      "Installed range %#x [0x%llx,0x%llx) rsrvd %d, subm=%d, avail=%lld\n",
      myRange, oid, endOID, reservedFrames, nSubMaps, myRange->nAvailFrames);
  
  map_range(myRange);
  init_range_map(myRange);
  
  bank0.limit += nFrames - nSubMaps;
  
  NextRangeSrmFrame += nSubMaps;
  
  return myRange;
}
