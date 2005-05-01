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
  seg_KernelCode   = 0x1,
  seg_KernelData   = 0x2,
  seg_DomainTSS    = 0x3,

  seg_DomainCode   = 0x4,
  seg_DomainData   = 0x5,
  seg_DomainPseudo = 0x6,
  
  /* These come last because they can be relocated without impacting
     the user selectors. */
  seg_KProcCode    = 0x7,
  seg_KProcData    = 0x8,
  
#if 0
  ApmCode32        = 0x9,
  ApmCode16        = 0x10,
  ApmData          = 0x11,
#endif
  
  seg_NUM_SEGENTRY
};

typedef enum SegEntryName SegEntryName;

enum SelectorName {
  sel_Null = 0x00,
  sel_KernelCode = 0x08,		/* entry 1, rpl=0 */
  sel_KernelData = 0x10,		/* entry 2, rpl=0 */
  sel_DomainTSS  = 0x18,		/* entry 3, rpl=3 */
  
  sel_DomainCode = 0x23,		/* entry 4, rpl=3 */
  sel_DomainData = 0x2b,		/* entry 5, rpl=3 */
  
  sel_DomainPseudo = 0x33,		/* entry 6, rpl=1 */

  sel_KProcCode  = 0x39,		/* entry 7, rpl=1 */
  sel_KProcData  = 0x41,		/* entry 8, rpl=1 */
  
#if 0
  /* THESE ARE NO GOOD!!! */
  ApmCode32      = 0x48,		/* entry 8, rpl=0 */
  ApmCode16      = 0x40,		/* entry 9, rpl=0 */
  ApmData        = 0x58,		/* entry 10, rpl=0 */
#endif
  
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
