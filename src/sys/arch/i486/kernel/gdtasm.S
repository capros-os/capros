	.file	"gdtasm.S"
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

#include <eros/i486/asm.h>

/* CONVERSION */	
ENTRY(gdt_ReloadSegRegs)
/* END CONVERSION */
	pushl	%ebp
	movl	%esp,%ebp

	/*
	 * immediately reload the data and stack segments so we are
	 * running out of a kosher universe:
	 */
	
	ljmp	$0x08,$next
next:
	mov	$0x10,%ax		/* load the kernel data segment. */
	mov	%ax,%ds
	mov	%ax,%es			/* for string moves! */
	mov	%ax,%ss			/* stack seg == data seg */

	xor	%ax,%ax			/* null segment to all others */
	mov	%ax,%fs	
	mov	%ax,%gs	

	leave

	ret

