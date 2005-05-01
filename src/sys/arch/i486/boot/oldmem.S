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


	.file "mem.S"
#include <eros/i486/asm.h>
/*
 *
 * old_memsize(i) :  return the memory size in KB. i == 0 for conventional
 *	        memory, i == 1 for extended memory
 *	BIOS call "INT 12H" to get conventional memory size
 *	BIOS call "INT 15H, AH=88H" to get extended memory size
 *		Both have the return value in AX.
 *
 */

ENTRY(old_memsize)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	mov	8(%ebp), %ebx

	call	EXT(prot_to_real)	/* enter real mode */

	cmpb	$0x1, %bl
	data32
	je	xext
	
	sti
	int	$0x12
	cli
	data32
	jmp	xdone

xext:	movb	$0x88, %ah
	sti
	int	$0x15
	cli

xdone:
	mov	%eax, %ebx

	data32
	call	EXT(real_to_prot)

	mov	%ebx, %eax
	pop	%ebx
	pop	%ebp
	ret
