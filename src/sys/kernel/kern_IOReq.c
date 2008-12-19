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
#include <kerninc/LogDirectory.h>
#include <kerninc/Ckpt.h>
#include <kerninc/ObjH-inline.h>
#include <arch-kerninc/Page-inline.h>
#include <eros/target.h>
#include <disk/TagPot.h>
#include <disk/DiskNode.h>
#include <eros/machine/IORQ.h>
#include <idl/capros/Range.h>

#define dbg_		0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* To avoid deadlock, IORequests for cleaning are segregated from others. */

IORequest IOReqs[KTUNE_NIOREQS];
Link * freeIOReqs = NULL;

IORequest IOReqsCleaning[KTUNE_NIOREQS_CLEANING];
Link * freeIOReqsCleaning = NULL;

IORQ IORQs[KTUNE_NIORQS];
Link * freeIORQs = NULL;

void
SleepOnPFHQueue(StallQueue * sq)
{
  /* The service of this queue depends on the user-mode Page Fault Handler.
  If a process that is part of the Page Fault Handler sleeps here,
  there is a possible deadlock. */
  assert(! proc_IsPFH(proc_Current()));

  act_SleepOn(sq);
  act_Yield();
}

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
    ioreq->cleaning = false;	// constant hereafter
    ioreq->pageH = NULL;
    ioreq->lk.next = freeIOReqs;
    freeIOReqs = &ioreq->lk;
  }

  for (i = 0; i < KTUNE_NIOREQS_CLEANING; i++) {
    IORequest * ioreq = &IOReqsCleaning[i];
    ioreq->cleaning = true;	// constant hereafter
    ioreq->pageH = NULL;
    ioreq->lk.next = freeIOReqsCleaning;
    freeIOReqsCleaning = &ioreq->lk;
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

DEFQUEUE(IOReqWait);

// Yields if can't allocate.
IORequest *
IOReq_AllocateOrWait(void)
{
  IORequest * ioreq = IOReq_Allocate();
  if (ioreq)
    return ioreq;

  dprintf(true, "No IOReq\n");
  SleepOnPFHQueue(&IOReqWait);
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
  SleepOnPFHQueue(&IOReqWait);
}

// ******************** IORequest for cleaning stuff *********************

IORequest *
IOReqCleaning_Allocate(void)
{
  IORequest * ioreq = container_of(freeIOReqsCleaning, IORequest, lk);
  if (ioreq) {
    freeIOReqsCleaning = ioreq->lk.next;
    link_Init(&ioreq->lk);
  }
  return ioreq;
}

DEFQUEUE(IOReqCleaningWait);

// Yields if can't allocate.
IORequest *
IOReqCleaning_AllocateOrWait(void)
{
  IORequest * ioreq = IOReqCleaning_Allocate();
  if (ioreq)
    return ioreq;

  SleepOnIOReqCleaning();
}

// Yields if can't allocate.
IORequest *
AllocateIOReqCleaningAndPage(void)
{
  PageHeader * pageH = objC_GrabPageFrame();
  IORequest * ioreq = IOReqCleaning_Allocate();
  if (ioreq) {
    ioreq->pageH = pageH;
    pageH->ioreq = ioreq;
    return ioreq;
  }

  ReleasePageFrame(pageH);
  SleepOnIOReqCleaning();
}

void
IOReq_Deallocate(IORequest * ioreq)
{
  ioreq->pageH = NULL;	// for safety and to mark free
  // Return it to the proper pool:
  if (ioreq->cleaning) {
    ioreq->lk.next = freeIOReqsCleaning;
    freeIOReqsCleaning = &ioreq->lk;
    sq_WakeAll(&IOReqCleaningWait);
  } else {
    ioreq->lk.next = freeIOReqs;
    freeIOReqs = &ioreq->lk;
    sq_WakeAll(&IOReqWait);
  }
}

// ************************ IORQ stuff **************************

IORQ *
IORQ_Allocate(void)
{
  IORQ * iorq = container_of(freeIORQs, IORQ, lk);
  if (iorq) {
    freeIORQs = iorq->lk.next;
    link_Init(&iorq->lk);
#ifndef NDEBUG
    iorq->creatorOID = node_ToObj(act_CurContext()->procRoot)->oid;
#endif
  }
  return iorq;
  
}

void
IORQ_Deallocate(IORQ * iorq)
{
  iorq->lk.next = freeIORQs;
  freeIORQs = &iorq->lk;
}

// ************************ Log stuff **************************

// Add one frame to lid, wrapping if necessary.
LID
IncrementLID(LID lid)
{
  lid += FrameToOID(1);
  if (lid >= logWrapPoint) 
    return MAIN_LOG_START;	// wrap to the beginning
  return lid;
}

LID
NextLogLoc(void)
{
  if (! restartIsDone()) {
    // This can happen if persistent objects are preloaded rather than
    // loaded from the last checkpoint.
    printf("NextLogLoc waiting for restart to complete.\n");
    SleepOnPFHQueue(&RestartQueue);
  }

  LID lid = logCursor;
  logCursor = IncrementLID(logCursor);
  return lid;
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
  sq_WakeAll(&ioreq->sq);
  IOReq_Deallocate(ioreq);
}

void
IOReq_EndReadPage(IORequest * ioreq)
{
#ifdef OPTION_OB_MOD_CHECK
  pageH_SetCheck(ioreq->pageH);
#endif

  IOReq_EndRead(ioreq);
}

void
IOReq_EndWrite(IORequest * ioreq)
{
  // The IORequest is done.
  PageHeader * pageH = ioreq->pageH;
  // Mark the page as no longer having I/O.
  pageH->ioreq = NULL;
  pageH_ToObj(pageH)->objAge = age_Steal;
  sq_WakeAll(&ioreq->sq);
  IOReq_Deallocate(ioreq);
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
    SleepOnPFHQueue(&pageH->ioreq->sq);
  }
  objH_TransLock(pObj);
}

// Yields.
void
objRange_FetchPage(ObjectRange * rng, OID oid, frame_t rangeLoc)
{
  assert(OIDIsPersistent(oid));

  PageHeader * pageH;
  ObjectHeader * pObj;
  // Read in the page.
  IORequest * ioreq = AllocateIOReqAndPage();

  // Initialize the page.
  pageH = ioreq->pageH;
#if 0
  printf("objRange_FetchPage pageH %#x oid %#llx\n", pageH, oid);
#endif
  pObj = pageH_ToObj(pageH);
  pObj->obType = ot_PtDataPage;
  objH_InitObj(pObj, oid);
  pageH_MDInitDataPage(pageH);
  objH_SetFlags(pObj, OFLG_Fetching | OFLG_Cleanable);

  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = rangeLoc;
  ioreq->doneFn = &IOReq_EndReadPage;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
  SleepOnPFHQueue(&ioreq->sq);
  // act_Yield does not return
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
  ioreq->doneFn = &IOReq_EndReadPage;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
  SleepOnPFHQueue(&ioreq->sq);
}

void
ioreq_Enqueue(IORequest * ioreq)
{
  switch (ioreq->requestCode) {
  default: ;
    assert(false);

  case capros_IOReqQ_RequestType_readRangeLoc:
    pageH_PrepareForDMAInput(ioreq->pageH);
    break;

  case capros_IOReqQ_RequestType_writeRangeLoc:
    pageH_PrepareForDMAOutput(ioreq->pageH);
    break;

  case capros_IOReqQ_RequestType_synchronizeCache:
    break;
  }

  IORQ * iorq = ioreq->objRange->u.rq.iorq;
#if 0
  dprintf(true, "ioreq_Enqueue %#x rl=%lld\n", ioreq, ioreq->rangeLoc);
#endif
  link_insertAfter(& iorq->lk, & ioreq->lk);
  sq_WakeAll(& iorq->waiter);
}

void
CleanLogPot(PageHeader * pageH, IORequest * ioreq)
{
  ioreq->pageH = pageH;
  pageH->ioreq = ioreq;
  LID lid = pageH_ToObj(pageH)->oid;
  ObjectRange * rng = LidToRange(lid);
  assert(rng);	// it had better be mounted
  ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(lid - rng->start);
  ioreq->doneFn = &IOReq_EndWrite;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
}

void
objRange_FetchPot(ObjectRange * rng, OID oidOrLid,
  frame_t rangeLoc, ObType obType)
{
  assert(OIDToObIndex(oidOrLid) == 0);

  IORequest * ioreq = AllocateIOReqAndPage();

  // Initialize the pot.
  PageHeader * pageH = ioreq->pageH;
  ObjectHeader * pObj = pageH_ToObj(pageH);
  pObj->obType = obType;
  objH_InitObj(pObj, oidOrLid);
  objH_SetFlags(pObj, OFLG_Fetching);

  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = rangeLoc;
  ioreq->doneFn = &IOReq_EndReadPage;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
  SleepOnPFHQueue(&ioreq->sq);
}

// Find or get an object pot from the home range.
// May Yield.
static PageHeader *
EnsureHomePot(ObjectRange * rng, OID oid)
{
  frame_t frame = OIDToFrame(oid);
  OID relOid = oid - rng->start;	// OID relative to this range
  frame_t relFrame = OIDToFrame(relOid);

  // Is the pot in memory?
  ObjectHeader * pObj = objH_Lookup(FrameToOID(frame), ot_PtHomePot);
  if (pObj) {
    objH_EnsureNotFetching(pObj);
    pObj->objAge = age_NewObjPot;	// mark referenced, but not strongly
    return objH_ToPage(pObj);
  }

  // Read in the pot.
  objRange_FetchPot(rng, oid, FrameToRangeLoc(relFrame), ot_PtHomePot);
}

// May Yield.
static ObCount
IOSource_GetObjectCount(ObjectRange * rng, OID oid,
  ObjectLocator * pObjLoc, bool callCount)
{
  assert(rng->start <= oid && oid < rng->end);

  PageHeader * pageH = EnsureHomePot(rng, oid);

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
  assert(rng->start <= oid && oid < rng->end);

  OID relOid = oid - rng->start;	// OID relative to this range

  switch (pObjLoc->objType) {
  default: ;
    assert(false);	// invalid type

  case capros_Range_otPage: ;
    TagPot * tp = (TagPot *)pageH_GetPageVAddr(pObjLoc->u.tagPot.tagPotPageH);
    unsigned int potEntry = pObjLoc->u.tagPot.potEntry;
    if (tp->tags[potEntry] & TagIsZero) {
      return CreateNewNullObject(capros_Range_otPage, oid, tp->count[potEntry]);
    }
    objRange_FetchPage(rng, oid, FrameToRangeLoc(OIDToFrame(relOid)));
    break;

  case capros_Range_otNode: ;
    PageHeader * pageH = EnsureHomePot(rng, oid);
    return node_ToObj(pageH_GetNodeFromPot(pageH, OIDToObIndex(relOid)));
  }
}

const struct ObjectSource IOObSource = {
  .name = "I/O",
  .objS_GetObjectType = &IOSource_GetObjectType,
  .objS_GetObjectCount = &IOSource_GetObjectCount,
  .objS_GetObject = &IOSource_GetObject
};

#ifdef OPTION_DDB
static void
db_show_ioreq(IORequest * ioreq)
{
  printf("%#x: cln=%d pageH=%#x objRng=%#x reqCode=%d\n",
         ioreq, ioreq->cleaning, ioreq->pageH,
         ioreq->objRange, ioreq->requestCode);
}

void
db_show_ioreqs(void)
{
  int i;
  for (i = 0; i < KTUNE_NIOREQS; i++) {
    IORequest * ioreq = &IOReqs[i];
    if (ioreq->pageH)
      db_show_ioreq(ioreq);
  }

  for (i = 0; i < KTUNE_NIOREQS_CLEANING; i++) {
    IORequest * ioreq = &IOReqsCleaning[i];
    if (ioreq->pageH)
      db_show_ioreq(ioreq);
  }
}
#endif
