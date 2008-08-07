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
#include <disk/CkptRoot.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/Activity.h>
#include <kerninc/IORQ.h>
#include <kerninc/Ckpt.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/Key-inline.h>
#include <eros/machine/IORQ.h>

extern Activity * migratorActivity;

#define dbg_restart	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_restart )

#define DEBUG(x) if (dbg_##x & dbg_flags)

uint64_t monotonicTimeOfRestart;

DEFQUEUE(RestartQueue);	// waiting for restart to finish

// Restart state:

LID lidNeeded = 0;

// We can be operating with up to two pages:
PageHeader * pageA;
PageHeader * pageB;
CkptRoot * curRoot;

enum {
  restartPhase_Begin,	// waiting for LIDs 0 and 1 to be mounted
  restartPhase_QueuingRoot1,
  restartPhase_WaitingRoot1,
};
unsigned int restartPhase = restartPhase_Begin;

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
  if (root->versionNumber != CkptRootVersion)
    return false;
  if (root->mostRecentGenerationNumber != root->checkGenNum)
    return false;
  if (root->integrityByte != IntegrityByteValue)
    return false;
  return true;
}

void
DoRestartStep(void)
{
  DEBUG(restart) printf("DoRestartStep %d\n", restartPhase);

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
        curRoot = root0->mostRecentGenerationNumber
                  > root1->mostRecentGenerationNumber
                  ? root0 : root1;
      }
    }
    if (curRoot == root0) {
      currentRoot = CKPT_ROOT_0;
      ReleasePageFrame(pageB);	// Free the old root.
    } else {
      currentRoot = CKPT_ROOT_1;
      ReleasePageFrame(pageA);
    }

    // Adjust allocation and call counts of non-persistent objects.
    assert(restartNPAllocCount == 0);	// not updated yet
    ObCount newRestartCount = curRoot->maxNPAllocCount + 1;
    // The +1 above ensures that capabilities to non-persistent objects
    // in persistent objects will be rescinded.

    // restartNPAllocCount ensures that any new NP objects will
    // have the correct count:
    restartNPAllocCount = newRestartCount;

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
        pNode->callCount += newRestartCount;

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
      }
    }

    workingGenerationNumber = curRoot->mostRecentGenerationNumber + 1;
    if (true || //// until we get restart working
curRoot->mostRecentGenerationNumber == 0) {
      // No checkpoint has been taken.
      // Is the amount of mounted log reasonable?
      if (OIDToFrame(logWrapPoint) < physMem_TotalPhysicalPages) {
        assert(!"implemented");	// FIXME wait for more log to be mounted
      }
      monotonicTimeOfRestart = 0;
      monotonicTimeOfLastDemarc = 0;
      logCursor = MAIN_LOG_START;
      workingGenFirstLid = MAIN_LOG_START;
      oldestNonRetiredGenLid = MAIN_LOG_START;
      logSizeLimited = (OIDToFrame(logWrapPoint - MAIN_LOG_START)
                        * KTUNE_LOG_LIMIT_PERCENT_NUMERATOR)
                       / LOG_LIMIT_PERCENT_DENOMINATOR;
      break;	// restart is done!
    }
    assert(false); //// incomplete

    // Set logCursor last.
    break;
  }
  // Restart is done and the migrator is initialized (it has allocated
  // all the space it needs).
  act_Wakeup(checkpointActivity);
  sq_WakeAll(&RestartQueue, false);
}
