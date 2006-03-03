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

/* Machine-dependent data that go in the Process structure. */
typedef struct ProcMD {
  kpmap_t MappingTable;
  kva_t cpuStack;
#ifdef OPTION_SMALL_SPACES
  uva_t limit;
  ula_t bias;
  struct PTE * smallPTE;
#endif
} ProcMD;

#endif // __MACHINE_PROCESS_H__
