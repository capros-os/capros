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
#include <kerninc/Check.h>
#include <kerninc/ObjH-inline.h>
#include <kerninc/Node-inline.h>
#include <idl/capros/Range.h>
#include <idl/capros/MigratorTool.h>
#include <eros/Invoke.h>
#include <eros/machine/IORQ.h>

#define dbg_ckpt	0x1
#define dbg_sync	0x2
#define dbg_numnodes	0x4
#define dbg_numpages	0x8
#define dbg_procs       0x10

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_ckpt )

#define DEBUG(x) if (dbg_##x & dbg_flags)

LID logCursor = 0;	// next place to write in the main log
LID logWrapPoint;	// end of main log
LID currentRootLID;	// CKPT_ROOT_0 or CKPT_ROOT_1

/* oldestNonRetiredGenLid is the LID following the last LID of the
 * newest retired generation. */
LID oldestNonRetiredGenLid;

/* workingGenFirstLid is the LID of the first frame of the
 * working generation. */
LID workingGenFirstLid;

/* logSizeLimited is the size of the main log times the limit percent. */
frame_t logSizeLimited;
GenNum workingGenerationNumber;
GenNum retiredGeneration = 0;

/* monotonicTimeOfLastDemarc is the time of the most recent demarcation event,
 * in units of nanoseconds.
 * That checkpoint may not be stabilized yet. */
uint64_t monotonicTimeOfLastDemarc;

unsigned int ckptState = ckpt_NotActive;

long numKRODirtyPages = 0;	// including pots
long numKRONodes = 0;
unsigned int KROPageCleanCursor = 0;	// next page to clean

GenNum migratedGeneration = 0;	// the latest fully migrated generation

/* The LIDs of the generation headers of all the unmigrated generations,
plus the newest migrated generation
in order from most recent (the restart generation) to older: */
LID unmigratedGenHdrLid[MaxUnmigratedGenerations + 1];
/* Index of the entry in unmigratedGenHdrLid containing the LID
of the GenHdr of the most recent generation (the restart generation). */
unsigned int wkgUGHL = 0;

DEFQUEUE(WaitForCkptInactive);
DEFQUEUE(WaitForCkptNeeded);
DEFQUEUE(WaitForObjDirWritten);

/*************************************************************************/
/* The following variables have state only while a checkpoint is active: */

PageHeader * GenHdrPageH = NULL;
DiskGenerationHdr * genHdr;
static LID thisGenHdrLid;

unsigned int KRONodeCleanCursor;	// next node to clean
/* All nodes before KRONodeCleanCursor are either not KRO
 * or are queued to be cleaned. */

PageHeader * * ProcDirFramesWritten;
unsigned long numDirEntsToSave;
unsigned int nextRangeToSync;
unsigned int rangesSynced;

/*
If no checkpoint is active, nextRetiredGeneration is undefined.
If a checkpoint is active and the numUnmigratedGenerations field in the
  CkptRoot has been fixed,
  nextRetiredGeneration has workingGenerationNumber - the value in that field.
If a checkpoint is active and the numUnmigratedGenerations field in the
  CkptRoot has not been fixed,
  nextRetiredGeneration has zero.
This handles the case in which migration of a generation is completed
  during the write of the checkpoint root. Such a generation isn't retired.
*/
GenNum nextRetiredGeneration;

unsigned int nextWkgUGHL;

/*************************************************************************/

/* During a checkpoint, persistent pages can be in the following states:

  OFLG_   Wkg I/O Generation(s) DirEnt Notes
KRO Dirty  *1  *2         *3    exists
 1    1    0   0  Wkg and Cur     no   need to clean
 1    1    1   0  Wkg             no   need to clean
 1    1    0  cln Wkg and Cur     no   Before the demarcation event, this page
                                       began to be cleaned and was redirtied.
 1    1    1  cln Wkg             no   Ditto, and the page was COW'ed before
                                       the clean finished.
 1    0    0  cln Wkg and Cur     yes  being cleaned *4
 1    0    1  cln Wkg             yes  being cleaned *4
 0    0    0   0  Wkg and Cur     yes  
 0    0    0  fet Wkg and Cur     yes  being fetched
 0    1    0   0          Cur     no   can't be cleaned now

 *1 0 indicates obType is ot_PtDataPage
    1 indicates obType is ot_PtWorkingCopy

 *2 This state is a combination of ioreq and OFLG_Fetching:
    0: ioreq == 0                       (no I/O)
    cln: ioreq != 0, OFLG_Fetching == 0 (being cleaned)
    fet: ioreq != 0, OFLG_Fetching == 1 (being fetched)

 *3 "Cur" means this page is the current version and can be found
    by objH_Lookup.

 *4 Page is nonzero (otherwise it would have been cleaned instantly)
*/

INLINE void
minEqualsL(long * var, long value)
{
  if (*var > value)
    *var = value;
}

INLINE void
maxEqualsL(long * var, long value)
{
  if (*var < value)
    *var = value;
}

static unsigned int
WrapCircularIndex(unsigned int idx, unsigned int max)
{
  if (idx >= max)
    return idx - max;
  return idx;
}

static LID
GetOldestNonRetiredGenLid(GenNum retGen)
{
  unsigned int numUnmigrGens = workingGenerationNumber - 1 - retGen;
  unsigned int c = WrapCircularIndex(wkgUGHL + numUnmigrGens,
                                     MaxUnmigratedGenerations + 1);
  return IncrementLID(unmigratedGenHdrLid[c]);
}

/* GetOldestNonNextRetiredGenLid should be called only while a checkpoint
 * is active.
 * It returns the LID following the last LID of
 * the generation returned by GetNextRetiredGeneration(). */
LID
GetOldestNonNextRetiredGenLid(void)
{
  return GetOldestNonRetiredGenLid(GetNextRetiredGeneration());
}

/* Return the amount of log needed to checkpoint all the dirty objects. */
unsigned long
CalcLogReservation(unsigned long numDirtyObjects[],
  unsigned long existingLogEntries)
{
  unsigned long total = 0;
  total += (numDirtyObjects[capros_Range_otNode] + DISK_NODES_PER_PAGE - 1)
             / DISK_NODES_PER_PAGE;
  total += numDirtyObjects[capros_Range_otPage];
  total += numDirtyLogPots;
  // Space to write out the log directory:
#define DirEntsPerPage (EROS_PAGE_SIZE / sizeof(ObjectDescriptor))
  total += (existingLogEntries
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

static void
IOReq_EndSync(IORequest * ioreq)
{
  // The IORequest is done.
  assert(sq_IsEmpty(&ioreq->sq));
  DEBUG(sync) printf("EndSync synced %d\n", rangesSynced);

  if (++rangesSynced >= nextRangeToSync)
    sq_WakeAll(&WaitForObjDirWritten);

  IOReq_Deallocate(ioreq);
}

// Wait for all the previous log writes to get to nonvolatile media.
static void
DoSync(void)
{
  while (nextRangeToSync < nLidRanges) {
    IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
    ObjectRange * rng = &lidRanges[nextRangeToSync++];

    ioreq->pageH = NULL;
    ioreq->requestCode = capros_IOReqQ_RequestType_synchronizeCache;
    ioreq->objRange = rng;
    ioreq->doneFn = &IOReq_EndSync;
    sq_Init(&ioreq->sq);	// won't be used
    ioreq_Enqueue(ioreq);
  }
  if (rangesSynced < nextRangeToSync)
    SleepOnPFHQueue(&WaitForObjDirWritten);
  // All writes are now completed.
  DEBUG(numpages) {
    unsigned int pageNum;
    // Scan all pages.
    for (pageNum = 0; pageNum < objC_nPages; pageNum++) {
      PageHeader * pageH = objC_GetCorePageFrame(pageNum);
      ObjectHeader * pObj = pageH_ToObj(pageH);
      (void)pObj;
      // Nothing should be being cleaned now.
      // GenHdrPageH is an exception because we leave pageH->ioreq set
      // even after the I/O is completed.
      assertex(pObj, pageH->ioreq == 0 || objH_GetFlags(pObj, OFLG_Fetching)
                     || pageH == GenHdrPageH);
      assertex(pObj, ! objH_IsKRO(pObj));
    }
  }
}


PageHeader * reservedPages = NULL;
unsigned int numReservedPages = 0;	// length of the above chain

// May Yield.
void
ReservePages(unsigned int numPagesWanted)
{
  while (numReservedPages < numPagesWanted) {
    PageHeader * pageH = objC_GrabPageFrame();
    numReservedPages++;
    // Link into free list:
    pageH->kt_u.link.next = reservedPages;
    reservedPages = pageH;
  }
}

// Declare a demarcation event, which is the beginning of a checkpoint.
// Yields.
void
DeclareDemarcationEvent(void)
{
  assert(!ckptIsActive());

  DEBUG(ckpt) printf("DeclareDemarcationEvent\n");

  nextRetiredGeneration = 0;

  ckptState = ckpt_Phase1;
  sq_WakeAll(&WaitForCkptNeeded);
}

// This is called after a checkpoint and also after restart.
void
PostCheckpointProcessing(void)
{
  assert(nextRetiredGeneration);
  retiredGeneration = nextRetiredGeneration;

  DEBUG(ckpt) printf("PostCkptProcessing  wkgGen=%d+1 retGen=%d\n",
                     workingGenerationNumber, retiredGeneration);

  // Start a new working generation:
  workingGenerationNumber++;

  // We have saved everything on disk, so migrated == retired.
  oldestNonRetiredGenLid = GetOldestNonNextRetiredGenLid();

  workingGenFirstLid = logCursor;

  ld_generationRetired(retiredGeneration);

  DEBUG(ckpt) check_Consistency("after ckpt");
}

// Variables for StoreProcessInfo:
static struct DiskProcessDescriptor * dpd;
unsigned int dpdsInCurrentPage;
bool inHdr;
// Where to store the number of descriptors in the current page:
uint32_t * numDpdsLoc;
PageHeader * * nextProcDirFramePP;
LID nextProcDirLid;

static struct DiskProcessDescriptor *
ProcFindDuplicate(OID procOid)
{
  struct DiskProcessDescriptor * dpdCursor = (struct DiskProcessDescriptor *)
        ((char *)genHdr + sizeof(DiskGenerationHdr));
  PageHeader * * nextProcDirFramePPCursor = &reservedPages;

  while (dpdCursor != dpd) {
    // Is there room for another DiskProcessDescriptor in this page?
    kva_t roomInPage = (- (kva_t)dpdCursor) & EROS_PAGE_MASK;
    if (roomInPage < sizeof(struct DiskProcessDescriptor)) {
      // Go to the next page.
      PageHeader * pageH = *nextProcDirFramePPCursor;
      assert(pageH);
      dpdCursor = (struct DiskProcessDescriptor *)
            (pageH_GetPageVAddr(pageH) + sizeof(uint32_t));
      // Follow the chain:
      nextProcDirFramePPCursor = & pageH->kt_u.link.next;
    }

    OID dpdOid;
    // Use memcpy, because dpd is unaligned and packed.
    memcpy(&dpdOid, &dpdCursor->oid, sizeof(OID));

    if (dpdOid == procOid) {	// OID matches
      return dpdCursor;		// this is a duplicate
    }

    // Go on to the next saved DiskProcessDescriptor:
    dpdCursor++;
  }
  return NULL;
}

static void
StoreProcessInfo(OID procOid,
  ObCount callCount,	// used only if hazToSave == actHaz_WakeResume
  uint8_t hazToSave)
{
  struct DiskProcessDescriptor * dpdCursor = ProcFindDuplicate(procOid);
  if (dpdCursor) {	// There is already an entry with this OID
    assert(dpdCursor->actHazard == actHaz_WakeResume);
	// because we store actHaz_WakeResume's first
    ObCount dpdCallCount;
    memcpy(&dpdCallCount, &dpdCursor->callCount, sizeof(ObCount));
    if (hazToSave == actHaz_WakeResume) {
      if (callCount > dpdCallCount) {
        // The old call count is stale. Replace it.
        memcpy(&dpdCursor->callCount, &callCount, sizeof(ObCount));
      }
    } else {
      // Any other hazard trumps actHaz_WakeResume, because
      // the callCount could be stale.
      dpdCursor->actHazard = hazToSave;
    }
    return;
  }

  // Is there room for another DiskProcessDescriptor in this page?
  kva_t roomInPage = (- (kva_t)dpd) & EROS_PAGE_MASK;
  if (roomInPage < sizeof(struct DiskProcessDescriptor)) {
    // Finish the current page.
    *numDpdsLoc = dpdsInCurrentPage;
    // Set up the next page.
    PageHeader * pageH = *nextProcDirFramePP;
    DEBUG(ckpt) printf("Process directory pageH %#x\n", pageH);
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
    nextProcDirFramePP = & pageH->kt_u.link.next;
  }
  // Use memcpy, because dpd is unaligned and packed.
  memcpy(&dpd->oid, &procOid, sizeof(OID));
  memcpy(&dpd->callCount, &callCount, sizeof(ObCount));
  memcpy(&dpd->actHazard, &hazToSave, sizeof(dpd->actHazard));
  dpd++;
  dpdsInCurrentPage++;
}

static void
CheckpointPage(PageHeader * pageH)
{
  ObjectHeader * pObj = pageH_ToObj(pageH);

  assert(! objH_IsKRO(pObj));

  if (objH_IsDirty(pObj)
    /* If this page is being cleaned, we must make it KRO to ensure
       it stays clean: */
      || (pageH->ioreq && ! objH_GetFlags(pObj, OFLG_Fetching))) {
    // Make this page Kernel Read Only.
    switch (pageH_GetObType(pageH)) {
    default:
      break;

    case ot_PtDataPage:
      pageH_MakeReadOnly(pageH);
    }
    pageH_BecomeUnwriteable(pageH);
    objH_SetFlags(pObj, OFLG_KRO);
    if (objH_IsDirty(pObj)) {
      numKRODirtyPages++;
    } else {
      /* This page is being cleaned and is clean.
      Since it is now KRO, we know it will stay clean, so we can create
      the log directory entry now. */
      CreateLogDirEntryForNonzeroPage(pageH);
    }
  }
}

#if (dbg_numnodes & dbg_flags)
static void
ValidateNumKRONodes(void)
{
  unsigned int nodeNum;
  unsigned int calcKRO = 0;
  for (nodeNum = 0; nodeNum < objC_nNodes; nodeNum++) {
    Node * pNode = objC_GetCoreNodeFrame(nodeNum);
    ObjectHeader * pObj = node_ToObj(pNode);
    if (objH_IsKRO(pObj)) {
      calcKRO++;
    }
  }
  assert(numKRONodes == calcKRO);
}
#endif

#if (dbg_numpages & dbg_flags)
static void
ValidateNumKROPages(void)
{
  unsigned int pageNum;
  unsigned int calcKRO = 0;
  // Scan all pages.
  for (pageNum = 0; pageNum < objC_nPages; pageNum++) {
    PageHeader * pageH = objC_GetCorePageFrame(pageNum);
    ObjectHeader * pObj = pageH_ToObj(pageH);
    switch (pObj->obType) {
    default:
      assertex(pageH, ! pageH->ioreq);
    case ot_PtTagPot:
    case ot_PtHomePot:
    case ot_PtLogPot:
      assertex(pageH, ! objH_IsKRO(pObj));
      break;

    case ot_PtWorkingCopy:
    case ot_PtDataPage:
      if (objH_IsKRO(pObj)) {
        if (objH_GetFlags(pObj, OFLG_DIRTY)) {
          calcKRO++;
        } else {
          // Must be being cleaned:
          assertex(pageH, pageH->ioreq && ! objH_GetFlags(pObj, OFLG_Fetching));
        }
      }
    }
  }
  assert(numKRODirtyPages == calcKRO);
}
#endif

static void
DoPhase1Work(void)
{
  int i;
  unsigned long objNum;

  // Grab a page for the generation header (if this is our first time
  // through here this checkpoint):
  if (! GenHdrPageH) {
    GenHdrPageH = objC_GrabPageFrame();
    genHdr = (DiskGenerationHdr *)pageH_GetPageVAddr(GenHdrPageH);
  }

  /* The number of process directory entries we need is:
  - the number of Activity's allocated to persistent processes, plus
  - the number of Resume keys to persistent processes in
    non-persistent nodes. 

  The latter number is unknown and potentially large.
  However, note that when we restart from this checkpoint,
  all the process directory entries must be loaded into
  Activity structures. If there are more process directory entries
  than there are Activity structures, we will be unable to restart.
  Therefore the number of Activity structures is an upper bound
  on the number of process directory entries we need to save,
  if the checkpoint is to be restartable and the number of
  Activity structures is the same on restart as it is now.
  */
  long numActivities = KTUNE_NACTIVITY;

  // Some can go in the generation header:
  numActivities -= (EROS_PAGE_SIZE - sizeof(DiskGenerationHdr))
                   / sizeof(struct DiskProcessDescriptor);
  minEqualsL(&numActivities, 0);
  // Number of pages we need for the rest:
#define numActivitiesPerPage (EROS_PAGE_SIZE / sizeof(struct DiskProcessDescriptor))
  long pagesToReserve = (numActivities + numActivitiesPerPage - 1)
                       / numActivitiesPerPage;
  // Reserve at least 2 for object directory frames.
  // We reserve them now rather than in phase 3, because at that time
  // no cleaning can be done, so pages might not be available.
  /* FIXME: What is the right number to reserve?
  We don't know the exact number of directory frames that will be needed,
  because the directory entries haven't all been created yet.
  We should have a few for each disk. */
  maxEqualsL(&pagesToReserve, 2);
  ReservePages(pagesToReserve);

  DEBUG(ckpt) printf("In phase 1, reservedPages=%#x\n", reservedPages);

  /* This is the moment of demarcation.
  From here through the end of DoPhase1Work, we must NOT Yield.
  This must be atomic. */

  monotonicTimeOfLastDemarc = sysT_NowPersistent();
  genHdr->persistentTimeOfDemarc = monotonicTimeOfLastDemarc;

  // Don't checkpoint a broken system:
  check_Consistency("before ckpt");

  KRONodeCleanCursor = 0;

  // Initialize StoreProcessInfo:
  dpd = (struct DiskProcessDescriptor *)
        ((char *)genHdr + sizeof(DiskGenerationHdr));
  dpdsInCurrentPage = 0;
  inHdr = true;
  nextProcDirFramePP = &reservedPages;
  numDpdsLoc = &genHdr->processDir.nDescriptors;
  genHdr->processDir.firstDirFrame = 0;
  genHdr->processDir.nDirFrames = 0;	// so far

  // Scan all nodes.
  for (objNum = 0; objNum < objC_nNodes; objNum++) {
    Node * pNode = objC_GetCoreNodeFrame(objNum);
    ObjectHeader * pObj = node_ToObj(pNode);

    if (pObj->obType == ot_NtFreeFrame)
      continue;

    if (OIDIsPersistent(pObj->oid)) {
      if (objH_IsDirty(pObj)) {
        assert(! objH_IsKRO(pObj));
        // Make this node Kernel Read Only.

        /* Unpreparing the node ensures that when we next try to dirty
         * the node, we will notice it is KRO. */
        node_Unprepare(pNode);
        numKRONodes++;
        nodeH_BecomeUnwriteable(pNode);
        objH_SetFlags(pObj, OFLG_KRO);
      }
    } else {	// a non-persistent node
      for (i = 0; i < EROS_NODE_SIZE; i++) {
        Key * pKey = node_SlotIsResume(pNode, i);
        if (pKey) {
          OID procOid;
          ObCount procCallCount;
          // This is similar to key_GetKeyOid.
          if (keyBits_IsPrepared(pKey)) {
            Node * pNode = pKey->u.gk.pContext->procRoot;
            ObjectHeader * pObj = node_ToObj(pNode);
            procOid = pObj->oid;
            procCallCount = pNode->callCount;
          } else {
            procOid = pKey->u.unprep.oid;
            procCallCount = pKey->u.unprep.count;
          }
          if (OIDIsPersistent(procOid)) {
            DEBUG(procs) printf("Resume key to %#llx in NP node\n", procOid);

            /* pKey is a resume key in a non-persistent node
            to a persistent process.
            On a restart from this checkpoint, the non-persistent world
            will be reinitialized and this resume key will be lost.
            To prevent the persistent process from hanging forever,
            we checkpoint it in a way so that when restarted,
            it will see an error return from the resume key. */

            /* There could be more than one such resume key to the same
            process, so StoreProcessInfo will look for a duplicate
            and use the more recent call count. */
            // CallCntUsed doesn't matter because on restart all objs have it.
            StoreProcessInfo(procOid, procCallCount, actHaz_WakeResume);
          }
        }
      }
    }

  }

#if (dbg_numnodes & dbg_flags)	// because it isn't declared otherwise
  DEBUG(numnodes) ValidateNumKRONodes();
#endif

  // Scan all pages.
  for (objNum = 0; objNum < objC_nPages; objNum++) {
    PageHeader * pageH = objC_GetCorePageFrame(objNum);

    switch (pageH_GetObType(pageH)) {
    default:
      break;

    case ot_PtTagPot:
    case ot_PtHomePot:
      assertex(pageH, ! objH_IsKRO(pageH_ToObj(pageH)));

      if (pageH_IsDirty(pageH)) {
        assert(!"complete");	// figure this out later
      }
      break;

    case ot_PtDataPage:
      if (! OIDIsPersistent(pageH_ToObj(pageH)->oid))
        break;
    case ot_PtLogPot:
      assertex(pageH, ! objH_IsKRO(pageH_ToObj(pageH)));

      CheckpointPage(pageH);
      break;
    }
  }

#if (dbg_numpages & dbg_flags)	// because it isn't declared otherwise
  DEBUG(numpages) ValidateNumKROPages();
#endif

  // Save Activity's to the process directory.
  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    Activity * act = &act_ActivityTable[i];
    if (act->state != act_Free) {
      OID procOid;
      if (! act_GetOID(act, &procOid))
        continue;
      if (OIDIsPersistent(procOid)) {
        uint8_t hazToSave;
        if (act->state == act_Sleeping) {
          assert(act->actHazard == actHaz_None);
          // On restart, wake sleepers with an error.
          hazToSave = actHaz_WakeRestart;
        } else
          hazToSave = act->actHazard;

        DEBUG(procs) printf("Saving proc oid=%#llx\n", procOid);
        // If hazToSave is actHaz_WakeResume, act->u.callCount is correct.
        // If not, act->u.callCount is ignored.
        /* Note: It is possible for this OID to already be in the directory.
        Suppose persistent process P calls nonpersistent process N.
        Now P gets paged out, so N's key is unprepared.
        Before N returns, another process rescinds P.
        Now suppose another process P2 is created that happens to have
        the same OID. 
        In this case, information from P2's Activity should overwrite
        the information we saved from N's stale Resume key to P.

        In another example:
        Suppose persistent process P calls nonpersistent process N.
        Suppose another process Sends a message to the Sleep key,
        passing a copy of the Resume key to P as the key to receive
        the reply.
        In this case, when we get here, N's Resume key has been saved
        with actHaz_WakeResume. Because the Resume key might be stale,
        we overwrite that entry with the data from P2's Sleeping Activity,
        which is actHaz_WakeRestart.
        */
        StoreProcessInfo(procOid, act->u.callCount, hazToSave);
      }
    }
  }
  // Finish the last page.
  *numDpdsLoc = dpdsInCurrentPage;

  ProcDirFramesWritten = &reservedPages;

  DEBUG(ckpt) printf("End phase 1, reservedPages=%#x\n", reservedPages);

  DEBUG(ckpt) check_Consistency("after ckpt P1");

  ckptState = ckpt_Phase2;
}

// Note, dod is not aligned!
static unsigned int
WriteDirEntsToPage(struct DiskObjectDescriptor * dod)
{
  unsigned int numDirEntsInPage = 0;
  while (1) {
    if (! numDirEntsToSave)
      break;		// no more to write
    kva_t roomInPage = (- (kva_t)dod) & EROS_PAGE_MASK;
    if (roomInPage < sizeof(struct DiskObjectDescriptor))
      break;		// no more will fit

    const ObjectDescriptor * od = ld_findNextObject(workingGenerationNumber);
    assert(od);		// else ran out before count ran out
    // dod is unaligned and packed, so use memcpy.
    memcpy(&dod->oid, &od->oid, sizeof(OID));
    memcpy(&dod->allocCount, &od->allocCount, sizeof(ObCount));
    memcpy(&dod->callCount, &od->callCount, sizeof(ObCount));
    memcpy(&dod->logLoc, &od->logLoc, sizeof(LID));
    memcpy(&dod->type, &od->type, sizeof(uint8_t));

    dod++;
    numDirEntsInPage++;
    numDirEntsToSave--;
  }
  return numDirEntsInPage;
}

static void
DoPhase2Work(void)
{
  // FIXME! Since the checkpoint process has high priority,
  // it will always get IORequest blocks before users,
  // so "clean me first" won't work!

  // Write the process directory frames.
  while (ProcDirFramesWritten != nextProcDirFramePP) {
    PageHeader * pageH = *ProcDirFramesWritten;
    IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
    DEBUG(ckpt) printf("Writing proc dir frame %#x\n", pageH);
    ioreq->pageH = pageH;	// link page and ioreq
    pageH->ioreq = ioreq;

    ObjectRange * rng = LidToRange(nextProcDirLid);
    assert(rng);	// it had better be mounted

    ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
    ioreq->objRange = rng;
    ioreq->rangeLoc = OIDToFrame(nextProcDirLid - rng->start);
    ioreq->doneFn = &IOReq_EndWrite;
    sq_Init(&ioreq->sq);	// won't be used
    ioreq_Enqueue(ioreq);

    ProcDirFramesWritten = &pageH->kt_u.link.next;
    nextProcDirLid = IncrementLID(nextProcDirLid);
  }

  // Clean nodes before pages, because this will generate node pots.
  while (CleanAKRONode()) {
#if (dbg_numnodes & dbg_flags)	// because it isn't declared otherwise
    DEBUG(numnodes) ValidateNumKRONodes();
#endif
  }

  /* The stages in the life of a typical page are:
  1. Page is dirty
  2. A demarcation event occurs. The page is now dirty and KRO.
  3. The page is queued to be cleaned by the disk driver.
     It is marked KRO and clean. (We always mark a page clean *before*
     cleaning it, so we will know if it is dirtied while being cleaned.)
  4. The cleaning finishes. The page is now clean and not KRO. */
  while (numKRODirtyPages) {
    CleanAKROPage();
#if (dbg_numpages & dbg_flags)	// because it isn't declared otherwise
    DEBUG(numpages) ValidateNumKROPages();
#endif
  }

  // Phase 2 is committed. Nothing below Yields.

  // All the dir ents for the working generation have now been created,
  // even though pages may still be queued for the disk driver.
  numDirEntsToSave = ld_numWorkingEntries();
  ld_resetScan(workingGenerationNumber);

  // Some can go in the generation header:
  struct DiskObjectDescriptor * dod = (struct DiskObjectDescriptor *)
      ((char *)genHdr
       + sizeof(DiskGenerationHdr)
       + genHdr->processDir.nDescriptors
         * sizeof(struct DiskProcessDescriptor) );

  // Write dir ents to the generation header:
  genHdr->objectDir.nDescriptors = WriteDirEntsToPage(dod);
  genHdr->objectDir.firstDirFrame = 0;	// no frame yet
  genHdr->objectDir.nDirFrames = 0;

  nextRangeToSync = 0;
  rangesSynced = 0;

  DEBUG(ckpt) check_Consistency("after ckpt P2");

  ckptState = ckpt_Phase3;
}

static void IOReq_EndObDirWrite(IORequest * ioreq);

static void
WriteAPageOfObDirEnts(PageHeader * pageH, IORequest * ioreq)
{
  // Fill the page with dir ents:
  uint32_t * objDirFrame = (uint32_t *)pageH_GetPageVAddr(pageH);
  struct DiskObjectDescriptor * dod
    = (struct DiskObjectDescriptor *)(objDirFrame + 1);
  *objDirFrame = WriteDirEntsToPage(dod);

  // Write it to disk:
  DEBUG(ckpt) printf("Writing ob dir frame %#x\n", pageH);
  ioreq->pageH = pageH;	// link page and ioreq
  pageH->ioreq = ioreq;

  // There is no cleaning going on, so the LIDs we allocate here
  // will be consecutive.
  LID lid = NextLogLoc();
  if (genHdr->objectDir.firstDirFrame == 0)
    genHdr->objectDir.firstDirFrame = lid;
  genHdr->objectDir.nDirFrames++;

  ObjectRange * rng = LidToRange(lid);
  assert(rng);	// it had better be mounted

  ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(lid - rng->start);
  ioreq->doneFn = &IOReq_EndObDirWrite;
  sq_Init(&ioreq->sq);	// won't be used
  ioreq_Enqueue(ioreq);
}

static void
IOReq_EndObDirWrite(IORequest * ioreq)
{
  // The IORequest is done.
  assert(sq_IsEmpty(&ioreq->sq));
  PageHeader * pageH = ioreq->pageH;

  // Mark the page as no longer having I/O.
  pageH->ioreq = NULL;

  if (numDirEntsToSave) {
    // Use this page and ioreq to write another page of dir ents.
    WriteAPageOfObDirEnts(pageH, ioreq);
    if (! numDirEntsToSave)	// we have now written them all
      sq_WakeAll(&WaitForObjDirWritten);
  } else {
    IOReq_Deallocate(ioreq);
    ReleasePageFrame(pageH);
  }
}

static void
IOReq_EndGenHdrWrite(IORequest * ioreq)
{
  DEBUG(ckpt) printf("EndGenHdrWrite\n");

  // The IORequest is done.
  sq_WakeAll(&ioreq->sq);

  // Hang on to the page and ioreq; they will be used for the checkpoint root.
}

static void
DoPhase3Work(void)
{
  DEBUG(ckpt) printf("Ckpt phase 3 reservedPages=%#x\n",
                     reservedPages);

  /* Pages for the directory were reserved earlier.
  Now would be a bad time to reserve pages, because nothing can be cleaned. */
  // Now put all the reserved pages to work as object directory frames:
  while (reservedPages) {
    PageHeader * pageH = reservedPages;
    DEBUG(ckpt) printf("Using obDir pageH %#x\n", pageH);
    // Unlink it:
    reservedPages = pageH->kt_u.link.next;
    numReservedPages--;
    if (numDirEntsToSave) {
      IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield
      WriteAPageOfObDirEnts(pageH, ioreq);
    } else {
      ReleasePageFrame(pageH);
    }
  }

  DEBUG(ckpt) printf("Ckpt phase 3 numDirEntsToSave=%d\n",
                     numDirEntsToSave);

  if (numDirEntsToSave) {
    SleepOnPFHQueue(&WaitForObjDirWritten);
  }

  // Verify that there are no more dir ents:
  assert(ld_findNextObject(workingGenerationNumber) == NULL);

  IORequest * ioreq = IOReqCleaning_AllocateOrWait();	// may Yield

  // Phase 3 is committed. Nothing below Yields.

  // Allocate a frame in the log for the generation header:
  thisGenHdrLid = NextLogLoc();

  // Calculate next wkgUGHL:
  if ((nextWkgUGHL = wkgUGHL) == 0)
    nextWkgUGHL = MaxUnmigratedGenerations + 1;
  nextWkgUGHL--;
  unmigratedGenHdrLid[nextWkgUGHL] = thisGenHdrLid;

  // Initialize the rest of the generation header:
  genHdr->versionNumber = 1;
  genHdr->generationNumber = workingGenerationNumber;
  genHdr->firstLid = workingGenFirstLid;
  genHdr->lastLid = thisGenHdrLid;

  // Write the generation header:
  PageHeader * pageH = GenHdrPageH;
  DEBUG(ckpt) printf("Writing gen dir frame %#x\n", pageH);
  ioreq->pageH = pageH;	// link page and ioreq
  pageH->ioreq = ioreq;

  ObjectRange * rng = LidToRange(thisGenHdrLid);
  assert(rng);	// it had better be mounted

  ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(thisGenHdrLid - rng->start);
  ioreq->doneFn = &IOReq_EndGenHdrWrite;
  sq_Init(&ioreq->sq);
  ioreq_Enqueue(ioreq);

  ckptState = ckpt_Phase4;

  // Wait for generation header to be written:
  SleepOnPFHQueue(&ioreq->sq);
}

static void
IOReq_EndCkptRootWrite(IORequest * ioreq)
{
  DEBUG(ckpt) printf("EndCkptRootWrite\n");

  // The IORequest is done.
  sq_WakeAll(&ioreq->sq);
  PageHeader * pageH = ioreq->pageH;

  // Mark the page as no longer having I/O.
  pageH->ioreq = NULL;

  IOReq_Deallocate(ioreq);
  assert(pageH == GenHdrPageH);
  ReleasePageFrame(pageH);
  GenHdrPageH = NULL;
  genHdr = NULL;	// for safety
}

static void
DoPhase4Work(void)
{
  DoSync();

  // Phase 4 is committed. Nothing below Yields.

  // Reuse the generation header page for the checkpoint root.
  CkptRoot * ckroot
    = (CkptRoot *)pageH_GetPageVAddr(GenHdrPageH);
  // Initialize the checkpoint root:
  ckroot->versionNumber = CkptRootVersion;
  ckroot->maxNPCount = maxNPCount;
  ckroot->checkGenNum
    = ckroot->mostRecentGenerationNumber = workingGenerationNumber;
  ckroot->endLog = logWrapPoint;
  ckroot->integrityByte = IntegrityByteValue;

  DEBUG(ckpt) printf("Ckpt phase 4 wkgGen=%d migrGen=%d\n",
                     workingGenerationNumber, migratedGeneration);

  // Number of unmigrated generations including the working generation:
  unsigned int numUnmigrGens = workingGenerationNumber - migratedGeneration;
  ckroot->numUnmigratedGenerations = numUnmigrGens;
  nextRetiredGeneration = migratedGeneration;

  unsigned int lidCursor = nextWkgUGHL;
  int i;
  for (i = 0; i < numUnmigrGens + 1; i++) {
    ckroot->generations[i] = unmigratedGenHdrLid[lidCursor];
    lidCursor = WrapCircularIndex(lidCursor + 1, MaxUnmigratedGenerations+1);
  }
  // For safety, fill the rest of the array:
  for (; i < MaxUnmigratedGenerations; i++)
    ckroot->generations[i] = UNUSED_LID;

  // Switch root locations:
  currentRootLID ^= (CKPT_ROOT_0 ^ CKPT_ROOT_1);

  // Write the checkpoint root:
  PageHeader * pageH = GenHdrPageH;
  DEBUG(ckpt) printf("Writing ck root frame %#x to LID %#llx\n",
                     pageH, currentRootLID);
  // page and ioreq are already linked
  IORequest * ioreq = pageH->ioreq;

  ObjectRange * rng = LidToRange(currentRootLID);
  assert(rng);	// it had better be mounted

  ioreq->requestCode = capros_IOReqQ_RequestType_writeRangeLoc;
  ioreq->objRange = rng;
  ioreq->rangeLoc = OIDToFrame(currentRootLID- rng->start);
  ioreq->doneFn = &IOReq_EndCkptRootWrite;
  sq_Init(&ioreq->sq);	// won't be used
  ioreq_Enqueue(ioreq);

  nextRangeToSync = 0;
  rangesSynced = 0;

  ckptState = ckpt_Phase5;

  // Wait for checkpoint root to be written:
  SleepOnPFHQueue(&ioreq->sq);
}

static void
DoPhase5Work(void)
{
  DEBUG(ckpt) printf("Ckpt phase 5 nextRangeToSync=%d nLidRanges=%d\n",
                     nextRangeToSync, nLidRanges);

  // Wait for checkpoint root to be written:
  DoSync();

  // Phase 5 is committed. Nothing below Yields.
  // The checkpoint is stabilized.

  assert(numReservedPages == 0);
  assert(reservedPages == NULL);

  // Advance wkgUGHL:
  wkgUGHL = nextWkgUGHL;

  PostCheckpointProcessing();

  ckptState = ckpt_NotActive;
  sq_WakeAll(&WaitForCkptInactive);
}

void
DoCheckpointStep(void)
{
  switch (ckptState) {
  default: ;
    assert(false);

notActive:
  case ckpt_NotActive:
    DEBUG(ckpt) printf("DoCheckpointStep not active\n");
    act_SleepOn(&WaitForCkptNeeded);
    act_Yield();
    
  case ckpt_Phase1:
    DEBUG(ckpt) printf("DoCheckpointStep P1\n");
    DoPhase1Work();
    DEBUG(ckpt) printf("DoCheckpointStep begin P2\n");
  case ckpt_Phase2:
    DoPhase2Work();
  case ckpt_Phase3:
    DEBUG(ckpt) printf("DoCheckpointStep P3\n");
    DoPhase3Work();
  case ckpt_Phase4:
    DEBUG(ckpt) printf("DoCheckpointStep P4\n");
    DoPhase4Work();
  case ckpt_Phase5:
    DEBUG(ckpt) printf("DoCheckpointStep P5\n");
    DoPhase5Work();
    goto notActive;
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
