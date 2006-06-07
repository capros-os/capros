#ifndef __MACHINE_PROCESS_H__
#define __MACHINE_PROCESS_H__
/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

struct ObjectHeader;

/* Machine-dependent data that go in the Process structure. */
typedef struct ProcMD {
  /*
     If the top level object of the process's address space is NOT known,
       flmtProducer has NULL,
       firstLevelMappingTable has FLPT_FCSEPA (to map the kernel),
       dacr has domain 0 client, no access to others (to fault user accesses),
       and pid has 0 (to not modify the virtual address).
     If the top level object of the process's address space IS known,
     flmtProducer points to it.
     If the producer's top level table is NOT known,
       firstLevelMappingTable has FLPT_FCSEPA (to map the kernel),
       dacr has domain 0 client, no access to others (to fault user accesses),
       and pid has 0 (to not modify the virtual address).
     If the producer's top level table IS known, and it is a small space,
       firstLevelMappingTable has FLPT_FCSEPA
         (to map the kernel and small spaces),
       and pid has the pid for that small space.
       If the small space has a domain assigned, dacr reflects that, otherwise
       dacr has domain 0 client, no access to others (to fault user accesses),
     If the producer's top level table IS known, and it is a full space,
       firstLevelMappingTable has the physical address of the full space,
       dacr has domain 0 client (for kernel) and domain 1 client (for user),
       no access to others,
       and pid has 0 (to not modify the virtual address).
  */
  struct ObjectHeader * flmtProducer;
  /* The following are cached from the above: */
  kpa_t firstLevelMappingTable;
  uint32_t pid;		/* Process ID in high 7 bits */
  uint32_t dacr;	/* domain access control register */
} ProcMD;

#endif /* __MACHINE_PROCESS_H__ */
