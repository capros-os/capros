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

static ObjectRange ranges[KTUNE_NRNGTBLENTS];
static uint32_t nRanges = 0;

/* On exit, returns u such that ranges[u].start <= oid < ranges[u+1].start .
 * Returns -1 if oid < ranges[0].start . */
static int
LookupRange(OID oid)
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

static ObjectRange *
LookupOID(OID oid)
{
  int i = LookupRange(oid);
  if (i < 0)
    return NULL;	// oid is before the first range

  ObjectRange * rng = &ranges[i];
  if (oid < rng->end)
    return rng;
  else return NULL;
}

bool
objC_AddRange(ObjectRange * rng)
{
  unsigned int i, j;

  if (nRanges == KTUNE_NRNGTBLENTS)
    fatal("Limit on total object ranges exceeded\n");
  
  DEBUG(obsrc)
    printf("New range: \"%s\" [%#llx,%#llx)\n",
		   rng->source->name, rng->start, rng->end);

  i = LookupRange(rng->start) + 1;	// may be == nRanges

  /* Because we do not yet implement mirroring, we check here
   * that the new range does not overlap any existing range. */

  if (i > 0) {
    if (ranges[i-1].end > rng->start)
      dprintf(true,
              "New range [%#llx,%#llx) overlaps existing range [%#llx,%#llx)\n",
              rng->start, rng->end, ranges[i-1].start, ranges[i-1].end);
  }
  if (i < nRanges)
    if (ranges[i].start < rng->end)
      dprintf(true,
              "New range [%#llx,%#llx) overlaps existing range [%#llx,%#llx)\n",
              rng->start, rng->end, ranges[i].start, ranges[i].end);
  
  DEBUG(obsrc)
    printf("AddRange: found %d nRanges=%d\n", i, nRanges);

  // Move higher ranges up.
  for (j = nRanges++; j > i; j--)
      ranges[j] = ranges[j-1];
 
  ranges[i] = *rng;

  sq_WakeAll(&SourceWait, false);
  
  DEBUG(obsrc)
    printf("AddRange: returning\n");

  return true;
}

bool
objC_HaveSource(OID oid)
{
  return (bool) LookupOID(oid);
}

// May Yield.
ObjectLocator
GetObjectType(OID oid)
{
  ObjectLocator objLoc;

  // Look in the object cache:
  ObjectHeader * pObj = objH_Lookup(oid);
  if (pObj) {
    objLoc.locType = objLoc_ObjectHeader;
    objLoc.u.objH = pObj;
    objLoc.objType = objH_GetBaseType(pObj);
    return objLoc;
  } else {
    // Look in the log directory ...

    // Look in the object source:
    ObjectRange * rng = LookupOID(oid);
    if (!rng) {
      dprintf(true, "No range for OID %#llx!\n", oid);
      act_SleepOn(&SourceWait);
      act_Yield();
    }
    return rng->source->objS_GetObjectType(rng, oid);
  }
}

ObCount
GetObjectCount(OID oid, ObjectLocator * pObjLoc, bool callCount)
{
  switch (pObjLoc->locType) {
  default:
    fatal("invalid locType");

  case objLoc_ObjectHeader:
    if (callCount && objH_isNodeType(pObjLoc->u.objH))
      return node_GetCallCount(objH_ToNode(pObjLoc->u.objH));
    else return objH_GetAllocCount(pObjLoc->u.objH);

  // If in the log directory, get count from there ...

  case objLoc_TagPot: ;
    if (pObjLoc->objType == capros_Range_otPage) {
      // A page. Counts are in the tag pot.
      //// get counts from tag pot
      ObjectRange * rng = pObjLoc->u.tagPot.range;
      return rng->source->objS_GetObjectCount(rng, oid, pObjLoc, callCount);////
    } else {	// a type in a pot
      ObjectRange * rng = pObjLoc->u.tagPot.range;
      return rng->source->objS_GetObjectCount(rng, oid, pObjLoc, callCount);
    }

  case objLoc_Preload: ;
    ObjectRange * rng = pObjLoc->u.preload.range;
    return rng->source->objS_GetObjectCount(rng, oid, pObjLoc, callCount);
  }
}

ObjectHeader *
GetObject(OID oid, const ObjectLocator * pObjLoc)
{
  switch (pObjLoc->locType) {
  default:
    fatal("invalid locType");

  case objLoc_ObjectHeader:
    return pObjLoc->u.objH;

  // If in the log directory, fetch the object ...

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
  ObjectRange * rng = LookupOID(pObj->oid);
  if (!rng) {
    dprintf(true, "No range for OID %#llx!\n", pObj->oid);
    return false;
  }

  return rng->source->objS_WriteBack(rng, pObj, inBackground);
}

void
objC_FindFirstSubrange(OID limStart, OID limEnd, 
  OID* subStart /*@ not null @*/, OID* subEnd /*@ not null @*/)
{
  int i = LookupRange(limStart);
  if (i < 0 || ranges[i].end < limStart) {
    i++;	// limStart is not in a range, so consider the next range
    if (i >= nRanges) {
      // No next range.
      *subStart = *subEnd = ~0llu;
      return;
    }
  }

  ObjectRange * rng = &ranges[i];
  // limStart <= rng->end;

  OID mySubStart = max(limStart, rng->start);
  OID mySubEnd = min(limEnd, rng->end);

  if (mySubEnd < mySubStart)
    mySubEnd = mySubStart;	// no range here

  *subStart = mySubStart;
  *subEnd = mySubEnd;

  DEBUG(findfirst)
    printf("ObCache::FindFirstSubrange(): limStart %#llx, "
	   "limEnd %#llx  nRanges %d subStart %#llx subEnd %#llx\n",
           limStart, limEnd, nRanges, mySubStart, mySubEnd);
}

#ifdef OPTION_DDB
void
objC_ddb_DumpSources()
{
  extern void db_printf(const char *fmt, ...);
  unsigned i = 0;

  for (i = 0; i < nRanges; i++) {
    ObjectRange * src = &ranges[i];
    printf("[%#llx,%#llx): %s\n", src->start, src->end, src->source->name);
  }

  if (nRanges == 0)
    printf("No object sources.\n");
}
#endif

void
objC_InitObjectSources(void)
{
  PreloadObSource_Init();

  PhysPageObSource_Init();
}
