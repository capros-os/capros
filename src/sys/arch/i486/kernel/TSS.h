#ifndef __TSS_H__
#define __TSS_H__
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

#include "Segment.h"

/* Intel Task State Segment structure - all the zeros are here to
 * conform to the layout of the TSS as specified in the architecture.
 * By conforming, we get to make use of the hardware context switch
 * mechanisms, and it doesn't do any harm...
 */

struct i386TSS {
  uint32_t	backLink;	/* unused */
  uint32_t	esp0;
  uint32_t	ss0;
  uint32_t	esp1;
  uint32_t	ss1;
  uint32_t	esp2;
  uint32_t	ss2;
  uint32_t	cr3;			/* cr3 in 386 manual - what's going on? */
  uint32_t	eip;
  uint32_t	eflags;
  uint32_t	eax;
  uint32_t	ecx;
  uint32_t	edx;
  uint32_t	ebx;
  uint32_t	esp;
  uint32_t	ebp;
  uint32_t	esi;
  uint32_t	edi;
  uint32_t	es;
  uint32_t	cs;
  uint32_t	ss;
  uint32_t	ds;
  uint32_t	fs;
  uint32_t	gs;
  uint32_t	ldtr;		/* local descriptor table segment */
  uint16_t      trapOnSwitch;	/* must be 0 or 1. */
  uint16_t	ioMapBase;	/* should be == sizeof(i386TSS) */
} ;

typedef struct i386TSS i386TSS;

extern i386TSS tss_TaskTable;

/* Former members of TSS */

/* private */
void tss_ltss(SelectorName);

/* public */
void tss_Init();

INLINE void 
tss_SetEntrySP(kva_t sp0)
{
  tss_TaskTable.esp0 = sp0;
}

INLINE void 
tss_SetMappingTable(uint32_t cr3)
{
  tss_TaskTable.cr3 = cr3;
}



#endif /* __TSS_H__ */
