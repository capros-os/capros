#ifndef __CKPT_H__
#define __CKPT_H__
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

#include <kerninc/LogDirectory.h>
#include <kerninc/IORQ.h>

enum {
  ckpt_NotActive = 0,
  ckpt_Phase1,
  ckpt_Phase2,
  ckpt_Phase3,
  ckpt_Phase4,
  ckpt_Phase5,
};
extern unsigned int ckptState;

/* monotonicTimeOfRestart is the time of the demarcation event
 * from which we restarted, in units of nanoseconds. */
extern uint64_t monotonicTimeOfRestart;
extern uint64_t monotonicTimeOfLastDemarc;

extern struct StallQueue WaitForCkptInactive;
extern struct StallQueue WaitForCkptNeeded;
extern struct StallQueue RestartQueue;

extern long numKRODirtyPages;
extern long numKRONodes;

extern struct Activity * checkpointActivity;

extern LID logCursor;
extern LID logWrapPoint;
extern LID currentRootLID;

extern LID unmigratedGenHdrLid[];
extern LID oldestNonRetiredGenLid;

/* workingGenFirstLid is the LID of the first frame of the
 * working generation. */
extern LID workingGenFirstLid;

#define LOG_LIMIT_PERCENT_DENOMINATOR 256
extern frame_t logSizeLimited;

extern GenNum migratedGeneration;
extern GenNum retiredGeneration;

extern bool IsPreloadedBigBang;
extern OID PersistentIPLOID;

#define KR_MigrTool 7

extern PageHeader * GenHdrPageH;
extern struct DiskGenerationHdr * genHdr;	// virtual address of the above

extern PageHeader * reservedPages;
extern unsigned int numReservedPages;
void ReservePages(unsigned int numPagesWanted);

extern unsigned int KROPageCleanCursor;
extern unsigned int KRONodeCleanCursor;

extern GenNum nextRetiredGeneration;

enum {
  restartPhase_Begin,	// waiting for LIDs 0 and 1 to be mounted
  restartPhase_QueuingRoot1,
  restartPhase_WaitingRoot1,
  restartPhase_Phase4,
  restartPhase_Done
};
extern unsigned int restartPhase;

LID GetOldestNonNextRetiredGenLid(void);

INLINE bool
ckptIsActive(void)
{
  return ckptState;
}

INLINE bool
restartIsDone(void)
{
  return restartPhase == restartPhase_Done;
}

INLINE void
WaitForRestartDone(void)
{
  if (! restartIsDone())
    SleepOnPFHQueue(&RestartQueue);
}

/* GetNextRetiredGeneration should be called only while a checkpoint is active.
 * It returns the generation number of the newest generation for which
 * it is known that it will be retired after the current checkpoint
 * is stabilized. */
INLINE GenNum
GetNextRetiredGeneration(void)
{
  assert(ckptIsActive() || ! restartIsDone());
  if (nextRetiredGeneration)
    /* It is possible that a generation will be migrated between the time
    the generation header is fixed and the time it has been written to disk,
    in which case we want the value in the header: */
    return nextRetiredGeneration;
  else
    /* It is possible that another generation will be migrated before
    the checkpoint is stabilized, but this is the best we can be sure of: */
    return migratedGeneration;
}

unsigned long CalcLogReservation(unsigned long numDirtyObjects[],
  unsigned long existingLogEntries);
void DeclareDemarcationEvent(void);
void DoCheckpointStep(void);

void PostCheckpointProcessing(void);

#endif /* __CKPT_H__ */
