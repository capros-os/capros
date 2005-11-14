	.file	"return.S"
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
		
	.weak	EXT(NPRETURN)
	.set	EXT(NPRETURN),EXT(__rt_do_NPRETURN)
	
	/* NPRETURN()
	 *
	 * For a stack frame map, see the STACKFRAME file
	 */
ENTRY(__rt_do_NPRETURN)	
	pusha
	
	movl	36(%esp),%ebp		/* pointer to message structure */
	movl	$IT_NPReturn,(%ebp)	/* invocation type */
		
	/* First, probe the receive pages, both for verification and
	   to eliminate the possibility of receipt failure.  It is still
	   possible for failure to be discovered when the receive actually 
	   happens if somebody screws with your mapping while you are 
	   waiting, but hey nothing's perfect. */
	
	movl	20(%ebp),%ecx		/* rcv len */
	jecxz	2f			/* skip probe if zero length */
	
	movl	24(%ebp),%esi		/* rcv ptr */
	addl	%esi,%ecx		/* this is upper exclusive bound. */

1:	andb	$0xff,(%esi)		/* validate current page */
	addl	$EROS_PAGE_SIZE,%esi
	cmpl	%esi,%ecx
	ja	1b			/* remember - EXCLUSIVE bound */

	andb	$0xff,-1(%ecx)		/* validate last page, which may
					   be the first page. */
	
	/* done with probe */	
2:
	
	movl	32(%ebp),%eax		/* snd code */
	movl	36(%ebp),%ebx		/* snd w1 */
	movl	40(%ebp),%ecx		/* snd w2 */
	movl	44(%ebp),%edx		/* snd w3 */
	
	movl	20(%ebp),%esi		/* rcv len */
	movl	24(%ebp),%edi		/* rcv ptr */

	int	$0x31			/* invocation trap */
	
	movl	%eax,48(%ebp)		/* rcv code */
	movl	%ebx,52(%ebp)		/* rcv w1 */
	movl	%ecx,56(%ebp)		/* rcv w2 */
	movl	%edx,60(%ebp)		/* rcv w3 */

	movw	%di,64(%ebp)		/* key info */
	movl	%esi,68(%ebp)		/* actual len received */

	/* rcv code also the return value, so stash that in the
	   EAX slot of the PUSHA frame:	*/
	movl	%eax,28(%esp)		/* rcv code */
	
	popa
	ret
