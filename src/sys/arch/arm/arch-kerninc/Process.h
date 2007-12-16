#ifndef __MACHINE_PROCESS_H__
#define __MACHINE_PROCESS_H__
/*
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

struct ObjectHeader;

/* Machine-dependent data that go in the Process structure. */

/*
This table shows all the possible states of the ProcMD data.

  flmt     FLMT     dacr
Producer    *1       *2       pid       Meaning
  NULL  FLPT_NullPA   0        0        Producer of the space is not known *8
  NULL  FLPT_NullPA   0 PID_IN_PROGRESS *3, *8
  !=0   FLPT_NullPA   0        0        Producer known, maptab not known *4, *8
  !=0   FLPT_FCSEPA   *5      !=0       Producer known, small space *7
  !=0   full maptab   0        0        Full space, tracking LRU *6, *7
  !=0   full maptab  0, 1      0        Full space *7

Notes:

*1 firstLevelMappingTable must always be a valid table
   so the kernel will be mapped when the process faults.

*2 The DACR gives client access to some domains and no access to all others.
   This column shows the domains that have client access.
   Domain 0 always has client access, so the kernel can access kernel memory.

*3 This case is analogous to PTE_IN_PROGRESS. See comments for that.

*4 Is this case of any use, without flags like wi.canWrite?

*5 dacr gives client access to domain 0 and, if the small space has a domain
   assigned, that domain.

*6 Access to domain 1 is temporarily withheld because we are tracking
   whether the FLPT is recently used.
   This provides a mechanism to efficiently restore the access. 

*7 There may be cache entries dependent on this ProcMD.

*8 The FLPT cannot be FLPT_FCSE here, even though the dacr gives access only
   to domain 0, because a user process that is privileged
   (running in System mode) might inadvertently access another process's
   small space instead of faulting to load its own. 
   (Note, we do not currently have privileged user processes,
   only kernel processes. Kernel processes access kernel memory only.) 
*/

typedef struct ProcMD {
  struct ObjectHeader * flmtProducer;
  /* The following are cached from the above: */
  kpa_t firstLevelMappingTable;
  uint32_t pid;		/* Process ID in high 7 bits */
  uint32_t dacr;	/* domain access control register */
} ProcMD;

#endif /* __MACHINE_PROCESS_H__ */
