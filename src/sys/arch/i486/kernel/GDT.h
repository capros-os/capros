#ifndef __GDT_H__
#define __GDT_H__
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

#define GDT_ENTRIES seg_NUM_SEGENTRY

extern uint32_t gdt_GdtTable[GDT_ENTRIES * 2];
extern uint32_t gdt_GDTdescriptor[2];


/* Former member functions of GDT */

INLINE void 
gdt_lgdt()
{
#ifdef __ELF__
  __asm__ __volatile__("lgdt gdt_GDTdescriptor"
		       : /* no output */
		       : /* no input */
		       : "memory");
#else
  __asm__ __volatile__("lgdt __3GDT$GDTdescriptor"
		       : /* no output */
		       : /* no input */
		       : "memory");
#endif
}

void gdt_ReloadSegRegs();

void gdt_Init();

void gdt_SetupTSS(SegEntryName, uint32_t base);

void gdt_SetupPageSegment(SegEntryName entry, uva_t vbase, uint32_t nPages);
void gdt_SetupByteSegment(SegEntryName entry, uva_t vbase, uint32_t nBytes);

#ifdef OPTION_DDB
void gdt_ddb_dump();
#endif


#endif /* __GDT_H__ */
