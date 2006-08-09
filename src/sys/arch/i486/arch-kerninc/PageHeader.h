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
  , ot_PtMappingPage

#define MD_PAGE_OBNAMES \
  , "PtMapPage "

#define MD_PAGE_VARIANTS \
  struct MapTabHeader mp;

typedef struct MapTabHeader MapTabHeader;
struct MapTabHeader {
/* N.B.: obType must be the first item in this structure.
   This puts it in the same location as PageHeader.kt_u.ob.obType. */
  uint8_t obType;		/* only ot_PtMappingPage */
    
/* The fields next and producer are required by the machine-independent code. */
  MapTabHeader * next;	/* next product of this producer */
  ObjectHeader * producer;
  struct Node * redSeg;	/* pointer to slot of keeper that
			 * dominated this mapping frame */
  unsigned char redSpanBlss;	/* blss of seg spanned by redSeg */
  bool wrapperProducer;
  uint8_t producerBlss; /* biased lss of map tbl producer.
			   NOTE: not the key, the object. */
  uint8_t rwProduct    : 1;	/* indicates mapping page is RW version */
  uint8_t caProduct    : 1;	/* indicates mapping page is callable version */
  /* The following fields are architecture-dependent,
     but I'm not going to reorganize them now,
     because this whole structure is architecture-dependent.
     A page could have more than one mapping table, or fewer than one. */
  uint8_t tableSize    : 1;	/* 0 for page table, 1 for page directory */
  uint8_t producerNdx  : 4;
  ula_t tableCacheAddr;
};

#endif // __MACHINE_PAGEHEADER_H__
