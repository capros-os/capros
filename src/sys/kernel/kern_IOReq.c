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
    iorq->needsSyncCache = false;
    iorq->lk.next = freeIORQs;
    freeIORQs = &iorq->lk;
    sq_Init(& iorq->waiter);
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
    link_Init(&ioreq->lk);
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
    pageH->ioreq = ioreq;
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
    link_Init(&iorq->lk);
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

void
IOReq_EndRead(IORequest * ioreq)
{
  // The IORequest is done.
  // Mark the page as no longer having I/O.
  PageHeader * pageH = ioreq->pageH;
  pageH->ioreq = NULL;
  pageH_SetReferenced(pageH);
  objH_ClearFlags(pageH_ToObj(pageH), OFLG_Fetching);
  sq_WakeAll(&ioreq->sq, false);
  // Caller has unlinked the ioreq and will deallocate it.
}

void
IOReq_WakeSQ(IORequest * ioreq)
{
  // The IORequest is done.
  // Mark the page as no longer having I/O.
  PageHeader * pageH = ioreq->pageH;
  pageH->ioreq = NULL;
  sq_WakeAll(&ioreq->sq, false);
  // Caller has unlinked the ioreq and will deallocate it.
}

// May Yield.
void
objH_EnsureNotFetching(ObjectHeader * pObj)
{
  if (objH_GetFlags(pObj, OFLG_Fetching)) {
    // Must be a page or pot:
    assert(pObj->obType > ot_NtLAST_NODE_TYPE);
    PageHeader * pageH = objH_ToPage(pObj);
    // Wait for the fetching to finish.
    act_SleepOn(&pageH->ioreq->sq);
    act_Yield();
  }
  objH_TransLock(pObj);
}

// May Yield.
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
  ObjectHeader * pObj = objH_Lookup(tagPotID, ot_PtTagPot);
  if (pObj) {
    objH_EnsureNotFetching(pObj);
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
  IORequest * ioreq = AllocateIOReqAndPage();

  /* Initialize the pot.
  We add it to the object hash now, with the flag OFLG_Fetching,
  so that if there is another request for the pot while it is being fetched,
  it won't be fetched twice.
  (The same is true of object pots and pages.) */
  PageHeader * pageH = ioreq->pageH;
  pObj = pageH_ToObj(pageH);
  pObj->obType = ot_PtTagPot;
  objH_InitObj(pObj, tagPotID);
  objH_SetFlags(pObj, OFLG_Fetching);

  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = ClusterToTagPotRangeLoc(clusterNum);
  ioreq->doneFn = &IOReq_EndRead;
  sq_Init(&ioreq->sq);
  act_SleepOn(&ioreq->sq);
  act_Yield();
}

void
ioreq_Enqueue(IORequest * ioreq)
{
  IORQ * iorq = ioreq->objRange->u.rq.iorq;
  link_insertAfter(& iorq->lk, & ioreq->lk);
  sq_WakeAll(& iorq->waiter, false);
}

// May Yield.
static PageHeader *
EnsureObjectPot(ObjectRange * rng, OID oid)
{
  frame_t frame = OIDToFrame(oid);
  OID relOid = oid - rng->start;	// OID relative to this range
  frame_t relFrame = OIDToFrame(relOid);

  // Is the pot in memory?
  ObjectHeader * pObj = objH_Lookup(frame, ot_PtObjPot);
  if (pObj) {
    objH_EnsureNotFetching(pObj);
    pObj->objAge = age_NewObjPot;	// mark referenced, but not strongly
    return objH_ToPage(pObj);
  }

  // Read in the pot.
  IORequest * ioreq = AllocateIOReqAndPage();

  // Initialize the pot.
  PageHeader * pageH = ioreq->pageH;
  pObj = pageH_ToObj(pageH);
  pObj->obType = ot_PtObjPot;
  objH_InitObj(pObj, oid);
  objH_SetFlags(pObj, OFLG_Fetching);

  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = FrameToRangeLoc(relFrame);
  ioreq->doneFn = &IOReq_EndRead;
  sq_Init(&ioreq->sq);
  act_SleepOn(&ioreq->sq);
  act_Yield();
  // act_Yield does not return
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
    ioreq->doneFn = &IOReq_EndRead;
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
    pObj->obType = ot_NtUnprepared;
    objH_InitObj(pObj, oid);
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

