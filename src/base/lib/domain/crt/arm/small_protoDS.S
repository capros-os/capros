	.file	"small_protoDS.S"
/*
 * Copyright (C) 2009, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include <eros/arch/arm/asm.h>

/*
protospace_destroy_small can be called from anywhere on the stack.
Move to the base of the stack so we can destroy all the other stack pages
in a C program.

Also, load __rt_stack_pages so we can destroy the data/bss section.
 */

	.text
ENTRY(protospace_destroy_small)
	ldr sp,=__rt_stack_pointer
	ldr r2,=__rt_stack_pages
	ldr sp,[sp]
	ldr r2,[r2]
// Call a C procedure to destroy pages for the stack and data/bss areas.
// Parameters in r0 and r1 are still there.
	b protospace_destroy_small_2
	// protospace_destroy_small_2 does not return
