	.file	"lostart.S"
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
	.text
	
	/* Reset the interrupt masks on the PIC.  Interrupts must be 
	   disabled while we actually diddle the part, but if you called 
	   this at all, you presumably intend for them to be enabled
	   afterwards (which is why the interrupt return path doesn't
	   use this function */
ENTRY(SetPIC__3IRQ)
	pushl	%ebp
	movl	%esp,%ebp
	
	cli	/* disable interrupts while we diddle the chip */

	movl EXT(_3IRQ.pic_mask),%eax

	outb  %al,$0x21		/* program primary PIC */
	movb  %ah,%al
	outb  %al,$0xa1		/* program secondary PIC */
	
	sti
	leave
	ret

