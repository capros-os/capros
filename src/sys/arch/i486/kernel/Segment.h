#ifndef __SEGMENT_H__
#define __SEGMENT_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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


enum SegEntryName {
  seg_Null         = 0,
  seg_KernelCode   = 1,
  seg_KernelData   = 2,
  seg_DomainTSS    = 3,

  seg_DomainCode   = 4,
  seg_DomainData   = 5,
  seg_DomainPseudo = 6,
  
  /* These come last because they can be relocated without impacting
     the user selectors. */
  seg_KProcCode    = 7,
  seg_KProcData    = 8,
  
  seg_NUM_SEGENTRY
};

typedef enum SegEntryName SegEntryName;

#define GDTSelector(index, rpl) (((index)<<3) + (rpl))

enum SelectorName {
  sel_Null = 0x00,
  sel_KernelCode = GDTSelector(seg_KernelCode, 0),
  sel_KernelData = GDTSelector(seg_KernelData, 0),
  sel_DomainTSS  = GDTSelector(seg_DomainTSS, 0),
  
  sel_DomainCode = GDTSelector(seg_DomainCode, 3),
  sel_DomainData = GDTSelector(seg_DomainData, 3),
  sel_DomainPseudo = GDTSelector(seg_DomainPseudo, 3),

  sel_KProcCode  = GDTSelector(seg_KProcCode, 1),
  sel_KProcData  = GDTSelector(seg_KProcData, 1),
  
  /* Descriptor aliases for use in BIOS32: */
  sel_KernelBios32 = sel_KernelCode,
  sel_KProcBios32 = sel_KProcCode,
} ;

typedef enum SelectorName SelectorName;

struct SegDescriptor {
  uint32_t loLimit     : 16;
  uint32_t loBase      : 16;
  uint32_t midBase     : 8;
  uint32_t type        : 4;
  uint32_t system      : 1;
  uint32_t dpl         : 2;
  uint32_t present     : 1;
  uint32_t hiLimit     : 4;
  uint32_t avl         : 1;
  uint32_t zero        : 1;
  uint32_t sz          : 1;
  uint32_t granularity : 1;
  uint32_t hiBase      : 8;
};

typedef struct SegDescriptor SegDescriptor;

struct GateDescriptor {
  uint32_t loOffset : 16;
  uint32_t selector : 16;
  uint32_t zero : 8;
  uint32_t type : 4;
  uint32_t system : 1;
  uint32_t dpl : 2;
  uint32_t present : 1;
  uint32_t hiOffset : 16;
};

typedef struct GateDescriptor GateDescriptor;

#endif /* __SEGMENT_H__ */
