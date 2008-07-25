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

#include <kerninc/kernel.h>
#include <disk/DiskNode.h>
#include <disk/GenerationHdr.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/IRQ.h>
#include <kerninc/Ckpt.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/Check.h>
#include <kerninc/ObjH-inline.h>
#include <idl/capros/Range.h>
#include <idl/capros/MigratorTool.h>
#include <eros/Invoke.h>

#define dbg_ckpt	0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_ckpt )

#define DEBUG(x) if (dbg_##x & dbg_flags)

unsigned int ckptState = ckpt_NotActive;

long numKROPages = 0;	// including pots
long numKRONodes = 0;
unsigned int KROPageCleanCursor;
unsigned int KRONodeCleanCursor;

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
    numKROPages++;
    objH_SetFlags(pObj, OFLG_KRO);
  }
  // else it's not marked dirty, in which case it isn't mapped with write
  // access, because that's how we track whether it becomes dirty.
}

// DoPhase1Work does NOT Yield. It must be atomic.
static void
DoPhase1Work(void)
{
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

  monotonicTimeOfLastDemarc = sysT_NowPersistent();

  ckptState = ckpt_Phase2;
}

static void
DoPhase2Work(void)
{
  // FIXME! Since the checkpoint process has high priority,
  // it will always get IORequest blocks before users,
  // so "clean me first" won't work!
  while (numKRONodes)
    CleanAKRONode();

  while (numKROPages)
    CleanAKROPage();

  assert(!"complete");
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
