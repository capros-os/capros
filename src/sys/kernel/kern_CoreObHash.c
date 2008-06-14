/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <arch-kerninc/KernTune.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectHeader.h>

#define dbg_hash 0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Following hash will slightly bias against buckets whose index is 0
 * mod 8, but gives a better good distribution of nodes than anything
 * else I could come up with that didn't use a multiply operation.
 * Pages will still give an even distribution, and it's important that
 * nodes not cluster in the hash buckets (arguably MORE important than
 * avoiding undesirable page clusters). 
 */

#if EROS_OBJECTS_PER_FRAME == 256
#define bucket32_ndx(oid32) \
   ((((oid32) >> 8) ^ (((oid32) & 0xff) << 9)) % KTUNE_NOBBUCKETS)
#elif EROS_OBJECTS_PER_FRAME == 16
#define bucket32_ndx(oid32) \
   ((((oid32) >> 4) ^ (((oid32) & 0xf) << 9)) % KTUNE_NOBBUCKETS)
#else
#define bucket32_ndx(oid) \
  ((((oid) % EROS_OBJECTS_PER_FRAME) == 0) ? \
   ((oid/EROS_OBJECTS_PER_FRAME) % KTUNE_NOBBUCKETS) : \
   ((oid) % KTUNE_NOBBUCKETS))
#endif

#define bucket_ndx(oid)  bucket32_ndx((uint32_t)oid)
   

/* static to ensure that it ends up in BSS: */
static ObjectHeader* ObBucket[KTUNE_NOBBUCKETS];
#ifdef OPTION_KERN_STATS
static uint64_t useCount[KTUNE_NOBBUCKETS];	/* for histograms */
#endif

#ifdef OPTION_DDB
void
objH_ddb_dump_hash_hist()
{
  uint32_t i = 0;
  extern void db_printf(const char *fmt, ...);

#ifdef OPTION_KERN_STATS
  printf("Usage counts for object buckets:\n");
  for (i = 0; i < KTUNE_NOBBUCKETS; i++)
    printf("Bucket %d: uses 0x%08x%08x\n", i,
           (uint32_t) (useCount[i]>>32),
           (uint32_t) (useCount[i])
           );
#else
  printf("No kernel usage statistics collected\n");
#endif
}

void
objH_ddb_dump_bucket(uint32_t bucket)
{
  extern void db_printf(const char *fmt, ...);

  uint32_t i;
  ObjectHeader *ob;
  
  for (i = 0, ob = ObBucket[bucket];
       ob->hashChainNext; ob = ob->hashChainNext, i++) {
    printf("%3x ", i);
    objH_ddb_dump(ob);
  }
}
#endif

/* thisPtr must not be a free object. */
void
objH_Intern(ObjectHeader* thisPtr)
{
  uint32_t ndx = 0;
  assert(objH_GetFlags(thisPtr, OFLG_CURRENT));
  
  ndx = bucket_ndx(thisPtr->oid);
  
  DEBUG(hash) printf("Interning obhdr 0x%08x oid=%#llx\n",
                     thisPtr, thisPtr->oid);
  assert(ObBucket[ndx] != thisPtr);
  
  thisPtr->hashChainNext = ObBucket[ndx];
  ObBucket[ndx] = thisPtr;
#ifdef OPTION_KERN_STATS
  useCount[ndx]++;	/* for histograms */
#endif
}

void
objH_Unintern(ObjectHeader* thisPtr)
{
  uint32_t ndx = bucket_ndx(thisPtr->oid);
  
  /*  assert(!isLocked()); */
  
  if (ObBucket[ndx] == 0)
    return;

  if (ObBucket[ndx] == thisPtr) {
    ObBucket[ndx] = ObBucket[ndx]->hashChainNext;
#ifdef OPTION_KERN_STATS
    useCount[ndx]--;	/* for histograms */
#endif
  }
  else {
    ObjectHeader *ob;
    
    for (ob = ObBucket[ndx]; ob->hashChainNext; ob = ob->hashChainNext)
      if (ob->hashChainNext == thisPtr) {
	ob->hashChainNext = ob->hashChainNext->hashChainNext;
#ifdef OPTION_KERN_STATS
	useCount[ndx]--;	/* for histograms */
#endif
	break;
      }
  }
}

/* type must be ot_PtTagPot, ot_PtObjPot, or zero for any object */
ObjectHeader *
objH_Lookup(OID oid, unsigned int type)
{
  ObjectHeader * pOb;
  
  DEBUG(hash) printf("Lookup oid=%#llx\n", oid);
  
  for (pOb = ObBucket[bucket_ndx(oid)]; pOb; pOb = pOb->hashChainNext) {
    DEBUG(hash) printf("ObHdr is 0x%08x oid is %#llx\n", pOb, pOb->oid);

    if (pOb->oid == oid) {
      unsigned int obType = pOb->obType;
      if (obType < ot_PtLAST_OBJECT_TYPE)
        obType = 0;	// not a pot
      if (obType == type) {
        DEBUG(hash) printf("Found oid %#llx\n", oid);
        return pOb;
      }
    }
  }

  DEBUG(hash) printf("Not found\n");
  return NULL;
}
