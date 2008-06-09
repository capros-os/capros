/*
 * Copyright (C) 2008, Strawberry Development Group
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
#include <kerninc/util.h>
#include <arch-kerninc/KernTune.h>
#include <kerninc/IORQ.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/ObjectHeader.h>
#include <eros/target.h>
#include <disk/TagPot.h>

#define dbg_		0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

IORQ IORQs[KTUNE_NIORQS];
Link * freeIORQs = NULL;

// ************************ IORQ stuff **************************

void
IORQ_Init(void)
{
  // All IORQs are free:
  int i;
  for (i = 0; i < KTUNE_NIORQS; i++) {
    IORQ * iorq = &IORQs[i];
    iorq->lk.next = freeIORQs;
    freeIORQs = &iorq->lk;
  }
}

IORQ *
IORQ_Allocate(void)
{
  IORQ * iorq = container_of(freeIORQs, IORQ, lk);
  if (iorq) {
    freeIORQs = iorq->lk.next;
  }
  return iorq;
  
}

void
IORQ_Deallocate(IORQ * iorq)
{
  iorq->lk.next = freeIORQs;
  freeIORQs = &iorq->lk;
}

// ************************ IOSource stuff **************************

ObjectLocator 
IOSource_GetObjectType(ObjectRange * rng, OID oid)
{
  ObjectLocator objLoc;

  OID relOid = oid - rng->start;	// OID relative to this range
  frame_t relFrame = OIDToFrame(relOid);
  // Object type is in the tag pot.
  frame_t tagPotRelID = FrameToTagPotRelID(relFrame);
  frame_t tagPotID = tagPotRelID + rng->start;
  // Is the tag pot in memory?
  ObjectHeader * pObj = objH_Lookup(tagPotID, true);
  if (pObj) {
    TagPot * tp = (TagPot *)pageH_GetPageVAddr(objH_ToPage(pObj));
    unsigned int potEntry = FrameIndexInCluster(relFrame);
    objLoc.objType = tp->typeAndAllocCountUsed[potEntry] & TagTypeMask;
////
    return objLoc;
  }
////
#if 0
  ObjectLocator objLoc = {
    .locType = objLoc_Preload,
    .objType = 0,////capros_Range_otPage,	// pages only
    .u = {
      .preload = {
         .range = rng
      }
    }
  };
#endif
  fatal("IOSource::WriteBack() unimplemented\n");
  objLoc.objType = 0;
  return objLoc;
}

static ObCount
IOSource_GetObjectCount(ObjectRange * rng, OID oid,
  ObjectLocator * pObjLoc, bool callCount)
{
  assert(rng->start <= oid && oid < rng->end);
  fatal("IOSource::WriteBack() unimplemented\n");

  return 0;	// FIXME
}

/* May Yield. */
ObjectHeader * 
IOSource_GetObject(ObjectRange * rng, OID oid,
  const ObjectLocator * pObjLoc)
{
  assert(rng->start <= oid && oid < rng->end);
  fatal("IOSource::WriteBack() unimplemented\n");

  return NULL;////
}

static bool
IOSource_WriteBack(ObjectRange * rng, ObjectHeader *obHdr, bool b)
{
  fatal("IOSource::WriteBack() unimplemented\n");

  return false;
}

const struct ObjectSource IOObSource = {
  .name = "I/O",
  .objS_GetObjectType = &IOSource_GetObjectType,
  .objS_GetObjectCount = &IOSource_GetObjectCount,
  .objS_GetObject = &IOSource_GetObject,
  .objS_WriteBack = &IOSource_WriteBack,
};

