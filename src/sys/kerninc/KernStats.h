#ifndef __KERNINC_KERNSTATS_H__
#define __KERNINC_KERNSTATS_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
 *
 * This file is part of the EROS Operating System.
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

#ifdef OPTION_KERN_STATS

struct KernStats_s {
  uint64_t nDepend;
  uint64_t nDepMerge;
  uint64_t nDepInval;
  uint64_t nDepZap;

  uint64_t nInvoke;
  uint64_t nInvKpr;
  uint64_t nInter;
  uint64_t nKeyPrep;

  uint64_t nWalkSeg;
  uint64_t nWalkLoop;

  uint64_t nPfTraps;
  uint64_t nPfAccess;

  uint64_t nGateJmp;
  uint64_t nInvRetry;
  uint64_t bytesMoved;

  uint64_t nRetag;
};
extern struct KernStats_s KernStats;

#endif

#endif /* __KERNINC_KERNSTATS_H__ */
