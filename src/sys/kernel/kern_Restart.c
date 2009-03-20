/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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
#include <kerninc/kernel.h>
#include <disk/DiskNode.h>
#include <disk/GenerationHdr.h>
#include <disk/DiskObjDescr.h>
#include <disk/CkptRoot.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Activity.h>
#include <kerninc/IORQ.h>
#include <kerninc/Ckpt.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/Key-inline.h>
#include <kerninc/mach-rtc.h>
#include <eros/machine/IORQ.h>

extern Activity * migratorActivity;

#define dbg_restart	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_restart )

#define DEBUG(x) if (dbg_##x & dbg_flags)

uint64_t monotonicTimeOfRestart;

// RTC of the time of the checkpoint from which we restarted:
unsigned long RTCOfRestartDemarc = 0;
// RTC of (re)start:
unsigned long RTCOfRestart;
bool IsPreloadedBigBang = false;
// If IsPreloadedBigBang is true, then we have:
OID PersistentIPLOID;

DEFQUEUE(RestartQueue);	// waiting for restart to finish

// Restart state:

LID lidNeeded = 0;

// We can be operating with up to two pages:
PageHeader * pageA;
PageHeader * pageB;
PageHeader * currentRootPageH;
CkptRoot * curRoot;

unsigned int restartPhase = restartPhase_Begin;

unsigned int genIndex;
uint32_t numDirFramesRead;
uint32_t numDirFramesProcessed;
LID curDirLID;

// Forward declarations:
static void IOReq_EndObDirRead(IORequest * ioreq);
static void IOReq_EndProcDirRead(IORequest * ioreq);
static void IOReq_EndGenHdrRead(IORequest * ioreq);

void
restart_LIDMounted(ObjectRange * rng)
{
  CalcLogExtent();

  // Are we waiting for this range?
  if (lidNeeded && range_Contains(rng, lidNeeded)) {
    assert(false); // FIXME implement this
  }

  if (restartPhase == restartPhase_Begin
      && LidToRange(CKPT_ROOT_0)
      && LidToRange(CKPT_ROOT_1) ) {
    // The two CkptRoots are mounted. Start the migrator thread.
    act_Wakeup(migratorActivity);
  }
}

static PageHeader *
StartCkptRootRead(LID lid)
{
  ObjectRange * rng = LidToRange(lid);

  IORequest * ioreq = AllocateIOReqAndPage();
  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(lid - rng->start);
  ioreq->doneFn = &IOReq_EndRead;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
  PageHeader * pageH = ioreq->pageH;
  pageH_ToObj(pageH)->obType = ot_PtKernelUse;
  return pageH;
}

static bool
ValidateCkptRoot(CkptRoot * root)
{
  if (root->versionNumber != CkptRootVersion) {
    DEBUG(restart) printf("Ckpt root %#x has version %d\n",
                          root, root->versionNumber);
    return false;
  }
  if (root->mostRecentGenerationNumber != root->checkGenNum) {
    DEBUG(restart) printf("Ckpt root %#x has mrgn=%d ckgn=%d\n",
                          root, root->mostRecentGenerationNumber,
                          root->checkGenNum );
    return false;
  }
  if (root->integrityByte != IntegrityByteValue) {
    DEBUG(restart) printf("Ckpt root %#x has integrityByte %d\n",
                          root, root->integrityByte);
    return false;
  }
  return true;
}

static void
AdjustNPCounts(void)
{
  // Adjust allocation and call counts of non-persistent objects.
  assert(restartNPCount == 0);	// not updated yet
  ObCount newRestartCount = curRoot->maxNPCount + 1;
#if 0
  printf("AdjustNPCounts: new cnt = %d\n", newRestartCount);
#endif
  // The +1 above ensures that capabilities to non-persistent objects
  // in persistent objects will be rescinded.

  // restartNPCount ensures that any new NP objects will
  // have the correct count:
  restartNPCount = newRestartCount;

  // Adjust the counts of existing objects.
  unsigned long objNum;

  // Scan all nodes:
  for (objNum = 0; objNum < objC_nNodes; objNum++) {
    int i;
    Node * pNode = objC_GetCoreNodeFrame(objNum);
    ObjectHeader * pObj = node_ToObj(pNode);

    if (pObj->obType == ot_NtFreeFrame)
      continue;

    if (! OIDIsPersistent(pObj->oid)) {
      pObj->allocCount += newRestartCount;
      if (pObj->allocCount > maxNPCount)
        maxNPCount = pObj->allocCount;
      pNode->callCount += newRestartCount;
      if (pNode->callCount > maxNPCount)
        maxNPCount = pNode->callCount;

      for (i = 0; i < EROS_NODE_SIZE; i++) {
        Key * pKey = node_GetKeyAtSlot(pNode, i);
        if (keyBits_IsRdHazard(pKey))
          key_ClearHazard(pKey);
        if (keyBits_IsUnprepared(pKey) && keyBits_IsObjectKey(pKey)
            && ! OIDIsPersistent(pKey->u.unprep.oid) ) {
          if (keyBits_IsHazard(pKey))
            key_ClearHazard(pKey);
          pKey->u.unprep.count += newRestartCount;
        }
      }
    }
    // else, there could be persistent objects if they were preloaded.
  }

  // Scan all pages:
  for (objNum = 0; objNum < objC_nPages; objNum++) {
    PageHeader * pageH = objC_GetCorePageFrame(objNum);
    ObjectHeader * pObj = pageH_ToObj(pageH);

    if ((pObj->obType == ot_PtDataPage || pObj->obType == ot_PtDevicePage)
        && ! OIDIsPersistent(pObj->oid) ) {
      pObj->allocCount += newRestartCount;
      if (pObj->allocCount > maxNPCount)
        maxNPCount = pObj->allocCount;
    }
  }
}

static void
FinishRestart(void)
{
  RTCOfRestart = RtcRead();
  /* Persistent monotonic time picks up where the checkpoint left off: */
  monotonicTimeOfRestart = monotonicTimeOfLastDemarc;

  nextRetiredGeneration = migratedGeneration;

  memcpy(unmigratedGenHdrLid, &curRoot->generations,
         sizeof(curRoot->generations));
  // wkgUGHL is initialized to &unmigratedGenHdrLid[0].

  logSizeLimited = (OIDToFrame(logWrapPoint - MAIN_LOG_START)
                    * KTUNE_LOG_LIMIT_PERCENT_NUMERATOR)
                   / LOG_LIMIT_PERCENT_DENOMINATOR;

  logCursor
    = IncrementLID(curRoot->generations[0]);

  PostCheckpointProcessing();

  ReleasePageFrame(currentRootPageH);

  restartPhase = restartPhase_Done;

  // Restart is done and the migrator is initialized (it has allocated
  // all the space it needs).
  act_Wakeup(checkpointActivity);
  sq_WakeAll(&RestartQueue);
}

static void
DoRestartPhaseWaitingRoot1(void)
{
  // CKPT_ROOT_1 has been read. Check CKPT_ROOT_0.
  if (pageA->ioreq) {
    act_SleepOn(&pageA->ioreq->sq);
    act_Yield();	// act_Yield does not return.
  }
  // Find the more recent root:
  CkptRoot * root0 = (CkptRoot *)pageH_GetPageVAddr(pageA);
  CkptRoot * root1 = (CkptRoot *)pageH_GetPageVAddr(pageB);
  DEBUG(restart) printf("DoRestartStep got headers %#x %#x\n",
                         root0, root1);
  if (! ValidateCkptRoot(root0)) {
    assert(ValidateCkptRoot(root1));	// else no valid root
    curRoot = root1;
  } else {
    if (! ValidateCkptRoot(root1)) {
      curRoot = root0;
    } else {
      // Both roots are valid. Choose the newer one.
      DEBUG(restart)
        printf("Root 0 restart gen=%lld, root 1 restart gen=%lld\n",
          root0->mostRecentGenerationNumber, root1->mostRecentGenerationNumber);
      curRoot = root0->mostRecentGenerationNumber
                > root1->mostRecentGenerationNumber
                ? root0 : root1;
    }
  }

  if (curRoot == root0) {
    currentRootLID = CKPT_ROOT_0;
    ReleasePageFrame(pageB);	// Free the old root.
    currentRootPageH = pageA;
  } else {
    currentRootLID = CKPT_ROOT_1;
    ReleasePageFrame(pageA);
    currentRootPageH = pageB;
  }
  DEBUG(restart) printf("Using root at %#llx\n", currentRootLID);

  if (IsPreloadedBigBang) {
    /* We are doing a big bang by preloading the persistent objects.
    Ignore the contents of the disk. */

    DEBUG(restart) printf("Restarting from preloaded big bang.\n");

    // Is the amount of mounted log reasonable?
    if (OIDToFrame(logWrapPoint) < physMem_TotalPhysicalPages) {
      assert(!"implemented");	// FIXME wait for more log to be mounted
    }

    // Oldest unmigrated generation starts at the beginning of the log:
    curRoot->generations[0] = MAIN_LOG_START - FrameToOID(1);
			// should probably be logWrapPoint instead

    /* GenNum 0 is used as a special case for nextRetiredGeneration.
    GenNum 1 will be the (nonexistent) restart generation, which needs
    no migration.
    GenNum 2 will be the first actual generation. */
    migratedGeneration = workingGenerationNumber = 1;
    monotonicTimeOfLastDemarc = 0;
    // RTCOfRestartDemarc = 0;	// it's initialized that way

    FinishRestart();

    // Start the persistent IPL process.
    DEBUG(restart) printf("Starting persistent IPL proc oid=%#llx\n",
                          PersistentIPLOID);
    StartActivity(PersistentIPLOID, restartNPCount, actHaz_None);

    return;	// restart is done!
  }

  DEBUG(restart) printf("Restarting from checkpoint.\n");

  AdjustNPCounts();

  // This is actually the restart generation number; workingGenerationNumber
  // will be incremented in FinishRestart.
  workingGenerationNumber = curRoot->mostRecentGenerationNumber;

  genIndex = curRoot->numUnmigratedGenerations;
  DEBUG(restart) printf("NumUnmigrGens=%d\n", genIndex);
  assert(0 < genIndex && genIndex <= MaxUnmigratedGenerations);

  restartPhase = restartPhase_Phase4;
  return;
}

static void
AddObjDescriptor(struct DiskObjectDescriptor * dod)
{
  ObjectDescriptor od;

  // dod is unaligned and packed, so use memcpy.
  memcpy(&od.oid, &dod->oid, sizeof(OID));
  memcpy(&od.allocCount, &dod->allocCount, sizeof(ObCount));
  memcpy(&od.callCount, &dod->callCount, sizeof(ObCount));
  memcpy(&od.logLoc, &dod->logLoc, sizeof(LID));
  memcpy(&od.type, &dod->type, sizeof(uint8_t));

  ld_recordLocation(&od, workingGenerationNumber - genIndex);
}

static void
AddProcDescriptor(struct DiskProcessDescriptor * dpd)
{
  // dpd is unaligned and packed, so use memcpy.
  OID oid;
  ObCount count;
  memcpy(&oid, &dpd->oid, sizeof(OID));
  memcpy(&count, &dpd->callCount, sizeof(ObCount));
  uint8_t haz = dpd->actHazard;

  assert(haz < actHaz_END);

  DEBUG(restart) {
    printf("Starting proc at oid %#llx haz %d", oid, haz);
    if (haz == actHaz_WakeResume)
      printf(" cc %#x", count);
    printf("\n");
  }

  StartActivity(oid, count, haz);
}

static void
ReadDirFrame(PageHeader * pageH, 
  void (*doneFn)(struct IORequest * ioreq))
{
  IORequest * ioreq = pageH->ioreq;	// already has an IORequest
  assert(ioreq->pageH == pageH);

  ObjectRange * rng = LidToRange(curDirLID);
  assert(rng);	// it had better be mounted

  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(curDirLID - rng->start);
  ioreq->doneFn = doneFn;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
  DEBUG(restart) printf("Reading dir, pageH=%#x ioreq=%#x\n",
                        pageH, ioreq);

  curDirLID = IncrementLID(curDirLID);
  numDirFramesRead++;
}

static void
ReadProcDirFrame(PageHeader * pageH)
{
  ReadDirFrame(pageH, &IOReq_EndProcDirRead);
}

static void
ReadGenHdr(void)
{
  LID lid = curRoot->generations[--genIndex];
  assert(lid != UNUSED_LID);

  PageHeader * pageH = GenHdrPageH;
  IORequest * ioreq = pageH->ioreq;	// already has an IORequest

  ObjectRange * rng = LidToRange(lid);
  assert(rng);	// it had better be mounted

  ioreq->requestCode = capros_IOReqQ_RequestType_readRangeLoc;
  ioreq->objRange = rng;
    ioreq->rangeLoc = OIDToFrame(lid - rng->start);
    ioreq->doneFn = &IOReq_EndGenHdrRead;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);
}

static void
FreePageAndIOReq(PageHeader * pageH)
{
  IORequest * ioreq = pageH->ioreq;
  ReleasePageFrame(pageH);
  IOReq_Deallocate(ioreq);
}

static void
DoneProcessingProcDirFrames(void)
{
  RTCOfRestartDemarc = genHdr->RTCOfDemarc;
  monotonicTimeOfLastDemarc = genHdr->persistentTimeOfDemarc;
  monotonicTimeOfRestart = monotonicTimeOfLastDemarc;

  migratedGeneration = curRoot->mostRecentGenerationNumber
                       - curRoot->numUnmigratedGenerations;

  DEBUG(restart) printf("DoneProcessingProcDirFrames wkgGen=%d+1 migGen=%d\n",
                   workingGenerationNumber, migratedGeneration);

  // Free all the pages we used:
  FreePageAndIOReq(GenHdrPageH);
  while (reservedPages) {
    PageHeader * pageH = reservedPages;
    reservedPages = pageH->kt_u.link.next;
    numReservedPages--;
    FreePageAndIOReq(pageH);
  }

  FinishRestart();
}

static void
DoneProcessingObDirFrames(void)
{
  DEBUG(restart) printf("Restart DoneProcessingObDirFrames\n");

  // We are done processing object directory frames for the current generation.
  if (genIndex > 0) {
    ReadGenHdr();	// read the header for the next generation
  } else {
    // We have reached the restart generation.

    // Process any process directory entries in the header:
    int i;
    struct DiskProcessDescriptor * procDescr
      = (struct DiskProcessDescriptor *) ((char *)genHdr + sizeof(*genHdr));
    for (i = 0; i < genHdr->processDir.nDescriptors; i++, procDescr++) {
      assert((char *)procDescr <= (char *)genHdr + EROS_PAGE_SIZE
                                 - sizeof(struct DiskProcessDescriptor));
      AddProcDescriptor(procDescr);
    }

    // Read the process directory.
    numDirFramesRead = 0;
    numDirFramesProcessed = 0;
    curDirLID = genHdr->processDir.firstDirFrame;

    if (genHdr->processDir.nDirFrames > 0) {
      // Use the reserved frames to read process directory frames:
      PageHeader * pageH = reservedPages;
      while (pageH && numDirFramesRead < genHdr->processDir.nDirFrames) {
        ReadProcDirFrame(pageH);
        pageH = pageH->kt_u.link.next;
      }
    } else {
      // No frames to read, so we are done reading.
      DoneProcessingProcDirFrames();
    }
  }
}

static void
ReadObDirFrame(PageHeader * pageH)
{
  ReadDirFrame(pageH, &IOReq_EndObDirRead);
}

static void
IOReq_EndObDirRead(IORequest * ioreq)
{
  DEBUG(restart) printf("EndObDirRead\n");
  assert(numDirFramesProcessed < numDirFramesRead);

  PageHeader * pageH = ioreq->pageH;
  kva_t pageAddr = pageH_GetPageVAddr(pageH);

  // The IORequest is done.
  assert(sq_IsEmpty(&ioreq->sq));

  // Process the object directory entries in this frame.
  int i;
  uint32_t numDescrs = *(uint32_t *)pageAddr;
  assert(numDescrs <= (EROS_PAGE_SIZE - sizeof(uint32_t))
                      / sizeof(struct DiskObjectDescriptor) );
  struct DiskObjectDescriptor * objDescr
    = (struct DiskObjectDescriptor *) (pageAddr + sizeof(uint32_t));
  for (i = 0; i < numDescrs; i++, objDescr++) {
    AddObjDescriptor(objDescr);
  }

  numDirFramesProcessed++;

  DEBUG(restart) printf("EndObDirRead, nodfr=%d, nodfp=%d, ndf=%d\n",
    numDirFramesRead, numDirFramesProcessed, genHdr->objectDir.nDirFrames);

  // If there are more frames to be read, read one.
  if (numDirFramesRead < genHdr->objectDir.nDirFrames) {
    ReadObDirFrame(pageH);
  } else {
    // Done with this page for this generation.
    // Just leave it in the reserved pool.

    if (numDirFramesProcessed >= genHdr->objectDir.nDirFrames)
      DoneProcessingObDirFrames();
  }
}

static void
IOReq_EndProcDirRead(IORequest * ioreq)
{
  DEBUG(restart) printf("EndProcDirRead\n");

  PageHeader * pageH = ioreq->pageH;
  kva_t pageAddr = pageH_GetPageVAddr(pageH);

  // The IORequest is done.
  assert(sq_IsEmpty(&ioreq->sq));

  // Process the process directory entries in this frame.
  int i;
  uint32_t numDescrs = *(uint32_t *)pageAddr;
  assert(numDescrs <= (EROS_PAGE_SIZE - sizeof(uint32_t))
                      / sizeof(struct DiskProcessDescriptor) );
  struct DiskProcessDescriptor * procDescr
    = (struct DiskProcessDescriptor *) (pageAddr + sizeof(uint32_t));
  for (i = 0; i < numDescrs; i++, procDescr++) {
    AddProcDescriptor(procDescr);
  }

  numDirFramesProcessed++;
  assert(numDirFramesProcessed <= numDirFramesRead);

  DEBUG(restart) printf("EndProcDirRead, nodfr=%d, nodfp=%d, ndf=%d\n",
    numDirFramesRead, numDirFramesProcessed, genHdr->processDir.nDirFrames);

  // If there are more frames to be read, read one.
  if (numDirFramesRead < genHdr->processDir.nDirFrames) {
    ReadProcDirFrame(pageH);
  } else {
    // Done with this page for this generation.
    // Just leave it in the reserved pool.

    if (numDirFramesProcessed >= genHdr->processDir.nDirFrames)
      DoneProcessingProcDirFrames();
  }
}

static void
IOReq_EndGenHdrRead(IORequest * ioreq)
{
  DEBUG(restart) printf("EndGenHdrRead genIndex=%d gh->genNum=%d wkgGen=%d\n",
             genIndex, genHdr->generationNumber, workingGenerationNumber);

  // The IORequest is done.
  assert(sq_IsEmpty(&ioreq->sq));

  assert(genHdr->versionNumber == 1);
  assert(genHdr->generationNumber == workingGenerationNumber - genIndex);

  // Process any object directory entries in the header.
  int i;
  struct DiskObjectDescriptor * objDescr = (struct DiskObjectDescriptor *)
     ((char *)genHdr + sizeof(*genHdr)
      + (genHdr->processDir.nDescriptors
         * sizeof(struct DiskProcessDescriptor) ) );
  for (i = 0; i < genHdr->objectDir.nDescriptors; i++, objDescr++) {
    assert((char *)objDescr <= (char *)genHdr + EROS_PAGE_SIZE
                               - sizeof(struct DiskObjectDescriptor));
    AddObjDescriptor(objDescr);
  }

  numDirFramesRead = 0;
  numDirFramesProcessed = 0;
  curDirLID = genHdr->objectDir.firstDirFrame;

  if (genHdr->objectDir.nDirFrames > 0) {
    // Use the reserved frames to read object directory frames:
    PageHeader * pageH = reservedPages;
    while (pageH && numDirFramesRead < genHdr->objectDir.nDirFrames) {
      ReadObDirFrame(pageH);
      pageH = pageH->kt_u.link.next;
    }
  } else {
    // No obdir frames to read, so we are done reading obdir frames.
    DoneProcessingObDirFrames();
  }
}

static void
DoRestartPhase4(void)
{
  DEBUG(restart) printf("Restart DoRestartPhase4\n");

  // Is the amount of mounted log reasonable?
  if (logWrapPoint < curRoot->endLog) {
    assert(!"implemented");	// FIXME wait for more log to be mounted
  }

  // Get a page for reading the generation headers:
  if (! GenHdrPageH) {
    GenHdrPageH = AllocateIOReqCleaningAndPage()->pageH;
    genHdr = (DiskGenerationHdr *)pageH_GetPageVAddr(GenHdrPageH);
  }

  // Get a pool of pages for reading the object directory:
  // What is the right number to reserve? Get 2 for now.
  // Must have an IORequest for each one, plus one for the header.
  assert(KTUNE_NIORQS >= (2+1));
  ReservePages(2);

  // Reserve an IORequest for each page:
  PageHeader * pageH;
  for (pageH = reservedPages; pageH; pageH = pageH->kt_u.link.next) {
    IORequest * ioreq = IOReqCleaning_Allocate();
    /* We should not be using IORequests for anything else now,
    so we should be assured of getting one: */
    assert(ioreq);
    // Link with page:
    pageH->ioreq = ioreq;
    ioreq->pageH = pageH;
  }

  // Load the object directory from each generation, oldest first:
  ReadGenHdr();

  /* The rest of the process of reading generation headers and 
  directory frames occurs in the IOReq completion routines.
  This puts the cost on the disk process rather than the restart process,
  but that's not a problem. */

  // Wait for that process to finish:
  SleepOnPFHQueue(&RestartQueue);	//// ??
}

void
DoRestartStep(void)
{
  DEBUG(restart) printf("DoRestartStep %d\n", restartPhase);

  while (restartPhase != restartPhase_Done) {
    switch (restartPhase) {
    case restartPhase_Begin:
      // CKPT_ROOT_0 and CKPT_ROOT_1 are mounted.
      pageA = StartCkptRootRead(CKPT_ROOT_0);
      restartPhase = restartPhase_QueuingRoot1;
    case restartPhase_QueuingRoot1:
      pageB = StartCkptRootRead(CKPT_ROOT_1);
      restartPhase = restartPhase_WaitingRoot1;
      act_SleepOn(&pageB->ioreq->sq);
      act_Yield();	// act_Yield does not return.

    case restartPhase_WaitingRoot1:
      DoRestartPhaseWaitingRoot1();
      break;

    case restartPhase_Phase4:
      DoRestartPhase4();
      break;

    case restartPhase_Done:
      break;
    }
  }
}
