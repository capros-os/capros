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

/*
 * Interface wrappers to the BIOS functions that all this is built on.
 * This file is derived from the Mach file bios.S
 */
	.file	"bios.s"
	
#include <eros/i486/asm.h>
/*
 * GetDisplayMode() -- get the active video mode
 */

ENTRY(GetDisplayMode)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	call	EXT(prot_to_real)	/* enter real mode */

	movb	$0x8, %ah		/* ask for display info */

	sti
	int	$0x10
	cli

	data32
	call	EXT(real_to_prot)

	pop	%ebx
	pop	%ebp
	ret
