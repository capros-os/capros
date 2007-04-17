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

/* Machine-dependent data that go in the Process structure. */
typedef struct ProcMD {

/*
The possibilities for the process address space map are:
(1) If MappingTable == PTE_ZAPPED, the process has no map. 
(2) If MappingTable == PTE_IN_PROGRESS, the process has no map.
(3) If MappingTable == KernPageDir_pa and smallPTE == 0,
    the process has no map, limit == UMSGTOP, and bias == 0.
(4) If MappingTable == KernPageDir_pa and smallPTE != 0,
    the process has a small space,
    smallPTE is &proc_smallSpaces[SMALL_SPACE_PAGES * ndx],
    limit is SMALL_SPACE_PAGES * EROS_PAGE_SIZE,
    and bias is UMSGTOP + (ndx * SMALL_SPACE_PAGES * EROS_PAGE_SIZE),
    where ndx is the index of the process in proc_ContextCache.
(5) If MappingTable == none of the above,
    the process has a large space,
    smallPTE == 0, limit == UMSGTOP, and bias == 0.
 */
  kpmap_t MappingTable;
  kva_t cpuStack;
#ifdef OPTION_SMALL_SPACES
  uva_t limit;
  ula_t bias;
  struct PTE * smallPTE;
#endif
} ProcMD;

#endif // __MACHINE_PROCESS_H__
