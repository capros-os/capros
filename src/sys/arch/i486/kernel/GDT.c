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

#include <kerninc/kernel.h>
#include "GDT.h"
#include "TSS.h"

/* Make sure this does not end up in BSS by using dummy initializers */
uint32_t gdt_GDTdescriptor[2];


void
gdt_SetupPageSegment(SegEntryName entry, uva_t linearBase, uint32_t nPages)
{
  SegDescriptor* seg /*@ not null @*/ = (SegDescriptor*) &gdt_GdtTable[entry*2];

  seg->loBase = (linearBase & 0xffffu);
  seg->midBase = (linearBase >> 16) & 0xffu;
  seg->hiBase = (linearBase >> 24) & 0xffu;
  seg->loLimit = (nPages & 0xffffu);
  seg->hiLimit = ((nPages >> 16) & 0xfu);
  seg->granularity = 1;
}

void
gdt_SetupByteSegment(SegEntryName entry, uva_t linearBase, uint32_t nBytes)
{
  SegDescriptor* seg /*@ not null @*/ = (SegDescriptor*) &gdt_GdtTable[entry*2];

  seg->loBase = (linearBase & 0xffffu);
  seg->midBase = (linearBase >> 16) & 0xffu;
  seg->hiBase = (linearBase >> 24) & 0xffu;
  seg->loLimit = (nBytes & 0xffffu);
  seg->hiLimit = ((nBytes >> 16) & 0xfu);
  seg->granularity = 0;
}

void
gdt_SetupTSS(SegEntryName entry, uint32_t base)
{
  SegDescriptor* seg /*@ not null @*/ = (SegDescriptor*) &gdt_GdtTable[entry*2];
  uint32_t linearBase;
  
#ifndef NDEBUG
  assert(seg->loLimit == sizeof(i386TSS)-1);
  assert(seg->loBase == 0);
  assert(seg->midBase == 0);
  assert(seg->hiBase == 0);
  assert(seg->type == 0x9);
  assert(seg->system == 0);
  assert(seg->dpl == 0);
  assert(seg->present == 1);
  assert(seg->hiLimit == 0);
  assert(seg->avl == 0);
  assert(seg->zero == 0);
  assert(seg->sz == 0);
  assert(seg->granularity == 0);
#endif
  
  linearBase = KVTOL(base);
  
  seg->loBase = linearBase & 0xffffu;
  seg->midBase = (linearBase >> 16) & 0xffu;
  seg->hiBase = (linearBase >> 24) & 0xffu;
}


#define GDT_SIZE (GDT_ENTRIES * 8)


void
gdt_Init()
{
  uint32_t wgdt;
  uint32_t klimit;

  wgdt = KVTOL(VtoKVA(gdt_GdtTable));
	
  gdt_GDTdescriptor[0] = GDT_SIZE | ((wgdt & 0xffff) << 16);
  gdt_GDTdescriptor[1] = wgdt >> 16;

  klimit = (0u - KVA) >> EROS_PAGE_ADDR_BITS;
  /* Initialize the GDT so that the kernel segment bases point to the
     right places. The table as initialized is correct for the user
     and APM segments. */
  gdt_SetupPageSegment(seg_KernelCode, KVA, klimit );
  gdt_SetupPageSegment(seg_KernelData, KVA, 0xfffff /*4G */);
  gdt_SetupPageSegment(seg_KProcCode,  KVA, klimit );
  gdt_SetupPageSegment(seg_KProcData,  KVA, 0xfffff /*4G */);
  gdt_SetupPageSegment(seg_DomainCode,  KVA, LARGE_SPACE_PAGES );
  gdt_SetupPageSegment(seg_DomainData,  KVA, LARGE_SPACE_PAGES );
  
  gdt_lgdt();
  gdt_ReloadSegRegs();
}


#ifdef OPTION_DDB
#include <arch-kerninc/db_machdep.h>
#include <ddb/db_output.h>

void
db_show_gdt(db_expr_t d, int i, db_expr_t dd, char* c)
{
  gdt_ddb_dump();
}


void
gdt_ddb_dump()
{
  int entry = 0;
  db_printf("GdtTable at 0x%08x\n", &gdt_GdtTable);
  
  for (entry = 0; entry < GDT_ENTRIES; entry++) {
    SegDescriptor* seg /*@ not null @*/ = (SegDescriptor*) &gdt_GdtTable[entry*2];

    uint32_t base = ((((uint32_t)seg->hiBase) << 24) |
		     (((uint32_t)seg->midBase) << 16) |
		     ((uint32_t)seg->loBase));
    uint32_t limit = ((((uint32_t)seg->hiLimit) << 16) |
		     ((uint32_t)seg->loLimit));
    if (seg->granularity == 1)
      limit <<= EROS_PAGE_ADDR_BITS;
    
    db_printf("[%02d] base=0x%08x limit=0x%08x [w0=0x%08x w1=0x%08x]\n", 
	      entry, base, limit,
	      gdt_GdtTable[entry * 2], gdt_GdtTable[entry * 2 + 1]);

    db_printf("     ty=%d sys=%d dpl=%d pres=%d avl=%d gran=%d (%d bit)\n",
	      seg->type, seg->system, seg->dpl, seg->present, seg->avl,
	      seg->granularity, seg->sz ? 32 : 16);
  }
}
#endif
