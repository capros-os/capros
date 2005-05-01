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

i386TSS tss_TaskTable = {
  0,				/* backLink */
  0,				/* esp0 */
  sel_KernelData,		/* ss0 */
  0,				/* esp1 */
  sel_Null,		        /* ss1 */
  0,				/* esp2 */
  sel_Null,		        /* ss2 */
  0,				/* cr3 */
  0,				/* eip */
  0,				/* eflags */
  0,				/* eax */
  0,				/* ecx */
  0,				/* edx */
  0,				/* ebx */
  0,				/* esp */
  0,				/* ebp */
  0,				/* esi */
  0,				/* edi */
  0,				/* es */
  0,				/* cs */
  0,				/* ss */
  0,				/* ds */
  0,				/* fs */
  0,				/* gs */
  0,				/* ldtr */
  0,				/* trapOnSwitch */
  sizeof(i386TSS)		/* ioMapBase */
} ;


void
tss_Init()
{
  gdt_SetupTSS(seg_DomainTSS, (uint32_t) &tss_TaskTable);

  tss_ltss(sel_DomainTSS);
}

void
tss_ltss(SelectorName selector)
{
  __asm__ __volatile__("ltr  %%ax"
		       : /* no output */
		       : "a" (selector)
		       );
}

