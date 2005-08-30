	.file	"memory.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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

#include <eros/i486/asm.h>

/*
 * Memory operations.  All of the other kernels I have seen seem
 * to be able to do this inline, but I get clobbered condition codes
 * whenever I try it (GCC bug), so...
 */
	
/* bzero(dst, len) */
ENTRY(__bzero)
	pushl	%ebp
	movl	%esp,%ebp
	
	pushl   %edi
	pushl   %ecx
	pushl   %eax
	
	xorl	%eax,%eax
	movl	12(%ebp),%ecx
	movl	8(%ebp),%edi

	cld			/* copy forward */
	testl	$3,%ecx
	jnz	1f

	/* word move okay */
	shr	$2,%ecx
	rep 
	stosl
	/* jmp	2f */
	
1:	rep 
	stosb
	
2:	popl	%eax
	popl	%ecx
	popl	%edi
	leave
	ret

/* __bcopy(const void *from, void *to, size_t len); */
ENTRY(__bcopy)
	pushl	%ebp
	movl	%esp,%ebp
	
	pushl   %edi
	pushl   %esi
	pushl   %ecx
	
	movl	16(%ebp),%ecx
	movl	12(%ebp),%edi
	movl	8(%ebp),%esi

	cld			/* copy forward */
	testl	$3,%ecx
	jnz	1f

	/* word move okay */
	shr	$2,%ecx
	rep 
	movsl
	/* jmp	2f */
	
1:	rep 
	movsb
	
2:	popl	%ecx
	popl	%esi
	popl	%edi
	leave
	ret

