#ifndef __MACHINE_PAGEHEADER_H__
#define __MACHINE_PAGEHEADER_H__
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

/* Machine-dependent data for the ObjectHeader. */ 

#define MD_PAGE_OBTYPES \
  , ot_PtMappingPage2	// second-level page table

#define MD_PAGE_OBNAMES \
  , "PtMapPage2"

#define MD_PAGE_VARIANTS \
  struct MapTabsVariant mp;

typedef struct MapTabHeader MapTabHeader;
struct MapTabHeader {
/* The fields next and producer are required by the machine-independent code. */
  MapTabHeader * next;	/* next product of this producer,
			   or next in free list */
  ObjectHeader * producer;

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
  ula_t tableCacheAddr;
};

struct MapTabsVariant {
  /* N.B.: obType must be the first item in this structure.
     This puts it in the same location as PageHeader.kt_u.ob.obType. */
  uint8_t obType;		/* only ot_PtMappingPage2 */

  MapTabHeader hdrs[4];
    /* For a First Level page table, only hdrs[0] is used, and
       hdrs[0].tableSize == 1.
       For Second Level page tables, hdrs[i].tableSize == 0 
       for all 0 <= i <= 3. */
    /* First level page tables are not fully implemented. */
};

#endif // __MACHINE_PAGEHEADER_H__
