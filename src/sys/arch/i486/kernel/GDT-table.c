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
#include <eros/i486/pseudoregs.h>

uint32_t gdt_GdtTable[GDT_ENTRIES*2] = {
				/* Entry 0 - Null Segment */
  0x0,
  0x0,

				/* Entry 1 - Kernel text */
  0x0000ffff,			/* 1G, base at 3G */
  0xc0c39b00,			/* accessed, ReadExec, paged, 386, DPL0 */

				/* Entry 2 - Kernel data/stack */
  0x0000ffff,			/* 4G, base at 3G -- NOTE THIS WRAPS */
  0xc0cF9300,			/* accessed, ReadWrite, paged, DPL0 */

				/* Entry 3 - TSS descriptor for current domain. */
  sizeof(i386TSS)-1,		/* 104 bytes, base at XXXX */
  0x00008900,			/* TSS,DPL0,byte granularity, base at XXXX */

				/* Entry 4 - Domain code */
  0x0000ffff,			/* 3G (for now), base at 0 */
  0x00cbfb00,			/* accessed, ReadExec, paged, 386, DPL3 */

				/* Entry 5 - Domain data */
  0x0000ffff,			/* 3G (for now), base at 0 */
  0x00cbf300,			/* accessed, ReadWrite, paged, 386, DPL3 */

				/* Entry 6 - Domain Pseudo Regs */
  sizeof(pseudoregs_t)-1,	/* base at XXX (rewritten) */
  0x0040f300,			/* accessed, ReadWrite, byte, 386, DPL3 */

				/* Entry 7 - kernel process text */
  0x0000ffff,			/* 1G, base at 3G */
  0xc0c3bb00,			/* accessed, ReadExec, paged, 386, DPL1 */

				/* Entry 8 - Kernel process data/stack */
  0x0000ffff,			/* 4G, base at 3G -- NOTE THIS WRAPS */
  0xc0cfb300,			/* accessed, ReadWrite, paged, DPL1 */

#if 0
  /* NOT SURE DESCRIPTIONS ON THE FOLLOWING ARE BELIEVABLE!! */
  
				/* Entry 9 - APM 32 bit code seg */
  0x00000000,			/* base at 0 */
  0x00c09b00,			/* accessed, ReadExec, 386, DPL0 */

				/* Entry 10 - APM 16 bit code seg */
  0x00000000,			/* base at 0 */
  0x00809b00,			/* accessed, ReadExec, 286, DPL0 */

				/* Entry 11 - APM data seg */
  0x00000000,			/* base at 0 */
  0x00c09300			/* accessed, ReadExec, 386, DPL0 */
#endif
};
