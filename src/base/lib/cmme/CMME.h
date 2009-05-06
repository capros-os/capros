#ifndef __CMME_H
#define __CMME_H
/*
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Common declarations for the CapROS Memory-Mapping Environment. */

/* Memory layout: */

/* 0x0: nothing. To catch misuse of NULL pointers.
0x1000: beginning of code, read only */

#define LK_STACK_BASE 0x00400000	// area for stack(s)
#define LK_LGSTACK_AREA 17
#define LK_STACK_AREA (1ul << LK_LGSTACK_AREA)	// 0x00020000

#define LK_MAPS_BASE 0x00800000	// area for ioremap()

#define LK_DATA_BASE 0x00c00000 // .data, .bss, and heap, backed by a VCSK
// Limit of memory is 0x02000000, because that is the limit of an ARM
// small space. 


/* Key registers: */
#include <domain/Runtime.h>
// KR_KEYSTORE is not required.
#define KR_OSTREAM    KR_APP(0)
#define KR_MAPS_GPT   KR_APP(1)
#define KR_CMME(i)    KR_APP(2+(i))	// first available key reg for program


/* Required constituents of constructors: */
#define KC_TEXT       0 // read-only portion of the program
#define KC_DATAVCSK   1	// initial data of the program
#define KC_INTERPRETERSPACE 2
#define KC_STARTADDR  3 // for dynamically-constructed drivers
#define KC_OSTREAM    4
#define KC_CMME(i)    (5+(i))

#ifndef __ASSEMBLER__
#endif

#endif // __CMME_
