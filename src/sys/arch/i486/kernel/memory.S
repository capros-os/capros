	.file	"memory.S"
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
 * The basic job of the startup code is to load known (reasonable) values
 * into all of the segment registers, create a stack, initialize the
 * first thread, establish a more permanent GDT, and branch into main
 * as quickly as possible. The theory is that C/C++ code is much more
 * maintainable and portable than assembly code. For more details, see
 * the documentation of kernel startup. 
 * 
 * On entry, the kernel is sitting at physical 0x1000.   Note that this may
 * shortly need to change, as the kernel is growing.  There is some incentive
 * to move it to 0x1001000, which would put it in upper memory and leave us
 * more room.  At some point I will probably re-engineer things so that the
 * kernel can be loaded in either place according to where it was linked.
 *
 * The bootstrap loader transfers control to us in protected mode.  It has
 * loaded all segment registers with 32-bit, 4GB, non-offsetting segments
 * of appropriate type, which were loaded from a temporary GDT that it 
 * built.  The A20 line has been enabled.
 *
 * In the latest design, no virtual mapping is established by the boot
 * loader, which shrinks it (the boot loader) somewhat.
 *
 * When we recieve control, interrupts are disabled.
	
 * A single kernel argument, which is a pointer to the SysInfo structure, 
 * is sitting on a temporary stack in low memory.  The transfer mechanism
 * for this structure will soon need to change.
 *
 * While the GDT set up by the bootstrap loader is still valid and 
 * addressable, the kernel should set up an official one as quickly as 
 * possible.  The one created by the bootstrap loaded lives someplace
 * that the kernel is almost certainly just about to reclaim.
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

