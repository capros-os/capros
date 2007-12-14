#ifndef __PHYSMEM_H__
#define __PHYSMEM_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group
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

#include <kerninc/kernel.h>
#include <kerninc/Key.h>

/* Descriptor for a physical memory allocation constraint. Actually,
 * this needs to be more detailed, because it also needs to describe
 * boundary crossing issues, but at the moment I'm not trying to deal
 * with that.
 * 
 * The current kernel implements only two general memory constraints:
 * word aligned at any address and page aligned at any address. The
 * general alignment and crossing constraint mechanism is currently
 * unimplemented, but the notion of a constraint structure is included
 * in anticipation of future ports that may require it.
 */

/* FIX: bound should be inclusive limit, for representability! */
struct PmemConstraint {
  kpa_t base;
  kpa_t bound;
  unsigned align;	/* must be a power of 2 */
} ;
typedef struct PmemConstraint PmemConstraint;

/* Values for PmemInfo.type */
#define MI_MEMORY     1         /* allocatable */
#define MI_DEVICEMEM  5         /* device memory region */
#define MI_BOOTROM    6         /* System ROM area. */

struct PmemInfo {
  kpa_t     base;
  kpa_t     bound;
  uint32_t  type;		/* address type of this range */
  uint32_t  readOnly;		/* are pages in region read-only? */
  kpa_t     allocBase;		/* how much has kernel allocated? */
  kpa_t     allocBound;		/* how much has kernel allocated? */
#if 0
  uint32_t  flags;		/* flags to be used by kernel */
#endif

  uint32_t  nPages;		/* number of pages allocated to the
				 * object cache */
  uint32_t  basepa;		/* base pa of pages allocated to ob cache*/
  PageHeader * firstObHdr;	/* pgHdrs for those pages */
} ;

typedef struct PmemInfo PmemInfo;

extern PmemConstraint physMem_pages;
extern PmemConstraint physMem_any;

extern PmemInfo *physMem_pmemInfo;
extern unsigned long physMem_nPmemInfo;
#define MAX_PMEMINFO 128

extern kpa_t physMem_PhysicalPageBound;
extern kpsize_t physMem_TotalPhysicalPages;

/* Former member functions of PhysMem */

kpsize_t physMem_MemAvailable(PmemConstraint *, unsigned unitSize);

void physMem_Init();

PmemInfo * physMem_ChooseRegion(kpsize_t sz, PmemConstraint *);
PmemInfo * physMem_AddRegion(kpa_t base, kpa_t bound, uint32_t type, 
			     bool readOnly);

kpa_t physMem_Alloc(kpsize_t sz, PmemConstraint *);
void physMem_ReserveExact(kpa_t base, kpsize_t size);

/* This function is machine specific. It typically lives in the same
 * file that builds the kernel map, as the two must agree about the
 * reserved regions.
 */
void physMem_ReservePhysicalMemory();
  
#ifdef OPTION_DDB
void physMem_ddb_dump();
#endif

kpsize_t PmemInfo_ContiguousPages(const PmemInfo * pmi);

INLINE kpsize_t 
physMem_AvailBytes(PmemConstraint *mc)
{ 
  return physMem_MemAvailable(mc, sizeof(uint8_t)); 
}

INLINE kpsize_t 
physMem_AvailPages(PmemConstraint *mc)
{ 
  return physMem_MemAvailable(mc, EROS_PAGE_SIZE); 
}

#endif /* __PHYSMEM_H__ */
