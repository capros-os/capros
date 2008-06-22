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
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/IORQ.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/PhysMem.h>
#include <disk/CkptRoot.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <kerninc/Key-inline.h>

#include <idl/capros/key.h>
#include <idl/capros/MigratorTool.h>
#include <eros/machine/IORQ.h>

#define dbg_restart	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_restart )

#define DEBUG(x) if (dbg_##x & dbg_flags)

GenNum workingGenerationNumber;
LID logCursor = 0;	// next place to write in the main log
LID currentRoot;	// CKPT_ROOT_0 or CKPT_ROOT_1
Activity * migratorActivity;

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
  return ioreq->pageH;
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

// temporary stuff:
DEFQUEUE(migratorQueue);
static void
DoMigrationStep(void)
{
  DEBUG(restart) printf("DoMigrStep %d\n", restartPhase);
  act_SleepOn(&migratorQueue);	// sleep forever (until implemented)
  act_Yield();	// act_Yield does not return.
}

static void
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

    if (curRoot->maxNPAllocCount) {
      // Adjust allocation and call counts of non-persistent objects.
      // This ensures that capabilities to them in persistent objects
      // will be rescinded.
      assert(false); //// incomplete
    }

    workingGenerationNumber = curRoot->mostRecentGenerationNumber + 1;
    if (curRoot->mostRecentGenerationNumber == 0) {
      // No checkpoint has been taken.
      logCursor = MAIN_LOG_START;
      // Is the amount of mounted log reasonable?
      if (OIDToFrame(logWrapPoint) < physMem_TotalPhysicalPages) {
        assert(!"implemented");	// FIXME wait for more log to be mounted
      }
      break;	// restart is done!
    }
    assert(false); //// incomplete

    break;
  }
}

/* May Yield. */
void
MigratorToolKey(Invocation* inv)
{
  inv_GetReturnee(inv);

  inv->exit.code = RC_OK;	// Until proven otherwise

  switch (inv->entry.code) {
  case OC_capros_key_getType:
    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = IKT_capros_MigratorTool;
    break;

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;

  case OC_capros_MigratorTool_restartStep:
    DoRestartStep();
    COMMIT_POINT();
    break;

  case OC_capros_MigratorTool_migrationStep:
    DoMigrationStep();
    COMMIT_POINT();
    break;
  }
  ReturnMessage(inv);
}

#define StackSize 256
#define KR_MigrTool 7

/* Most of the work of restart and migration is done in the kernel.
 * This thread just drives the execution.
 */
/* Note, beware of accessing a page both from this process and from
 * the kernel. On some architectures (ARM), that can result in
 * an incoherent cache. */
void
MigratorStart(void)
{
  DEBUG(restart) printf("Start Migrator thread, act=%#x\n", migratorActivity);

  // The migrator thread begins by performing a restart.

  Message Msg = {
    .snd_invKey = KR_MigrTool,
    .snd_code = OC_capros_MigratorTool_restartStep,
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
  CALL(&Msg);

  DEBUG(restart) printf("Finished restart.\n");

  // Start migration
  for (;;) {
    Msg.snd_code = OC_capros_MigratorTool_migrationStep;
    CALL(&Msg);
  }
}

void
CreateMigratorActivity(void)
{
  fixreg_t * stack = MALLOC(fixreg_t, StackSize);

  migratorActivity = kact_InitKernActivity("Migr", pr_Normal,
    dispatchQueues[pr_Normal], &MigratorStart,
    stack, &stack[StackSize]);
  migratorActivity->readyQ = dispatchQueues[pr_Normal];

  // Endow it with the migrator tool.
  Key * k = & migratorActivity->context->keyReg[KR_MigrTool];
  keyBits_InitType(k, KKT_MigratorTool);
}

