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
 * putc(ch)
 * BIOS call "INT 10H Function 0Eh" to write character to console
 *	Call with	%ah = 0x0e
 *			%al = character
 *			%bh = page
 *			%bl = foreground color ( graphics modes)
 */


ENTRY(putc)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%ecx

	movb	0x8(%ebp), %cl

	call	EXT(prot_to_real)

	data32
	mov	$0x1, %ebx		/* %bh=0, %bl=1 (blue) */
	movb	$0xe, %ah
	movb	%cl, %al
	sti
	int	$0x10			/* display a byte */
	cli

	data32
	call	EXT(real_to_prot)

	pop	%ecx
	pop	%ebx
	pop	%ebp
	ret


/*
 * getc()
 * BIOS call "INT 16H Function 00H" to read character from keyboard
 *	Call with	%ah = 0x0
 *	Return:		%ah = keyboard scan code
 *			%al = ASCII character
 */

ENTRY(getc)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx			/* save %ebx */

	call	EXT(prot_to_real)

	movb	$0x0, %ah
	sti
	int	$0x16
	cli

	movb	%al, %bl		/* real_to_prot uses %eax */

	data32
	call	EXT(real_to_prot)

	xor	%eax, %eax
	movb	%bl, %al

	pop	%ebx
	pop	%ebp
	ret

/*
 *
 * bios_diskinfo(drive):  return a word that represents the
 *	max number of sectors and heads and drives for this device
 *
 */

ENTRY(bios_diskinfo)
	push	%ebp
	mov	%esp, %ebp
	push	%es
	push	%ebx
	push	%ecx
	push	%edx
	push	%edi

	movb	0x8(%ebp), %dl		/* diskinfo(drive #) */
	call	EXT(prot_to_real)	/* enter real mode */

	movb	$0x8, %ah		/* ask for disk info */

	sti
	int	$0x13
	cli

	jnc	ok
#if 0
	/*
	 * Urk.  Call failed.  It is not supported for floppies by old BIOS's.
	 * Guess it's a 15-sector floppy.
	 */
	subb	%ah, %ah		/* %ax = 0 */
	movb	%al, %al
	movb	%ah, %bh		/* %bh = 0 */
	movb	$2, %bl			/* %bl	bits 0-3 = drive type,
						bit    2 = 1.2M */
	movb	$79, %ch		/* max track */
	movb	$15, %cl		/* max sector */
	movb	$1, %dh			/* max head */
	movb	$1, %dl			/* # floppy drives installed */
	/* es:di = parameter table */
#else
	/* call failed -- return all zeros */
	xorl	%ecx,%ecx
	xorl	%edx,%edx
#endif

	/* carry = 0 */
ok:

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	/* 
	 * form a longword representing all this gunk:
	 *       6 bit zero
	 *	10 bit cylinder
	 *	 8 bit head
	 *	 8 bit sector
	 */
	movb	%cl, %al		/* Upper two bits of cylinder count */
	andl	$192,%eax	
	leal	0(,%eax,4),%eax		/* << 2 */
	movb	%ch, %al		/* Lower 8 bits */
	sall	$16,%eax		/* << 16 */
	movb	%dh, %ah		/* max head */
	andb	$0x3f, %cl		/* mask of cylinder gunk */
	movb	%cl, %al		/* max sector (and # sectors) */

	pop	%edi
	pop	%edx
	pop	%ecx
	pop	%ebx
	pop	%es
	pop	%ebp
	ret
	
/*
 * biosread(dev, cyl, head, sec, nsec, offseg, offset)
 *	Read "nsec" sectors from disk to offset "offset" in boot segment
 * BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
 *	Call with	%ah = 0x2
 *			%al = number of sectors
 *			%ch = cylinder
 *			%cl = sector
 *			%dh = head
 *			%dl = drive (0x80 for hard disk, 0x0 for floppy disk)
 *			%es:%bx = segment:offset of buffer
 *	Return:		
 *			%al = 0x0 on success; err code on failure
 */

ENTRY(biosread)
	push	%ebp
	mov	%esp, %ebp

	push	%ebx
	push	%ecx
	push	%edx
	push	%es

	movb	0x10(%ebp), %dh
	movw	0x0c(%ebp), %cx
	/* cylinder; the highest 2 bits of cyl is in %cl */
	xchgb	%ch, %cl
	rorb	$2, %cl
	movb	0x14(%ebp), %al
	orb	%al, %cl
	incb	%cl			/* sector; sec starts from 1, not 0 */
	movb	0x8(%ebp), %dl		/* device */
	movl	0x20(%ebp), %ebx	/* offset */

	/* prot_to_real will set %es to BOOTSEG */
	call	EXT(prot_to_real)	/* enter real mode */
	/* overwrite %es with desired target segment */
	addr32
	movl	0x1c(%ebp), %eax	/* offseg */
	mov	%ax,%es
	movb	$0x2, %ah		/* subfunction */
	addr32
	movb	0x18(%ebp), %al		/* number of sectors */

	sti
	int	$0x13
	cli

	/* save return value (actually movw %ax, %bx) */
	mov	%eax, %ebx

	data32
	call	EXT(real_to_prot)	/* back to protected mode */

	xor	%eax, %eax
	movb	%bh, %al		/* return value in %ax */

	pop	%es
	pop	%edx
	pop	%ecx
	pop	%ebx
	
	pop	%ebp

	ret
	
/*
 * Mach Operating System
 * Copyright (c) 1992, 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: Mach, Revision 2.2  92/04/04  11:34:26  rpd
 *	bios.S,v 1.4 1994/11/18 05:02:12 phk Exp
 */

/*
  Copyright 1988, 1989, 1990, 1991, 1992 
   by Intel Corporation, Santa Clara, California.

                All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
