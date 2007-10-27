/*
 * Copyright (C) 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* threadnum.h -- get current thread number
*/

#include <eros/arch/arm/asm.h>
#include <linuxk/lsync.h>

	.text
	ENTRY(lk_getCurrentThreadNum)
/* See which area the stack pointer falls in. */
	sub r0,sp,#1	// in case the stack is completely empty
	sub r0,r0,#LK_STACK_BASE
	mov r0,r0, LSR #LK_LGSTACK_AREA
	bx lr
