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
#include <kerninc/Activity.h>
#include <eros/target.h>
#include <disk/TagPot.h>
#include <disk/DiskNode.h>
#include <eros/machine/IORQ.h>
#include <idl/capros/Range.h>

#define dbg_		0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

IORequest IOReqs[KTUNE_NIOREQS];
Link * freeIOReqs = NULL;

IORQ IORQs[KTUNE_NIORQS];
Link * freeIORQs = NULL;

// ************************ IORequest stuff **************************

void
IORQ_Init(void)
{
  int i;
  // All IORQs are free:
  for (i = 0; i < KTUNE_NIORQS; i++) {
    IORQ * iorq = &IORQs[i];
    iorq->lk.next = freeIORQs;
    freeIORQs = &iorq->lk;
  }

  // All IOReqs are free:
  for (i = 0; i < KTUNE_NIOREQS; i++) {
    IORequest * ioreq = &IOReqs[i];
    ioreq->lk.next = freeIOReqs;
    freeIOReqs = &ioreq->lk;
  }
}

IORequest *
IOReq_Allocate(void)
{
  IORequest * ioreq = container_of(freeIOReqs, IORequest, lk);
  if (ioreq) {
    freeIOReqs = ioreq->lk.next;
  }
  return ioreq;
  
}

static DEFQUEUE(IOReqWait);

// Yields if can't allocate.
IORequest *
IOReq_AllocateOrWait(void)
{
  IORequest * ioreq = IOReq_Allocate();
  if (ioreq)
    return ioreq;

  dprintf(true, "No IOReq\n");
  act_SleepOn(&IOReqWait);
  act_Yield();
}

// Yields if can't allocate.
IORequest *
AllocateIOReqAndPage(void)
{
  PageHeader * pageH = objC_GrabPageFrame();
  IORequest * ioreq = IOReq_Allocate();
  if (ioreq) {
    ioreq->pageH = pageH;
    return ioreq;
  }

  ReleasePageFrame(pageH);

  dprintf(true, "No IOReq\n");
  act_SleepOn(&IOReqWait);
  act_Yield();
}

void
IOReq_Deallocate(IORequest * ioreq)
{
  ioreq->lk.next = freeIOReqs;
  freeIOReqs = &ioreq->lk;

  sq_WakeAll(&IOReqWait, false);
}

// ************************ IORQ stuff **************************

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

static void
IOReq_WakeSQ(struct IORequest * ioreq)
{
  // The IORequest is done.
  //// Mark the page as no longer having I/O.
  sq_WakeAll(&ioreq->sq, false);
}

ObjectLocator 
IOSource_GetObjectType(ObjectRange * rng, OID oid)
{
  ObjectLocator objLoc;

  OID relOid = oid - rng->start;	// OID relative to this range
  frame_t relFrame = OIDToFrame(relOid);
  // Object type is in the tag pot.
  frame_t clusterNum = FrameToCluster(relFrame);
  frame_t tagPotRelID = ClusterToTagPotRelID(clusterNum);
  frame_t tagPotID = tagPotRelID + rng->start;

  // Is the tag pot in memory?
  ObjectHeader * pObj = objH_Lookup(tagPotID, true);
  if (pObj) {
    objLoc.locType = objLoc_TagPot;
    objLoc.u.tagPot.tagPotPageH = objH_ToPage(pObj);
    objLoc.u.tagPot.range = rng;
    TagPot * tp = (TagPot *)pageH_GetPageVAddr(objH_ToPage(pObj));
    unsigned int potEntry = FrameIndexInCluster(relFrame);
    objLoc.u.tagPot.potEntry = potEntry;
    objLoc.objType = tp->tags[potEntry] & TagTypeMask;
    return objLoc;
  }
  // Read in the tag pot.
  // FIXME need to avoid reading the same pot multiple times!
  IORequest * ioreq = AllocateIOReqAndPage();
  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = ClusterToTagPotRangeLoc(clusterNum);
  ioreq->doneFn = &IOReq_WakeSQ;
  sq_Init(&ioreq->sq);
  act_SleepOn(&ioreq->sq);
  act_Yield();
}

// May Yield.
static PageHeader *
EnsureObjectPot(ObjectRange * rng, OID oid)
{
  frame_t frame = OIDToFrame(oid);
  OID relOid = oid - rng->start;	// OID relative to this range
  frame_t relFrame = OIDToFrame(relOid);

  // Is the pot in memory?
  ObjectHeader * pObj = objH_Lookup(frame, true);////
  if (! pObj) {
    // Read in the pot.
    // FIXME need to avoid reading the same pot multiple times!
    IORequest * ioreq = AllocateIOReqAndPage();
    ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
    ioreq->objRange = rng;
    ioreq->rangeLoc = FrameToRangeLoc(relFrame);
    ioreq->doneFn = &IOReq_WakeSQ;
    sq_Init(&ioreq->sq);
    act_SleepOn(&ioreq->sq);
    act_Yield();
    // act_Yield does not return
  }
  return objH_ToPage(pObj);
}

// May Yield.
static ObCount
IOSource_GetObjectCount(ObjectRange * rng, OID oid,
  ObjectLocator * pObjLoc, bool callCount)
{
  assert(rng->start <= oid && oid < rng->end);

  PageHeader * pageH = EnsureObjectPot(rng, oid);

  OID relOid = oid - rng->start;	// OID relative to this range

  // A node pot is just an array of DiskNode's.
  DiskNode * dn = (DiskNode *)pageH_GetPageVAddr(pageH);
  dn += OIDToObIndex(relOid);
  return callCount ? dn->callCount : dn->allocCount;
}

// May Yield.
ObjectHeader * 
IOSource_GetObject(ObjectRange * rng, OID oid,
  const ObjectLocator * pObjLoc)
{
  PageHeader * pageH;

  assert(rng->start <= oid && oid < rng->end);

  OID relOid = oid - rng->start;	// OID relative to this range

  switch (pObjLoc->objType) {
  default: ;
    assert(false);	// invalid type

  case capros_Range_otPage: ;
    // Read in the page.
    // FIXME need to avoid reading the same page multiple times!
    IORequest * ioreq = AllocateIOReqAndPage();
    ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
    ioreq->objRange = rng;
    ioreq->rangeLoc = FrameToRangeLoc(OIDToFrame(relOid));
    ioreq->doneFn = &IOReq_WakeSQ;
    sq_Init(&ioreq->sq);
    act_SleepOn(&ioreq->sq);
    act_Yield();
    // act_Yield does not return
    break;

  case capros_Range_otNode:
    pageH = EnsureObjectPot(rng, oid);

    Node * pNode = objC_GrabNodeFrame();

    // A node pot is just an array of DiskNode's.
    DiskNode * dn = (DiskNode *)pageH_GetPageVAddr(pageH);
    dn += OIDToObIndex(relOid);
    node_SetEqualTo(pNode, dn);
    ObjectHeader * pObj = node_ToObj(pNode);
    objH_InitObj(pObj, oid, capros_Range_otNode);
    objH_ClearFlags(pObj, OFLG_DIRTY);
    objH_SetFlags(pObj, OFLG_Cleanable);
    return pObj;
  }
}

static void
IOSource_WriteRangeLoc(ObjectRange * rng, frame_t rangeLoc,
  PageHeader * pageH)
{
  IORequest * ioreq = IOReq_AllocateOrWait();
  ioreq->pageH = pageH;
  ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = rangeLoc;
  ioreq->doneFn = &IOReq_WakeSQ;////
  sq_Init(&ioreq->sq);
  act_SleepOn(&ioreq->sq);
  act_Yield();
  // act_Yield does not return
}

const struct ObjectSource IOObSource = {
  .name = "I/O",
  .objS_GetObjectType = &IOSource_GetObjectType,
  .objS_GetObjectCount = &IOSource_GetObjectCount,
  .objS_GetObject = &IOSource_GetObject,
  .objS_WriteRangeLoc = &IOSource_WriteRangeLoc,
};

