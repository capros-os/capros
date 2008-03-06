	.file "interpreter.S"
/*
 * Copyright (C) 2008, Strawberry Development Group.
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

/* An interpreter that issues capability invocations described in a table.

This program needs no read-write storage and can be used to build
an address space.
*/

/* This program must be linked with InterpreterTable, which is simply
an array of Message structures.
You can build it using InterpreterTable.h.
The array must end with a message that transfers control, usually
swapAddrSpaceAndPC32. */

#include <eros/machine/asm.h>
	
#include <eros/target-asm.h>
#include <eros/arch/arm/SWI.h>
#include <eros/Invoke.h>
	
	.text
#ifdef __ELF__
ENTRY(_start)
#else
#error
ENTRY(start)
#endif

	ldr r0,=ConstructionTable
	b loop

	.skip _start + 0x20 - ., 0
// Destruction interpreter starts at 0x20.

/* We get here from InterpreterDestroy(), which calls:
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF,
    telospaceCap,
    0x20,	// well-known address of destruction interpreter
    finalResult,
    NULL,	// won't be received by this code
    oldSpaceCap);

  The passed retCode is returned in w1, which is placed in r2. */

	mov r7,r2	// save it
	ldr r0,=DestructionTable
loop:
	// r0 has address of struct InterpreterStep to interpret.
	// r7 has final error if any
	ldr r6,[r0,#72]		// destructionOffset
	ldmia r0,{r1-r5}
	cmp r6,#0
	movlt r5,r7		// if < 0, set snd_w1
	ldr r14,[r0,#32]	// rcv_keys
	ldr r12,[r0,#36]	// rcv_limit
	swi SWI_Invoke

	/* All values are returned in registers, so we don't need any
	read-write storage. */

	// Return code must be OK:
	cmp r1,#RC_OK

	/* Other return values are ignored. */

	addeq r0,r0,#76		// sizeof(struct InterpreterStep)
	beq loop

	// Got an error in r1.
	ldr r7,[r0,#68]		// errorResult
	cmp r7,#1		// faultOnError?
	swieq SWI_halt
	ldr r0,=DestructionTable
	cmp r7,#0		// passErrorThrough?
	moveq r7,r1
	add r0,r6		// destructionOffset loaded above
	b loop
