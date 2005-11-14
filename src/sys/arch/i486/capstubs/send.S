	.file	"send.S"
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
		
	movl	32(%ebp),%eax		/* snd code */
	movl	36(%ebp),%ebx		/* snd w1 */
	movl	40(%ebp),%ecx		/* snd w2 */
	movl	44(%ebp),%edx		/* snd w3 */
	
	movl	$0,%esi			/* rcv len */
	movl	$0,%edi			/* rcv ptr */

	int	$0x31			/* invocation trap */
	
	/* rcv code also the return value, so stash that in the
	   EAX slot of the PUSHA frame:	*/
	movl	$0,28(%esp)		/* rcv code */
	
	popa
	ret

