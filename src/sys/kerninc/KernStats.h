#ifndef __KERNINC_KERNSTATS_H__
#define __KERNINC_KERNSTATS_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#ifdef OPTION_KERN_STATS
#include <arch-kerninc/KernStats.h>

struct KernStats_s {
  uint64_t nDepend;
  uint64_t nDepMerge;
  uint64_t nDepInval;
  uint64_t nDepMakeRO;
  uint64_t nDepTrackRef;
  uint64_t nDepTrackDirty;
  uint64_t nDepZap;

  uint64_t nInvoke;	// number of key invocations started
  uint64_t nInvKpr;
  uint64_t nInter;	// number of interrupts
  uint64_t nKeyPrep;

  uint64_t nWalkSeg;
  uint64_t nWalkLoop;

  uint64_t nPfTraps;
  uint64_t nPfAccess;

  uint64_t nGateJmp;
  uint64_t nInvRetry;
  uint64_t bytesMoved;

  MD_KERN_STATS_FIELDS
};
extern struct KernStats_s KernStats;

void KernStats_PrintMD(void);

#endif

#endif /* __KERNINC_KERNSTATS_H__ */
