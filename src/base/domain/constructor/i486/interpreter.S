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
	
	.text
#ifdef __ELF__
ENTRY(_start)
#else
#error
ENTRY(start)
#endif

	movl EXT(InterpreterTable),%ebp	/* ptr to Message structure */
loop:
	movl    32(%ebp),%eax	// snd_code
	movl    36(%ebp),%ebx	// snd_w1
	movl    40(%ebp),%ecx	// snd_w2
	movl    44(%ebp),%edx	// snd_w3
	movl    20(%ebp),%esi	// rcv_limit
	movl    24(%ebp),%edi	// rcv_data
	int     $0x31		// invocation trap

	/* All values are returned in registers, so we don't need any
	read-write storage. */

	// Return code must be OK:
	cmpl	$RC_OK,%eax
	jne error

	/* Other return values are ignored. */

	addl	$68,%ebp	// sizeof(Message)
	jmp loop

	/* On error, call the console to halt the process */
/* INSTEAD, there should be a parallel table for undoing the construction. */
error:	jmp .
