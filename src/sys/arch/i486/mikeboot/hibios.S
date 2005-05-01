/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

/*
 * Interface wrappers to the BIOS functions that all this is built on.
 * This file is derived from the Mach file bios.S
 */
	.file	"hibios.S"

#include <eros/i486/asm.h>

/*
 * GetDisplayMode() -- get the active video mode
 */

ENTRY(GetDisplayMode)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	call	EXT(prot_to_real)	/* enter real mode */

	movb	$0xf, %ah		/* ask for display info */

	sti
	int	$0x10
	cli

	movb	%al, %bl

	data32
	call	EXT(real_to_prot)

	xorl	%eax, %eax
	movb	%bl, %al

	pop	%ebx
	pop	%ebp
	ret

/*
 * SetDisplayMode() -- set the active video mode
 */

ENTRY(SetDisplayMode)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	xor	%ebx, %ebx
	/* Grab the passed mode number (first arg): */
	movb	0x8(%ebp),%bl

	call	EXT(prot_to_real)	/* enter real mode */

	.code16

	movb	$0x0, %ah		/* set display mode */
	movb	%bl, %al		/* mode to set */

	sti
	int	$0x10
	cli

	call	EXT(real_to_prot)

	.code32

	pop	%ebx
	pop	%ebp
	ret
