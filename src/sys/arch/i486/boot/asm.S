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
 * See additional copyright notices at bottom of this file.
 */
	.file "asm.S"

#include <eros/i486/asm.h>
#include "boot-asm.h"

	.text

/*
 *
 * real_to_prot()
 * 	transfer from real mode to protected mode.
 */

ENTRY(real_to_prot)
	/* guarantee that interrupt is disabled when in prot mode */
	cli

	/* load the gdtr */
	addr32
	data32
	lgdt	EXT(Gdtr)

	/* set the PE bit of CR0 */
	mov	%cr0, %eax

	data32
	or	$CR0_PE_ON, %eax
	mov	%eax, %cr0 

	/*
	 * make intrasegment jump to flush the processor pipeline and
	 * reload CS register
	 */
	data32
	ljmp	$BOOTCODE32, $xprot

xprot:
	/*
	 * we are in USE32 mode now
	 * set up the protected mode segment registers : DS, SS, ES
	 */
	mov	$BOOTDATA32, %eax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	ret

/*
 *
 * prot_to_real()
 * 	transfer from protected mode to real mode
 * 
 */

ENTRY(prot_to_real)

	/* set up a dummy stack frame for the second seg change. */
	movl	$BOOT2_SEG, %eax
	pushw	%ax
	movl	$xreal, %eax	/* gas botches pushw $xreal, extra bytes 0, 0 */; 
	pushw	%ax		/* decode to add %al, (%eax) (%al usually 0) */

	/* Change to use16 mode. */
	ljmp	$BOOTCODE16, $code16_reentry

code16_reentry:
	/* clear the PE bit of CR0 */
	mov	%cr0, %eax
	data32
	and 	$CR0_PE_OFF, %eax
	mov	%eax, %cr0

	/*
	 * make intersegment jmp to flush the processor pipeline
	 * using the fake stack frame set up earlier
	 * and reload CS register
	 */
	lret

xreal:
	/*
	 * we are in real mode now
	 * set up the real mode segment registers : DS, SS, ES
	 */
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %ss
	movw	%ax, %es

	data32
	ret

/*
 * StartKernel(phyaddr, sysinfo)
 *	start the program in protected mode where phyaddr is the entry point,
 *	passing bootdev and SysInfo pointer
 */

ENTRY(StartKernel)
	push	%ebp
	mov	%esp, %ebp
	movl	%esp, %eax		/* Use eax as the old stack pointer */

	/* Convert the current stack to a 32 bit flat model.  Note that we
	   are simply passing our own stack to the kernel, which isn't
	   necessarily a good idea... */
	mov	$KERNDATASEG, %ebx
	movw	%bx, %ss
	addl	$(BOOT2_SEG<<4),%esp
	
	/* copy the arguments from the old stack to the new stack */
	pushl	0x0C(%eax)		/* sysinfo */

	/* the following piece of cruft is to allow us to do
	 * an intersegment branch using the passed address.
	 */	

	/* push on our entry address */
	pushl	$KERNCODESEG		/* segment selector */
	pushl	0x08(%eax)		/* kernel entry address */

	/* convert over the other data segs */
	movl	$KERNDATASEG, %ebx
	movw	%bx, %ds
	movw	%bx, %es
	movw	%bx, %fs
	movw	%bx, %gs

	movl	$0x1, %eax	/* kernel xfer convention, so we can 
				   fix this later */
	
	/* convert the PC (and code seg) */
	lret
	
/*
 * ppcpy(from, to, len)
 *	copy /len/ bytes from physical address /from/ to physical address
 *      /to/.
 */
ENTRY(ppcpy)
	push	%ebp
	mov	%esp, %ebp
	push	%es
	push	%ds
	push	%esi
	push	%edi
	push	%ecx

	cld

	mov	0x8(%ebp), %esi		/* source */
	mov	0xc(%ebp), %edi		/* destination */
	mov	0x10(%ebp), %ecx	/* count */

	/* set %es to point at the flat segment */
	mov	$DATA32SEG, %eax
	movw	%ax, %es
	movw	%ax, %ds

	rep
	movsb

	pop	%ecx
	pop	%edi
	pop	%esi
	pop	%ds
	pop	%es
	pop	%ebp

	ret
	
/*
 *
 * get_mem_map(addr, cont) :  address and old continuation value (zero to
 *		start), for the Query System Address Map BIOS call.
 *
 *  Sets the first 4-byte int value of "addr" to the size returned by
 *  the call.  If the call fails, sets it to zero.
 *
 *	Returns:  new (non-zero) continuation value, 0 if done.
 *
 * NOTE: Currently hard-coded for a maximum buffer length of 1024.
 */

ENTRY(get_mem_map)
	push	%ebp
	push	%ebx
	push	%ecx
	push	%edx
	push	%edi
	push	%esi

	/* place address (+4) in ES:DI */
	movl	0x1c(%esp), %eax
	addl	$4, %eax
	movl	%eax, %edi
#if 0
	andl	$0xf, %edi
	shrl	$4, %eax
	movl	%eax, %esi
#endif

	/* set continuation value */
	movl	0x20(%esp), %ebx

	/* set default maximum buffer size */
	movl	$0x14, %ecx

	/* set EDX to 'SMAP' */
	movl	$0x534d4150, %edx

	call	EXT(prot_to_real)	/* enter real mode */

	data32
	movl	$0xe820, %eax
	int	$0x15

#define REL16(x) .word EXT(x)-0f; 0:;
#define REL8(x)  .byte EXT(x)-0f; 0:;
#define jc_16_short	.byte 0x72; 
#define jne_16_short	.byte 0x75;
#define jl_16_short	.byte 0x7c;
#define jg_16_short	.byte 0x7f;
#define jmp_16_short    .byte 0xeb;
	
	jc_16_short	REL8(xnosmap)

	data32
	cmpl	$0x534d4150, %eax
	jne_16_short	REL8(xnosmap)

	data32
	cmpl	$0x14, %ecx
	jl_16_short	REL8(xnosmap)

	data32
	cmpl	$0x400, %ecx
	jg_16_short	REL8(xnosmap)
        jmp_16_short    REL8(xsmap)
	
xnosmap:
	data32
	xorl	%ecx, %ecx

xsmap:
	data32
	call	EXT(real_to_prot)

	/* write length of buffer (zero if error) into "addr" */
	movl	0x1c(%esp), %eax
	movl	%ecx, (%eax)

	/* set return value to continuation */
	movl	%ebx, %eax

	pop	%esi
	pop	%edi
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
 *	from: Mach, Revision 2.2  92/04/04  11:34:13  rpd
 *	asm.S,v 1.5 1994/10/06 09:41:01 rgrimes Exp
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
