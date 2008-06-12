#ifndef __CKPTROOT_H__
#define __CKPTROOT_H__
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

#include <eros/target.h>
#include <disk/ErosTypes.h>

#define MaxUnmigratedGenerations 240
#define CkptRootVersion 1

/* The CkptRoot is stored in LID 0 and 1.
 * The more recent of the two is the stabilized checkpoint. */

typedef struct CkptRoot {
  // Beware of alignment: this structure is used on both the host and target.

  uint32_t versionNumber;

  ObCount maxNPAllocCount;

  uint64_t mostRecentGenerationNumber;

  // The last LID in the log + 1.
  // This tells us when the entire log is mounted.
  // Is this needed?
  LID endLog;

  // The LIDs of the headers of the unmigrated generations,
  // in order beginning with the most recent:
  LID generations[MaxUnmigratedGenerations];	// 0 if not used
} CkptRoot;

#endif // __CKPTROOT_H__
