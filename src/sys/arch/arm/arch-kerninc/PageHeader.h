#ifndef __MACHINE_PAGEHEADER_H__
#define __MACHINE_PAGEHEADER_H__
/*
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

/* Machine-dependent data for the ObjectHeader. */ 

#define MD_PAGE_OBTYPES \
  , ot_PtMappingPage1	/* first-level page table (not implemented yet) */ \
  , ot_PtMappingPage2	// second-level page table

#define MD_PAGE_OBNAMES \
  , "PtMapPage2"

#define MD_PAGE_VARIANTS \
  struct MapTabsVariant mp;

#define MD_PAGE_OBFIELDS \
  ula_t cacheAddr;
/* cacheAddr applies only to user-mappable pages; those with obType ==
   ot_PtDataPage, ot_PtDevicePage, ot_PtDMABlock, or ot_PtDMASecondary. */

#define CACHEADDR_ONEREADER 0
#define CACHEADDR_WRITEABLE 1
#define CACHEADDR_NONE 2
#define CACHEADDR_READERS 3
#define CACHEADDR_UNCACHED 4
/* To support cache coherency, a user-mappable page is in
one of the following states.
MVA means modified virtual address. A page's MVA may be zero.

1. Not mapped at any MVA. cacheAddr has CACHEADDR_NONE.
   The cache has no entries for the page.
2. Mapped readonly at one MVA. cacheAddr has the MVA
   (low bit is CACHEADDR_ONEREADER).
   The cache may have clean entries for that MVA.
3. Mapped writeable at one MVA. cacheAddr has the MVA, with the
   low bit set to CACHEADDR_WRITEABLE.
   The cache may have clean or dirty entries for that MVA.
4. Mapped readonly at multiple MVAs. cacheAddr has CACHEADDR_READERS.
   The cache may have clean entries for multiple addresses.
5. Mapped writeable at some MVA, and also mapped at a different MVA.
   cacheAddr has CACHEADDR_UNCACHED.
   The cache has no entries for the page.

A device page is always CACHEADDR_UNCACHED. Think of such pages as being
writeable by the hardware. 

The kernel can read and write pages at the kernel address
PTOV(pageH_GetPhysAddr(pageH)).
These addresses are mapped write-through, so they never have dirty
cache entries.
User-mappable pages (including DMA pages) may have cache entries
at the kernel address, but they are stale.
Pages of type ot_PtFreeFrame, ot_PtSecondary, and ot_PtNewAlloc
may also have stale cache entries at the kernel address,
but they have no other cache entries.
Pages of type ot_PtKernelUse, ot_Pt*Pot, and ot_PtMappingPage*
may have live cache entries at the kernel address,
and no other cache entries.

The kernel addresses TempMap0VA and TempMap1VA are always mapped uncached.
*/

/*
There are three types of MapTabHeaders:

For a coarse (second level) page table:
  tableSize is 0
  ndxInPage tells where the table is in its containing page

For a first level page table (not yet implemented):
  tableSize is 1
  tableCacheAddr is 0
  ndxInPage is unused.

For a small space:
  tableSize is 1
  tableCacheAddr is nonzero and tells which small space this represents.
  ndxInPage is unused.
*/

typedef struct MapTabHeader MapTabHeader;
struct MapTabHeader {
/* The fields next, producer, and backgroundGPT
   are required by the machine-independent code. */
  MapTabHeader * next;	/* next product of this producer,
			   or next in free list */
  ObjectHeader * producer;

  struct Node * backgroundGPT;	/* GPT containing background key
			for this table, 0 if none. */

	/* If this is a first level table, tableCacheAddr == 0.
	If this is a small space, tableCacheAddr == the PID.
	If this is a second level page table and rwProduct == 1,
	tableCacheAddr has the single MVA at which this table may be mapped.
	If this is a second level page table and rwProduct == 0,
	tableCacheAddr is unused. */
  ula_t tableCacheAddr;

  uint8_t mthAge;

  uint8_t readOnly     : 1;
  uint8_t tableSize    : 1;	/* 1 for first level table or small space,
				0 for second level page table */
  uint8_t isFree       : 1;

  /* A mapping table is pinned iff it is used by a page fault handler process.
   * It remains pinned until it is destroyed. */
  uint8_t kernelPin    : 1;

  uint8_t producerNdx  : (EROS_NODE_LGSIZE-1);
  uint8_t ndxInPage    : 2;	/* mp.hdrs[ndxInPage] == this */
};

struct MapTabsVariant {
  /* N.B.: obType must be the first item in this structure.
     This puts it in the same location as PageHeader.kt_u.ob.obType. */
  uint8_t obType;		/* only ot_PtMappingPage2 */

  MapTabHeader hdrs[4];
    /* For a First Level page table, only hdrs[0] is used, and
       hdrs[0].tableSize == 1.
       For Second Level page tables, for all 0 <= i <= 3,
             hdrs[i].tableSize == 0 
         and hdrs[i].ndxInPage == i . */
    /* First level page tables are not fully implemented. */
};

#endif // __MACHINE_PAGEHEADER_H__
