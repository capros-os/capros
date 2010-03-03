	.file	"asm.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2008-2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

#include <eros/arch/i486/asm.h>
#include <eros/arch/i486/target-asm.h>
#include "asm.h"

	.text

ENTRY(ReadCR0)
	movl %cr0,%eax
	ret

ENTRY(WriteCR0)
	movl 4(%esp),%eax
	movl %eax,%cr0
	ret

#ifdef EROS_HAVE_FPU

ENTRY(FPUInit)
	fninit
	ret

ENTRY(FPUSave)
	clts		// ClearTSFlag()
ENTRY(FsaveInstruction)	// this instruction can trap
	fsave 0(%eax)
	// Set the TS flag
	movl %cr0,%eax
	orl $CR0_TS,%eax
	movl %eax,%cr0
	ret

ENTRY(FPURestore)
	clts		// ClearTSFlag()
	frstor 0(%eax)
	// Set the TS flag
	movl %cr0,%eax
	orl $CR0_TS,%eax
	movl %eax,%cr0
	ret

#if 0 // not needed
// Return the FPU Control Word in %ax.
ENTRY(ReadFCW)
	pushl	%ebp
	movl	%esp,%ebp
	sub $4,%esp
	
	fnstcw 0(%esp)
	popl %eax

	leave
	ret
#endif

ENTRY(ClearTSFlag)
	clts
	ret

#endif // EROS_HAVE_FPU

ENTRY(rdtsc)
	pushl	%ebp
	movl	%esp,%ebp
	
	xorl	%eax,%eax
	xorl	%edx,%edx
	
	cmpl	$5,EXT(CpuType)
	jb	1f		/* don't call it if not supported */

	rdtsc

1:	leave
	ret

/*
 * Halt the processor without rebooting, ensuring that the panic message will 
 * be seen. 
 * FIX:  Whether we halt or reboot should be configurable, but not for now.
 */
ENTRY(halt)
	cli			/* disable interrupts */
	movl	$0x8f418f48,0x000b8000	/* "HA" */
	movl	$0x8f548f4C,0x000b8004	/* "LT" */
#if 1
	movl	$0x8f208f20,0x000b8008	/* "  " */
	movb    4(%esp),%bl	/* pick up character arg */
	movb	$0x8f,%bh
	movw	%bx,0x000b800a	/* <the char> */
	
	movl	$0x8f308f20,0x000b800c	/* " 0" */
	movw	$0x8f78,0x000b8010	/* "x" */
	movl	$0x000b800e,%esi
	addl	$16,%esi	/* width of number on video memory */	
pf_show_addr:
	/* Print out the fault address */
	movb	$0x8f,%ah
	movl    (%esp),%ebx	/* pick up return address */
	subl	$5,%ebx		/* back up to the call instruction */
	movl	$0x8,%ecx
	
px:	subl	$2,%esi
	movb    %bl,%al
	andb	$0xf,%al
	cmpb	$0x9,%al
	ja	hex_digit
	addb	$0x30,%al	/* add ascii of '0' */
	jmp	got_digit
hex_digit:	
	addb	$0x41,%al	/* add ascii of 'A' */
	subb	$10,%al		/* subtract 10 */
got_digit:	
	shr	$4,%ebx
	movw	%ax,(%esi)
	loop	px
#endif
	
L_halt:
	hlt
	jmp L_halt

ENTRY(abort)
	movl	$0x000b8000,%eax	// CGA buffer?
	movl	$0x07620741,(%eax)	// "Ab"
	cli			/* disable interrupts */
L_abort: jmp L_abort

/*
 * Halt the processor without rebooting, ensuring that the panic message will 
 * be seen.  Don't change the interrupt state.
 */
ENTRY(pause)
L_pause:
	hlt
	jmp L_pause

#if 0
/*
 * Stall: stall for a small number of cycles while waiting for some piece of 
 * hardware to settle:
 */
ENTRY(Stall)
	pushl %ebp
	movl %esp,%ebp
	movl 8(%ebp),%eax
	testl %eax,%eax
	je L_Stall_Done
	.align 2,0x90
L_Stall_Loop:
/APP
	nop
/NO_APP
	decl %eax
	jne L_Stall_Loop
L_Stall_Done:
	leave
	ret
#endif
