#ifndef __MACHINE_PAGEHEADER_H__
#define __MACHINE_PAGEHEADER_H__
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Machine-dependent data for the ObjectHeader. */ 

#define MD_PAGE_OBTYPES \
  , ot_PtMappingPage2	// second-level page table

#define MD_PAGE_OBNAMES \
  , "PtMapPage2"

#define MD_PAGE_VARIANTS \
  struct MapTabsVariant mp;

#define MD_PAGE_OBFIELDS \
  ula_t cacheAddr;
/* cacheAddr applies only to pages with obType ==
   ot_PtDataPage or ot_PtDevicePage. */

#define CACHEADDR_WRITEABLE 1
#define CACHEADDR_NONE 2
#define CACHEADDR_READERS 3
#define CACHEADDR_UNCACHED 4
/* To support cache coherency, a data page is in one of the following states.
MVA means modified virtual address. A page's MVA may be zero.

1. Not mapped at any MVA. cacheAddr has CACHEADDR_NONE.
2. Mapped readonly at one MVA. cacheAddr has the MVA (low bit is zero).
3. Mapped writeable at one MVA. cacheAddr has the MVA, with the
   low bit set to CACHEADDR_WRITEABLE.
4. Mapped readonly at multiple MVAs. cacheAddr has CACHEADDR_READERS.
5. Mapped writeable at some MVA, and also mapped at a different MVA.
   cacheAddr has CACHEADDR_UNCACHED.
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
/* The fields next and producer are required by the machine-independent code. */
  MapTabHeader * next;	/* next product of this producer,
			   or next in free list */
  ObjectHeader * producer;

  /* If tableCacheAddr is odd, this mapping table (which must be a second
     level page table) may be mapped at various modified virtual addresses.
     Therefore for cache coherency its PTEs must not grant write access. 
     (The data may be logically writeable, but if written, a new maptab
     must be used, and the cache flushed.)

     If tableCacheAddr is even, this mapping table is mapped only at
     that MVA, and cache entries dependent on this mapping table have
     only that MVA. */
  // FIXME: implement this
  ula_t tableCacheAddr;

  struct Node * redSeg;	/* pointer to slot of keeper that
			 * dominated this mapping frame */
  unsigned char redSpanBlss;	/* blss of seg spanned by redSeg */
  bool wrapperProducer;
  uint8_t producerBlss; /* biased lss of map tbl producer.
			   NOTE: not the key, the object. */
  uint8_t rwProduct    : 1;	/* indicates mapping page is RW version */
  uint8_t caProduct    : 1;	/* indicates mapping page is callable version */
  uint8_t tableSize    : 1;	/* 1 for first level table, 0 for page table */
  uint8_t isFree       : 1;
  uint8_t producerNdx  : 4;
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
