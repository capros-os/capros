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

/* On entry, the whole bootstrap has been loaded, but we are still running
 * in 16 bit mode.  
 *
 * %cx and %dx hold information about the booted drive and partition.
 *
 * Load the GDT register and new segments to get us
 * into 32 bit mode ASAP and proceed.
 */
	
#include <eros/i486/asm.h>
#include "boot-asm.h"
#include "debug.h"
	
ENTRY(boot2_start)
	cli
	
	/* Load a GDT out of our own space, so we can overwrite the
	   first-stage boot module with impunity. */
	
	/* load %ds so that we can load the GDT table pointer */
	push	%cs
	pop	%ds
	
	/* load the gdtr */
	addr32
	data32
	lgdt	EXT(Gdtr)

	/* set the PE bit of CR0 */
	mov	%cr0, %eax

	data32
	or	$CR0_PE_ON, %eax
	mov	%eax, %cr0 
	/* We are now in Protected mode. */
	
	data32
	ljmp	$BOOTCODE32, $reload_segs
	
reload_segs:
	/* We are now in 32-bit mode. */
	
	movw	$BOOTDATA32, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	
	movl	$BOOT2_STACK,%esp

	movl	$0x000b8000,%eax	/* video address */
	subl	$BOOT2_ADDR,%eax	/* bias correctly */
	movl	$0x8f418f48,(%eax)	/* "HA" */
	
	/* save the boot drive and head information on the stack 
	 * temporarily while we zero the BSS area. */

	pushl	%ecx
	pushl	%edx
	pushl	%eax	/* not sure if this is needed, but... */
	pushl	%edi

	/*
	 * Zero the bss section. 
	 */

	cld

	movl	$ EXT(edata), %edi	/* destination */
	movl	$ EXT(end), %ecx
	subl	$ EXT(edata), %ecx
	mov	$0x0, %eax		/* value */

	rep
	stosb
	
	popl	%edi
	popl	%eax
	popl	%edx
	popl	%ecx
	
	movw	%dx,EXT(BootHeadDrive)
	movw	%cx,EXT(BootCylSec)
	
#ifdef ASM_DEBUG
	pushl	$EXT(low_hello_msg)
	call	printf
	addl	$4,%esp
#endif

#if 0
	call	EXT(set_boot_disk)
	addl	$8,%esp
#endif	
	
#ifdef ASM_DEBUG
	pushl	$EXT(low_proceed_msg)
	call	printf
	addl	$4,%esp

	call	EXT(waitkbd)
#endif
	
	call	EXT(main)
	
	/* halt() is implemented here so I can safely call it from
	   low assembler */
ENTRY(halt)
	cli			/* disable interrupts */
	
	movw	$0x3f2,%dx	/* shut off floppy drive */
	movb	$0x8,%al
	outb	%al,%dx
	
	movl	$0x000b8000,%eax	/* video address */
	subl	$BOOT2_ADDR,%eax	/* bias correctly */
	movl	$0x8f418f48,(%eax)	/* "HA" */
	movl	$0x8f548f4C,4(%eax)	/* "LT" */
1:
	hlt
	jmp 1b
	
	/* FROM HERE DOWN IS DATA */
	
VAR(low_hello_msg)
	.ascii "hello\n\0"
VAR(low_proceed_msg)
	.ascii "proceed?\n\0"
	
	.align 16
LEXT(gdt_base)
	.long 0x0		/* null entry */
	.long 0x0

	/* This is retained here for comment value...
		struct seg_desc {
			unsigned short	limit_15_0;
			unsigned short	base_15_0;
			unsigned char	base_23_16;
			unsigned char	p_dpl_type;
			unsigned char	limit_19_16 : 4;
			unsigned char	g_b_a : 4;
			unsigned char	base_31_24;
		};
	*/

	/* KERNEL CODE:	32-bit code segment with base=0, limit=4G */
	.word 0xffff		/* limit == 4G */
	.word 0x0		/* base[15:0] == 0 */
	.byte 0x0		/* base[23:16] == 0 */
	.byte 0x9f		/* p_dpl_type - present, non-system, 
				   accessed conforming code */
	.byte 0xcf		/* g_b_a: paged, 32 bit
				   limit[19:16] */
	.byte 0x00		/* base[31:24] */
	
	/* KERNEL DATA:	32-bit data segment with base=0, limit=4G
	   This segment is also used by the boot code to write physical 
	   memory. */
	.word 0xffff		/* limit == 4G */
	.word 0x0		/* base[15:0] == 0 */
	.byte 0x0		/* base[23:16] == 0 */
	.byte 0x93		/* p_dpl_type - present, non-system, 
				   accessed conforming code */
	.byte 0xcf		/* g_b_a: paged, 32 bit
				   limit[19:16] */
	.byte 0x00		/* base[31:24] */
	
	/* BOOT CODE:	32-bit code segment with base=BOOT2_SEG */
	.word 0xffff		/* limit == 4G */
	.word 0x0		/* base[15:0] == 0 */
	.byte (BOOT2_ADDR >> 16)/* base[23:16] == BOOT2_SEG */
	.byte 0x9f		/* p_dpl_type - present, non-system, 
				   accessed conforming code */
	.byte 0xcf		/* g_b_a: paged, 32 bit
				   limit[19:16] */
	.byte 0x00		/* base[31:24] */

	/* BOOT DATA:	32-bit data segment with base=BOOT2_SEG */
	.word 0xffff		/* limit == 4G */
	.word 0x0		/* base[15:0] == 0 */
	.byte (BOOT2_ADDR >> 16)/* base[23:16] == BOOT2_SEG */
	.byte 0x93		/* p_dpl_type - present, non-system, 
				   accessed data */
	.byte 0xcf		/* g_b_a: paged, 32 bit
				   limit[19:16] */
	.byte 0x00		/* base[31:24] */
	
	/* BOOT CODE:	16-bit code segment with base=BOOT2_SEG
	   Used during transition from real mode to protected mode. */
	.word 0xffff		/* limit == 64k */
	.word 0x0		/* base[15:0] == 0 */
	.byte (BOOT2_ADDR >> 16)/* base[23:16] == BOOT2_SEG */
	.byte 0x9f		/* p_dpl_type - present, non-system, 
				   conforming code */
	.byte 0x0		/* limit[19:16] */
	.byte 0x0		/* g_b_a: byte size, 16 bit */
	.byte 0x0		/* base[31:24] */
	
LEXT(gdt_bound)

ENTRY(Gdtr)
	.word EXT(gdt_bound) - EXT(gdt_base) - 1
	.long EXT(gdt_base) + BOOT2_ADDR
