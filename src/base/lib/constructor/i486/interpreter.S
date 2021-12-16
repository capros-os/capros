	.file "interpreter.S"
/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

#define sizeofMessage 72
#define offsetof_errorResult sizeofMessage
#define offsetof_destructionOffset (sizeofMessage+4)
#define sizeofInterpreterStep (sizeofMessage+8)

	.text
#ifdef __ELF__
ENTRY(_start)
#else
#error
ENTRY(start)
#endif

	movl $EXT(ConstructionTable),%ebp	// ptr to struct InterpreterStep
	jmp loop

	.skip _start + 0x20 - ., 0
// Destruction interpreter starts at 0x20.

/* We get here from InterpreterDestroy(), which calls:
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF,
    telospaceCap,
    0x20,	// well-known address of destruction interpreter
    finalResult,
    NULL,	// won't be received by this code
    oldSpaceCap);

  The passed retCode is returned in w1, which is placed in %ebx. */

	/* We have no stack, so we can use %esp as the "holding cell".
	Save the retCode in %esp. */
	movl	%ebx,%esp
	movl $EXT(DestructionTable),%ebp	// ptr to struct InterpreterStep
loop:
	// %ebp has address of struct InterpreterStep to interpret.
	// %esp has final error if any
	movl    32(%ebp),%eax	// snd_code
	movl    36(%ebp),%ebx	// snd_w1
	movl    40(%ebp),%ecx	// snd_w2
	movl    44(%ebp),%edx	// snd_w3
	cmpl	$-1,offsetof_destructionOffset(%ebp)	// getHolding?
	jne 1f
	movl	%esp,%ebx	// holding cell to snd_w1
1:
	movl    20(%ebp),%esi	// rcv_limit
	movl    24(%ebp),%edi	// rcv_data
	int     $0x31		// invocation trap

	/* All values are returned in registers, so we don't need any
	read-write storage. */

	// Check return code:
	cmpl	$RC_OK,%eax
	jne error

	cmpl	$-2,offsetof_destructionOffset(%ebp)	// setHolding?
	jne 1f
	movl	%ebx,%esp	// rcv_w1 to holding cell
1:
	/* Other return values are ignored. */

	addl	$sizeofInterpreterStep,%ebp
	jmp loop

error:
	// Got an error in %eax.
	movl offsetof_errorResult(%ebp),%esp
	cmpl $1,%esp		// faultOnError?
	je halt
	movl offsetof_destructionOffset(%ebp),%ebp
	addl EXT(DestructionTable),%ebp
	cmpl $0,%esp		// passErrorThrough?
	jne 1f
	movl %eax,%esp
1:	jmp loop

halt:	hlt		// fault
