/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <kerninc/Node.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/ObjH-inline.h>
#include <kerninc/Activity.h>
#include <kerninc/LogDirectory.h>
#include <disk/TagPot.h>

#define dbg_obsrc	0x20	/* addition of object ranges */
#define dbg_findfirst	0x40	/* finding first subrange */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/****************************************************************
 *
 * Interaction with object sources;
 *
 ****************************************************************/

static DEFQUEUE(SourceWait);

static ObjectRange obRanges[KTUNE_NRNGTBLENTS];
static uint32_t nObRanges = 0;

static ObjectRange lidRanges[KTUNE_NLOGTBLENTS];
static uint32_t nLidRanges = 0;

LID logWrapPoint;

/* On exit, returns u such that ranges[u].start <= oid < ranges[u+1].start .
 * Returns -1 if oid < ranges[0].start . */
static int
LookupRange(OID oid, ObjectRange * ranges, uint32_t nRanges)
{
  // Binary search for the range containing oid.
  int l = 0;
  int u = nRanges - 1;
  while (1) {
    if (u < l)
      break;
    unsigned int i = (l + u)/2;	// midpoint of current range
    if (oid < ranges[i].start)
      u = i-1;
    else
      l = i+1;
  }
  return u;
}

// Find the range containing the specified OID or LID.
static ObjectRange *
LookupOID(OID oid, ObjectRange * ranges, uint32_t nRanges)
{
  int i = LookupRange(oid, ranges, nRanges);
  if (i < 0)
    return NULL;	// oid is before the first range

  ObjectRange * rng = &ranges[i];
  if (oid < rng->end)
    return rng;
  else return NULL;
}

ObjectRange *	// or NULL if failed
AddRange(const ObjectRange * rng, ObjectRange * ranges, uint32_t * pnRanges)
{
  unsigned int i, j;
  uint32_t nRanges = *pnRanges;
  
  DEBUG(obsrc)
    printf("New range: \"%s\" [%#llx,%#llx)\n",
		   rng->source->name, rng->start, rng->end);

  i = LookupRange(rng->start, ranges, nRanges) + 1;	// may be == nRanges

  /* Because we do not yet implement mirroring, we check here
   * that the new range does not overlap any existing range. */

  if (i > 0) {
    if (ranges[i-1].end > rng->start) {
      dprintf(true,
              "New range [%#llx,%#llx) overlaps existing range [%#llx,%#llx)\n",
              rng->start, rng->end, ranges[i-1].start, ranges[i-1].end);
    }
  }
  if (i < nRanges)
    if (ranges[i].start < rng->end) {
      dprintf(true,
              "New range [%#llx,%#llx) overlaps existing range [%#llx,%#llx)\n",
              rng->start, rng->end, ranges[i].start, ranges[i].end);
    }
  
  DEBUG(obsrc)
    printf("AddRange: found %d nRanges=%d\n", i, nRanges);

  // Move higher ranges up.
  for (j = nRanges++; j > i; j--)
      ranges[j] = ranges[j-1];
  (*pnRanges)++;
 
  ranges[i] = *rng;

  sq_WakeAll(&SourceWait, false);
  
  DEBUG(obsrc)
    printf("AddRange: returning\n");

  return &ranges[i];
}

bool
objC_AddRange(const ObjectRange * rng)
{
  if (nObRanges == KTUNE_NRNGTBLENTS)
    fatal("Limit on total object ranges exceeded\n");

  return AddRange(rng, obRanges, &nObRanges);
}

bool
AddLIDRange(const ObjectRange * rng)
{
  if (nLidRanges == KTUNE_NLOGTBLENTS)
    fatal("Limit on total object ranges exceeded\n");

  ObjectRange * newRng = AddRange(rng, lidRanges, &nLidRanges);
  if (! newRng)
    return false;

  restart_LIDMounted(newRng);
  return true;
}

bool
objC_HaveSource(OID oid)
{
  return (bool) LookupOID(oid, obRanges, nObRanges);
}

// Find the ObjectRange containing this LID.
// Return NULL if none.
ObjectRange *
LidToRange(LID lid)
{
  return LookupOID(lid, lidRanges, nLidRanges);
}

// Calculate logWrapPoint, which is the smallest LID such that
// all smaller LIDs are mounted.
void
CalcLogExtent(void)
{
  LID limit = 0;	// end of what we have found so far
  for (;;) {
    ObjectRange * rng = LidToRange(limit);
    if (! rng) {
      logWrapPoint = limit;
      return;
    }
    limit = rng->end;
  }
}

// May Yield.
ObjectLocator
GetObjectType(OID oid)
{
  ObjectLocator objLoc;

  // Look in the object cache:
  ObjectHeader * pObj = objH_Lookup(oid, 0);
  if (pObj) {
    objLoc.locType = objLoc_ObjectHeader;
    objLoc.u.objH = pObj;	// Beware, pObj may have OFLG_Fetching
    objLoc.objType = objH_GetBaseType(pObj);
    return objLoc;
  } else {
    // Look in the log directory.
    const ObjectDescriptor * objDescP = ld_findObject(oid);
    if (objDescP) {
      objLoc.locType = objLoc_ObjectDescriptor;
      objLoc.objType = objDescP->type;
      objLoc.u.objDesc.allocCount = objDescP->allocCount;
      objLoc.u.objDesc.callCount = objDescP->callCount;
      objLoc.u.objDesc.logLoc = objDescP->logLoc;
      return objLoc;
    }

    // Look in the object source:
    ObjectRange * rng = LookupOID(oid, obRanges, nObRanges);
    if (!rng) {
      dprintf(true, "No range for OID %#llx!\n", oid);
      act_SleepOn(&SourceWait);
      act_Yield();
    }
    return rng->source->objS_GetObjectType(rng, oid);
  }
}

// May Yield.
ObCount
GetObjectCount(OID oid, ObjectLocator * pObjLoc, bool callCount)
{
  switch (pObjLoc->locType) {
  default:
    fatal("invalid locType");

  case objLoc_ObjectHeader:
    objH_EnsureNotFetching(pObjLoc->u.objH);
    if (callCount && objH_isNodeType(pObjLoc->u.objH))
      return node_GetCallCount(objH_ToNode(pObjLoc->u.objH));
    else return objH_GetAllocCount(pObjLoc->u.objH);

  case objLoc_ObjectDescriptor:
    if (callCount)
      return pObjLoc->u.objDesc.callCount;
    else return pObjLoc->u.objDesc.allocCount;

  case objLoc_TagPot: ;
    if (pObjLoc->objType == capros_Range_otPage) {
      // A page. The count is in the tag pot.
      TagPot * tp = (TagPot *)pageH_GetPageVAddr(pObjLoc->u.tagPot.tagPotPageH);
      return tp->count[pObjLoc->u.tagPot.potEntry];
    } else {	// a type in a pot
      ObjectRange * rng = pObjLoc->u.tagPot.range;
      return rng->source->objS_GetObjectCount(rng, oid, pObjLoc, callCount);
    }

  case objLoc_Preload: ;
    ObjectRange * rng = pObjLoc->u.preload.range;
    return rng->source->objS_GetObjectCount(rng, oid, pObjLoc, callCount);
  }
}

// May Yield.
ObjectHeader *
GetObject(OID oid, const ObjectLocator * pObjLoc)
{
  switch (pObjLoc->locType) {
  default:
    fatal("invalid locType");

  case objLoc_ObjectHeader:
    objH_EnsureNotFetching(pObjLoc->u.objH);
    return pObjLoc->u.objH;

  case objLoc_ObjectDescriptor:
  {
    LID lid = pObjLoc->u.objDesc.logLoc;
    if (!lid) {		// a null object
      ObjectHeader * pObj = CreateNewNullObject(pObjLoc->objType, oid,
                              pObjLoc->u.objDesc.allocCount);
      if (pObjLoc->objType == capros_Range_otNode)
        objH_ToNode(pObj)->callCount = pObjLoc->u.objDesc.callCount;
      return pObj;
    } else {		// fetch the object from the log
      assert(!"complete");
    }
  }

  case objLoc_TagPot: ;
  {
    ObjectRange * rng = pObjLoc->u.tagPot.range;
    return rng->source->objS_GetObject(rng, oid, pObjLoc);
  }

  case objLoc_Preload:
  {
    ObjectRange * rng = pObjLoc->u.preload.range;
    return rng->source->objS_GetObject(rng, oid, pObjLoc);
  }
  }
}

bool
objC_WriteBack(ObjectHeader * pObj, bool inBackground)
{
  ObjectRange * rng = LookupOID(pObj->oid, obRanges, nObRanges);
  if (!rng) {
    dprintf(true, "No range for OID %#llx!\n", pObj->oid);
    return false;
  }

////  return rng->source->objS_WriteBack(rng, pObj, inBackground);
  return false;	// for now
}

void
objC_FindFirstSubrange(OID limStart, OID limEnd, 
  OID* subStart /*@ not null @*/, OID* subEnd /*@ not null @*/)
{
  int i = LookupRange(limStart, obRanges, nObRanges);
  if (i < 0 || obRanges[i].end < limStart) {
    i++;	// limStart is not in a range, so consider the next range
    if (i >= nObRanges) {
      // No next range.
      *subStart = *subEnd = ~0llu;
      return;
    }
  }

  ObjectRange * rng = &obRanges[i];
  // limStart <= rng->end;

  OID mySubStart = max(limStart, rng->start);
  OID mySubEnd = min(limEnd, rng->end);

  if (mySubEnd < mySubStart)
    mySubEnd = mySubStart;	// no range here

  *subStart = mySubStart;
  *subEnd = mySubEnd;

  DEBUG(findfirst)
    printf("ObCache::FindFirstSubrange(): limStart %#llx, "
	   "limEnd %#llx  nObRanges %d subStart %#llx subEnd %#llx\n",
           limStart, limEnd, nObRanges, mySubStart, mySubEnd);
}

#ifdef OPTION_DDB
void
objC_ddb_DumpSources()
{
  extern void db_printf(const char *fmt, ...);
  unsigned i = 0;

  for (i = 0; i < nObRanges; i++) {
    ObjectRange * src = &obRanges[i];
    printf("[%#llx,%#llx): %s\n", src->start, src->end, src->source->name);
  }

  if (nObRanges == 0)
    printf("No object sources.\n");
}
#endif

void
objC_InitObjectSources(void)
{
  PreloadObSource_Init();

  PhysPageObSource_Init();
}
