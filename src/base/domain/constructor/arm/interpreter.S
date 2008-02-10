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
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <eros/ConsoleKey.h>

#include <domain/Runtime.h>
#include <linuxk/lsync.h>	// must have KR_OSTREAM
#include <eros/arch/arm/SWI.h>
	
#define MessageStruct(label, snd_invKey, snd_code, snd_w1, \
                      snd_key0, snd_key1, rcv_key0, rcv_key1) \
	.section .rodata ;\
	.balign 4 ;\
/* Just declare the values to be loaded into registers. */ \
label:	.byte snd_invKey, IT_Call, 0, 0 ;\
	.byte snd_key0,snd_key1,KR_VOID,KR_VOID ;\
	.long 0		/* snd_len */ ;\
	.long snd_code ;\
	.long snd_w1 ;\
/* #if RESUME_SLOT == 3 */ \
	.long 0,0	/* snd_w2 and _w3 */ ;\
	.long 0		/* snd_data */ ;\
	.byte rcv_key0,rcv_key1,KR_VOID,KR_VOID /* snd_keys */ ;\
	.long 0, 0	/* rcv_limit and rcv_data, not used */ ;\
	/* The remainder of the Message structure is fields for values */ \
	/* that are returned in registers, not in the structure. */ \
	.text

	.text
#ifdef __ELF__
ENTRY(_start)
#else
#error
ENTRY(start)
#endif

	ldr r0,=InterpreterTable
loop:	ldmia r0,{r1-r5}
	ldr r14,[r0,#32]	// rcv_keys
	ldr r12,[r0,#36]	// rcv_limit
	swi SWI_Invoke

	/* All values are returned in registers, so we don't need any
	read-write storage. */

	// Return code must be OK:
	cmp r1,#RC_OK
	bne error

	/* Other return values are ignored. */

	add r0,r0,#68		// sizeof(Message)
	b loop

	/* On error, call the console to halt the process */
/* INSTEAD, there should be a parallel table for undoing the construction. */
error:
	mov r6,r0		// save these for debugging
	mov r7,r1

	// Get Console key from constituent 0.
	ldr r0,=constitMsg
	ldmia r0,{r1-r5}
	ldr r14,[r0,#32]	// rcv_keys
	ldr r12,[r0,#36]	// rcv_limit
	swi SWI_Invoke
	
errLoop:
	ldr r0,=errorMsg
	ldmia r0,{r1-r5}
	ldr r14,[r0,#32]	// rcv_keys
	ldr r12,[r0,#36]	// rcv_limit
	swi SWI_Invoke
	b errLoop

	MessageStruct(constitMsg, KR_CONSTIT, OC_capros_Node_getSlotExtended,
                      KC_OSTREAM,
                      KR_VOID, KR_VOID, KR_TEMP0, KR_VOID)

	MessageStruct(errorMsg, KR_TEMP0, OC_Console_KDB, 0,
                      KR_VOID, KR_VOID, KR_VOID, KR_VOID)
