	.file	"send.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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

#include <eros/arch/i486/asm.h>
#include <eros/arch/i486/target-asm.h>
#include <eros/Invoke.h>
	
	.weak	EXT(SEND)
	.set	EXT(SEND),EXT(__rt_do_SEND)
	
	/* SEND()
	 *
	 * For a stack frame map, see the STACKFRAME file
	 */
ENTRY(__rt_do_SEND)	
	pusha
	
	movl	36(%esp),%ebp		/* pointer to message structure */
	movl	$IT_Send,(%ebp)		/* invocation type */

sendCommon:
	movl	32(%ebp),%eax		/* snd code */
	movl	36(%ebp),%ebx		/* snd w1 */
	movl	40(%ebp),%ecx		/* snd w2 */
	movl	44(%ebp),%edx		/* snd w3 */
	
	int	$0x31			/* invocation trap */
	
	popa
	ret

	.weak	EXT(PSEND)
	.set	EXT(PSEND),EXT(__rt_do_PSEND)
	
	/* PSEND()
	 *
	 * For a stack frame map, see the STACKFRAME file
	 */
ENTRY(__rt_do_PSEND)	
	pusha
	
	movl	36(%esp),%ebp		/* pointer to message structure */
	movl	$IT_PSend,(%ebp)		/* invocation type */
	jmp sendCommon
