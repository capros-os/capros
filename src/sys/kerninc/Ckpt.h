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

enum {
  ckpt_NotActive = 0,
  ckpt_Phase1,
  ckpt_Phase2,
  ckpt_Phase3,
};
extern unsigned int ckptState;

/* monotonicTimeOfRestart is the time of the demarcation event
 * from which we restarted, in units of nanoseconds. */
extern uint64_t monotonicTimeOfRestart;

/* monotonicTimeOfLastDemarc is the time of the most recent demarcation event,
 * in units of nanoseconds.
 * That checkpoint may not be stabilized yet. */
extern uint64_t monotonicTimeOfLastDemarc;

extern struct StallQueue WaitForCkptInactive;
extern struct StallQueue WaitForCkptNeeded;

extern long numKRODirtyPages;
extern long numKRONodes;
extern unsigned int KROPageCleanCursor;
extern unsigned int KRONodeCleanCursor;

extern struct Activity * checkpointActivity;

extern LID logCursor;	// next place to write in the main log
extern LID logWrapPoint;	// end of main log

/* oldestNonRetiredGenLid is the LID following the last LID of the
 * newest retired generation. */
extern LID oldestNonRetiredGenLid;

/* oldestNonNextRetiredGenLid is valid only while a checkpoint is active.
 * It is the LID following the last LID of the generation
 * returned by GetNextRetiredGeneration(). */
extern LID oldestNonNextRetiredGenLid;

/* workingGenFirstLid is the LID of the first frame of the
 * working generation. */
extern LID workingGenFirstLid;

#define LOG_LIMIT_PERCENT_DENOMINATOR 256
/* logSizeLimited is the size of the main log times the limit percent. */
extern frame_t logSizeLimited;

extern GenNum retiredGeneration;

#define KR_MigrTool 7

INLINE bool
ckptIsActive(void)
{
  return ckptState;
}

INLINE bool
restartIsDone(void)
{
  // logCursor is zero until reset sets it to its proper value,
  // which is always nonzero.
  return logCursor != 0;
}

/* GetNextRetiredGeneration should be called only while a checkpoint is active.
 * It returns the generation number of the newest generation for which
 * it is known that it will be retired after the current checkpoint
 * is stabilized. */
INLINE GenNum
GetNextRetiredGeneration(void)
{
  extern GenNum nextRetiredGeneration;
  extern GenNum migratedGeneration;

  assert(ckptIsActive());
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

#endif /* __CKPT_H__ */
