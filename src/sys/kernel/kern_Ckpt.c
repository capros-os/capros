/*
 * Copyright (C) 2008, Strawberry Development Group.
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
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Activity.h>
#include <kerninc/IORQ.h>
#include <kerninc/Ckpt.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/Check.h>
#include <kerninc/ObjH-inline.h>
#include <idl/capros/Range.h>
#include <idl/capros/MigratorTool.h>
#include <eros/Invoke.h>
#include <eros/machine/IORQ.h>

#define dbg_ckpt	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_ckpt )

#define DEBUG(x) if (dbg_##x & dbg_flags)

unsigned int ckptState = ckpt_NotActive;

long numKRODirtyPages = 0;	// including pots
long numKRONodes = 0;
unsigned int KROPageCleanCursor;
unsigned int KRONodeCleanCursor;

PageHeader * GenHdrPageH;
LID nextProcDirLid;

DEFQUEUE(WaitForCkptInactive);
DEFQUEUE(WaitForCkptNeeded);

/* Return the amount of log needed to checkpoint all the dirty objects. */
unsigned long
CalcLogReservation(void)
{
  unsigned long total = 0;
  total += (numDirtyObjects[capros_Range_otNode] + DISK_NODES_PER_PAGE - 1)
             / DISK_NODES_PER_PAGE;
  total += numDirtyObjects[capros_Range_otPage];
  total += numDirtyLogPots;
  // Space to write out the log directory:
#define DirEntsPerPage (EROS_PAGE_SIZE / sizeof(ObjectDescriptor))
  total += (ld_numWorkingEntries()
            + numDirtyObjects[capros_Range_otNode]
            + numDirtyObjects[capros_Range_otPage] + DirEntsPerPage - 1)
           / DirEntsPerPage;
  // Space to write out the maximum process directory:
#define ProcEntsPerPage (EROS_PAGE_SIZE / sizeof(struct DiskProcessDescriptor))
  total += (KTUNE_NCONTEXT + ProcEntsPerPage - 1) / ProcEntsPerPage;
  total += 1;	// for the generation header frame
  return total;
#undef ProcEntsPerPage
#undef DirEntsPerPage
}

PageHeader * reservedPages = NULL;
unsigned int numReservedPages = 0;

// May Yield.
static void
ReservePages(unsigned int numPagesWanted)
{
  while (numReservedPages < numPagesWanted) {
    PageHeader * pageH = objC_GrabPageFrame();
    numReservedPages++;
    // Link into free list:
    *(PageHeader * *)&pageH->kt_u.free.freeLink.next = reservedPages;
    reservedPages = pageH;
  }
}

// Declare a demarcation event, which is the beginning of a checkpoint.
// Yields.
void
DeclareDemarcationEvent(void)
{
  assert(!ckptIsActive());

  ckptState = ckpt_Phase1;
  sq_WakeAll(&WaitForCkptNeeded, false);
}

void
CheckpointPage(PageHeader * pageH)
{
  ObjectHeader * pObj = pageH_ToObj(pageH);

  if (objH_GetFlags(pObj, OFLG_DIRTY)) {
    assert(! objH_GetFlags(pObj, OFLG_KRO));
    // Make this page Kernel Read Only.
    switch (pageH_GetObType(pageH)) {
    default:
      break;

    case ot_PtDataPage:
      pageH_MakeReadOnly(pageH);
    }
    numKRODirtyPages++;
    objH_SetFlags(pObj, OFLG_KRO);
  }
  // else it's not marked dirty, in which case it isn't mapped with write
  // access, because that's how we track whether it becomes dirty.
}

// DoPhase1Work does NOT Yield. It must be atomic.
static void
DoPhase1Work(void)
{
  int i;
  // Grab a page for the generation header:
  GenHdrPageH = objC_GrabPageFrame();
  DiskGenerationHdr * genHdr
    = (DiskGenerationHdr *)pageH_GetPageVAddr(GenHdrPageH);

  // Reserve pages for the process directory.

  int numActivities = KTUNE_NACTIVITY - numFreeActivities;
  /* Note, some of the Activitys will be for non-persistent processes,
  so this is an upper bound on the number we need.
  It won't hurt to have an extra free page or two around. */

  // Some can go in the generation header:
  numActivities -= (EROS_PAGE_SIZE - sizeof(DiskGenerationHdr))
                   / sizeof(struct DiskProcessDescriptor);
  // Reserve pages for the rest:
  if (numActivities > 0) {
#define numActivitiesPerPage (EROS_PAGE_SIZE / sizeof(struct DiskProcessDescriptor))
    ReservePages((numActivities + numActivitiesPerPage - 1)
                 / numActivitiesPerPage);
    /* Note, if the above Yields, when restarted we will start at
    DoPhase1Work, which will recalculate numActivities,
    which may have changed. */
  }

  /* This is the moment of demarcation.
  From here through the end of DoPhase1Work, we must NOT Yield.
  This must be atomic. */

  // Don't checkpoint a broken system:
  check_Consistency("before ckpt");

  KROPageCleanCursor = 0;
  KRONodeCleanCursor = 0;

  // Scan all pages.
  unsigned long objNum;
  for (objNum = 0; objNum < objC_nPages; objNum++) {
    PageHeader * pageH = objC_GetCorePageFrame(objNum);

    switch (pageH_GetObType(pageH)) {
    default:
      break;

    case ot_PtTagPot:
    case ot_PtHomePot:
      assertex(pageH, ! objH_GetFlags(pageH_ToObj(pageH), OFLG_KRO));

      if (objH_GetFlags(pageH_ToObj(pageH), OFLG_DIRTY)) {
        assert(!"complete");	// figure this out later
      }
      break;

    case ot_PtDataPage:
      if (! OIDIsPersistent(pageH_ToObj(pageH)->oid))
        break;
    case ot_PtLogPot:
      assertex(pageH, ! objH_GetFlags(pageH_ToObj(pageH), OFLG_KRO));

      CheckpointPage(pageH);
      break;
    }
  }

  // Scan all nodes.
  for (objNum = 0; objNum < objC_nNodes; objNum++) {
    Node * pNode = objC_GetCoreNodeFrame(objNum);
    ObjectHeader * pObj = node_ToObj(pNode);

    if (pObj->obType == ot_NtFreeFrame)
      continue;

    if (! OIDIsPersistent(pObj->oid))
      break;

    if (objH_GetFlags(pObj, OFLG_DIRTY)) {
      assert(! objH_GetFlags(pObj, OFLG_KRO));
      // Make this node Kernel Read Only.

      /* Unpreparing the node ensures that when we next try to dirty
       * the node, we will notice it is KRO. */
      node_Unprepare(pNode);
      numKRONodes++;
      objH_SetFlags(pObj, OFLG_KRO);
    }
  }

  // Save Activity's to the process directory.
  struct DiskProcessDescriptor * dpd;
  dpd = (struct DiskProcessDescriptor *)
        ((char *)genHdr + sizeof(DiskGenerationHdr));
  unsigned int dpdsInCurrentPage = 0;
  bool inHdr = true;
  PageHeader * * nextProcDirFramePP = &reservedPages;
  // Where to store the number of descriptors in the current page:
  uint32_t * numDpdsLoc = &genHdr->processDir.nDescriptors;
  genHdr->processDir.firstDirFrame = 0;
  genHdr->processDir.nDirFrames = 0;	// so far
  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    Activity * act = &act_ActivityTable[i];
    if (act->state != act_Free) {
      OID procOid;
      ObCount procAllocCount;
      if (act->context) {	// process info is in the Process structure
        Process * proc = act->context;
        if (proc_IsKernel(proc))
          continue;
        ObjectHeader * pObj = node_ToObj(proc->procRoot);
#if 0
        printf("P1 act=%#x proc=%#x root=%#x\n", act, proc, pObj);
#endif
        procOid = pObj->oid;
        procAllocCount = pObj->allocCount;
      } else {
        if (! keyBits_IsType(&act->processKey, KKT_Process))
          continue;	// process was rescinded
        procOid = key_GetKeyOid(&act->processKey);
        procAllocCount = key_GetAllocCount(&act->processKey);
      }
      if (OIDIsPersistent(procOid)) {
        // Is there room for another DiskProcessDescriptor in this page?
        kva_t roomInPage = (- (kva_t)dpd) & EROS_PAGE_MASK;
        if (roomInPage < sizeof(struct DiskProcessDescriptor)) {
          // Finish the current page.
          *numDpdsLoc = dpdsInCurrentPage;
          // Set up the next page.
          PageHeader * pageH = *nextProcDirFramePP;
          assert(pageH);	// else we didn't reserve enough!
          LID lid = NextLogLoc();
          if (inHdr) {		// this is the first full frame
            genHdr->processDir.firstDirFrame = lid;
            nextProcDirLid = lid;
            inHdr = false;
          }
          genHdr->processDir.nDirFrames++;
          numDpdsLoc = (uint32_t *)pageH_GetPageVAddr(pageH);
          dpd = (struct DiskProcessDescriptor *)
                ((kva_t)numDpdsLoc + sizeof(uint32_t));
          // Follow the chain:
          nextProcDirFramePP
            = (PageHeader * *)& pageH->kt_u.free.freeLink.next;
        }
        uint8_t hazToSave;
        if (act->state == act_Sleeping && act->actHazard == actHaz_None)
          // On restart, wake sleepers with an error.
          hazToSave = actHaz_WakeRestart;
        else
          hazToSave = act->actHazard;;
        // Use memcpy, because dpd is unaligned and packed.
        memcpy(&dpd->oid, &procOid, sizeof(OID));
        memcpy(&dpd->allocCount, &procAllocCount, sizeof(ObCount));
        memcpy(&dpd->actHazard, &hazToSave, sizeof(dpd->actHazard));
        dpd++;
        dpdsInCurrentPage++;
      }
    }
  }
  // Finish the last page.
  *numDpdsLoc = dpdsInCurrentPage;

  // Free any unused reserved pages:
  while (1) {
    PageHeader * pageH = *nextProcDirFramePP;
    if (! pageH)
      break;
    // Follow the chain:
    nextProcDirFramePP
      = (PageHeader * *)& pageH->kt_u.free.freeLink.next;
    ReleasePageFrame(pageH);
  }

  monotonicTimeOfLastDemarc = sysT_NowPersistent();

  ckptState = ckpt_Phase2;
}

static void
IOReq_EndDirWrite(IORequest * ioreq)
{
  // The IORequest is done.
  PageHeader * pageH = ioreq->pageH;

  // Mark the page as no longer having I/O.
  pageH->ioreq = NULL;

  assert(sq_IsEmpty(&ioreq->sq));
  // Caller has unlinked the ioreq and will deallocate it.

  ReleasePageFrame(pageH);
}

static void
DoPhase2Work(void)
{
  // FIXME! Since the checkpoint process has high priority,
  // it will always get IORequest blocks before users,
  // so "clean me first" won't work!

  // Write the process directory frames.
  while (reservedPages) {
    PageHeader * pageH = reservedPages;
    IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
    ioreq->pageH = pageH;	// link page and ioreq
    pageH->ioreq = ioreq;

    ObjectRange * rng = LidToRange(nextProcDirLid);
    assert(rng);	// it had better be mounted

    ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
    ioreq->objRange = rng;
    ioreq->rangeLoc = OIDToFrame(nextProcDirLid - rng->start);
    ioreq->doneFn = &IOReq_EndDirWrite;
    sq_Init(&ioreq->sq);	// won't be used
    ioreq_Enqueue(ioreq);

    reservedPages = *(PageHeader * *)&pageH->kt_u.free.freeLink.next;
    nextProcDirLid = IncrementLID(nextProcDirLid);
  }

  while (numKRONodes)
    CleanAKRONode();

  /* The stages in the life of a typical page are:
  1. Page is dirty
  2. A demarcation event occurs. The page is now dirty and KRO.
  3. The page is queued to be cleaned by the disk driver.
     It is marked KRO and clean. (We always mark a page clean *before*
     cleaning it, so we will know if it is dirtied while being cleaned.)
  4. The cleaning finishes. The page is now clean, not KRO. */
  while (numKRODirtyPages)
    CleanAKROPage();

  assert(!"complete");

  ckptState = ckpt_Phase3;
}

void
DoCheckpointStep(void)
{
  switch (ckptState) {
  default: ;
    assert(false);

  case ckpt_NotActive:
    DEBUG(ckpt) printf("DoCheckpointStep not active\n");
    act_SleepOn(&WaitForCkptNeeded);
    act_Yield();
    
  case ckpt_Phase1:
    DEBUG(ckpt) printf("DoCheckpointStep P1\n");
    DoPhase1Work();
  case ckpt_Phase2:
    DEBUG(ckpt) printf("DoCheckpointStep P2\n");
    DoPhase2Work();
    break;
  }
}

void
CheckpointThread(void)
{
  DEBUG(ckpt) printf("Start Checkpoint thread, act=%#x\n", checkpointActivity);

  Message Msg = {
    .snd_invKey = KR_MigrTool,
    .snd_code = OC_capros_MigratorTool_checkpointStep,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .rcv_limit = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID
  };

  for (;;) {
    CALL(&Msg);
  }
}
