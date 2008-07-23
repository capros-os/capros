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

extern long numKROFrames;
extern long numKRONodes;
extern unsigned int KROPageCleanCursor;
extern unsigned int KRONodeCleanCursor;

extern struct Activity * checkpointActivity;

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

unsigned long CalcLogReservation(void);
void DeclareDemarcationEvent(void);
void DoCheckpointStep(void);

#endif /* __CKPT_H__ */
