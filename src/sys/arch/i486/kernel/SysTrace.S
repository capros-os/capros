	.file	"SysTrace.S"
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
ENTRY(mach_ClearCounters)
        /* END CONVERSION */
	pushl	%ebp
	movl	%esp,%ebp
	pusha
	
	xorl	%eax,%eax

	cmpl	$5,EXT(CpuType)
	jne	1f		/* don't call it if not supported */
	
	/* disable the counters */
	movl	$0x11,%ecx	/* CES */
	rdmsr
	pushl %eax
	pushl %edx
	andl	$0xFC00FC00,%eax
	orl	$0x02000200,%eax
	wrmsr
	
	/* Zero the current counters: */
	xorl	%edx,%edx
	xorl	%eax,%eax
	movl	$0x12,%ecx	/* Counter 0 */
	wrmsr
	movl	$0x13,%ecx	/* Counter 1 */
	wrmsr

	/* Reset the counters */
	popl	%edx
	popl	%eax
	movl	$0x11,%ecx	/* CES */
	wrmsr
	
	jmp	2f
	
1:	cmpl	$6,EXT(CpuType)
	jne	2f
		
	movl	$0x186,%ecx
	rdmsr
	movl	%eax,%ebx
	/* disable the counters */
	andl	$0xFFBFFFFF,%EAX
	wrmsr
	
	/* Zero the current counters: */
	xorl	%edx,%edx
	xorl	%eax,%eax
	movl	$0xc1,%ecx	/* Counter 0 */
	wrmsr
	movl	$0xc2,%ecx	/* Counter 1 */
	wrmsr

	/* Re-enable the counters if previously enabled */
	movl	$0x186,%ecx
	rdmsr
	movl	%ebx,%eax
	wrmsr
	
2:	popa
	leave
	ret

        /* CONVERSION */
ENTRY(mach_DisableCounters)
        /* END CONVERSION */
	pushl	%ebp
	movl	%esp,%ebp
	pusha
	
	cmpl	$5,EXT(CpuType)
	jne	1f
	
	/* Disable both event counters: */
	movl	$0x11,%ecx	/* CES */
	rdmsr
	andl	$0xff3fff3f,%eax
	wrmsr
	

1:	cmpl	$6,EXT(CpuType)
	jne	2f

	/* program counter 0 */
	movl	$0x186,%ecx
	rdmsr
	/* disable the counters */
	andl	$0xffBfffff,%EAX
	wrmsr

2:	popa
	leave
	ret

        /* CONVERSION */
ENTRY(mach_EnableCounters)
        /* END CONVERSION */
	pushl	%ebp
	movl	%esp,%ebp
	pusha

	cmpl	$5,EXT(CpuType)
	jne	1f
	
	movl	$0x11,%ecx
	rdmsr
	orl	$0x00c00040,%eax
	wrmsr

	jmp 2f

1:	cmpl	$6,EXT(CpuType)
	jne	2f

	/* program counter 0 */
	movl	$0x186,%ecx
	rdmsr
	orl	$0x00400000,%EAX
	wrmsr

2:	popa
	leave
	ret

ENTRY(Pentium_SetCounterMode)
	pushl	%ebp
	movl	%esp,%ebp
	pusha

	movl	8(%EBP),%EBX
	shll	$16,%EBX
	movw	8(%EBP),%BX
	
	andl	$0x003F003F,%EBX
	orl	$0x02000200,%EBX
	
	cmpl	$0,12(%EBP)
	je	1f		/* do not set cycle count bit */

	orl	$0x01000100,%EBX

1:	
	movl	$0x11,%ecx
	rdmsr
	andl	$0xFC00FC00,%eax
	orl     %ebx,%eax
	wrmsr
	
	popa
	leave	
	ret
	
ENTRY(PentiumPro_SetCounterMode)
	pushl	%ebp
	movl	%esp,%ebp
	pusha

	movl	8(%ebp),%EBX
	andl	$0xff,%ebx	/* only use event subfield */
	/* Set PC to indicate overflow*/
	orl	$0x80000,%ebx
	/* count in OS mode: */
	orl	$0x20000,%ebx
	/* UNIT = 0x20 (self): */
	orl	$0x2000,%ebx
		
	/* If we are supposed to be counting clocks, set E field too */
	cmpl	$0,12(%EBP)
	je	1f
	orl	$0x40000,%EBX
	
1:	
	
	/* program counter 0 */
	movl	$0x186,%ecx
	rdmsr

	movl	%ebx,EXT(setup_value)
	movl	%ebx,%eax
	wrmsr
	
	/* Counter one should count both OS and US events */
	orl	$0x10000,%ebx
	
	/* program counter 1 */
	movl	$0x187,%ecx
	rdmsr

	movl	%EBX,%EAX
	wrmsr

	popa
	leave
	ret
	
	
ENTRY(SetupCounters)
	pushl	%ebp
	movl	%esp,%ebp
	pusha

	cmpl	$5,EXT(CpuType)
	jne	1f
	
	movl	8(%EBP),%EBX
	shll	$16,%EBX
	movw	8(%EBP),%BX
	
	andl	$0x003F003F,%EBX
	orl	$0x02000200,%EBX
	
	cmpl	$0,16(%EBP)
	je	count_p5_events

	orl	$0x01000100,%EBX

count_p5_events:	
	movl	$0x11,%ecx
	rdmsr
	andl	$0xFC00FC00,%eax
	orl     %EBX,%eax
	wrmsr
	
	jmp 2f

1:	cmpl	$6,EXT(CpuType)
	jne	2f

	movl	12(%ebp),%EBX
	andl	$0xff,%ebx	/* only use event subfield */
	/* Set PC to indicate overflow*/
	orl	$0x80000,%ebx
	/* count in OS mode: */
	orl	$0x20000,%ebx
	/* UNIT = 0x20 (self): */
	orl	$0x2000,%ebx
		
	/* If we are supposed to be counting clocks, set E field too */
	cmpl	$0,16(%EBP)
	je	count_p6_events
	orl	$0x40000,%EBX
	
count_p6_events:	
	
	/* program counter 0 */
	movl	$0x186,%ecx
	rdmsr

	movl	%ebx,EXT(setup_value)
	movl	%ebx,%eax
	wrmsr
	
	/* Counter one should count both OS and US events */
	orl	$0x10000,%ebx
	
	/* program counter 1 */
	movl	$0x187,%ecx
	rdmsr

	movl	%EBX,%EAX
	wrmsr

2:	popa
	leave
	ret

        /* CONVERSION */
ENTRY(mach_ReadCounter)
        /* END CONVERSION */
	pushl	%ebp
	movl	%esp,%ebp
	pushl   %ecx

	cmpl	$5,EXT(CpuType)
	jl	1f		/* invalid counter or no counters */
	
	cmpl	$1,8(%ebp)
	jl	2f		/* counter 0 */
	je	3f		/* counter 1 */

	/* invalid counter -- return 0 */
1:	xorl	%eax,%eax
	xorl	%edx,%edx
	jmp	4f

	/* COUNTER ZERO PROCESSING */
	
2:	/* Is it a Pentum Pro? */
	cmpl	$6,EXT(CpuType)
	jne	2f	/* try next CPU */
	
	/* Fetch pentium pro counter value: */
	movl	$0xc1,%ecx
	rdmsr
	andl	$0xff,%edx	/* 40 bit counter */

	jmp	4f
	
2:	/* Is it a Pentum? */
	cmpl	$5,EXT(CpuType)
	jne	1b	/* unknown CPU */
	
	/* Fetch pentium counter value: */
	movl	$0x12,%ecx	/* Counter 0 */
	rdmsr
	andl	$0xff,%edx	/* 40 bit counter */
	
	jmp	4f
	
	/* COUNTER ONE PROCESSING */
3:	/* Is it a Pentum Pro? */
	cmpl	$6,EXT(CpuType)
	jne	3f	/* try next CPU */
	
	/* Fetch pentium pro counter value: */
	movl	$0xc2,%ecx
	rdmsr
	andl	$0xff,%edx	/* 40 bit counter */

	jmp	4f
	
3:	/* Is it a Pentum? */
	cmpl	$5,EXT(CpuType)
	jne	1b	/* unknown CPU */
	
	/* Fetch pentium counter value: */
	movl	$0x13,%ecx	/* Counter 1 */
	rdmsr
	andl	$0xff,%edx	/* 40 bit counter */

	jmp	4f
	
4:	popl	%ecx
	leave
	ret
	
#if 0
ENTRY(rdcounter0)
	pushl	%ebp
	movl	%esp,%ebp
	pushl   %edx
	pushl   %ecx
	
	xorl	%eax,%eax

	cmpl	$5,EXT(CpuType)
	jne	1f		/* don't call it if not supported */
	
	/* Fetch counter value: */
	movl	$0x12,%ecx	/* Counter 0 */
	rdmsr

	jmp	2f
	
1:	cmpl	$6,EXT(CpuType)
	jne	2f

	movl	$0xc1,%ecx
	rdmsr

2:	popl   %ecx
	popl   %edx
	leave
	ret

ENTRY(rdcounter1)
	pushl	%ebp
	movl	%esp,%ebp
	pushl   %edx
	pushl   %ecx
	
	xorl	%eax,%eax

	cmpl	$5,EXT(CpuType)
	jne	1f		/* don't call it if not supported */
	
	/* Fetch counter value: */
	movl	$0x13,%ecx	/* Counter 1 */
	rdmsr

1:	cmpl	$6,EXT(CpuType)
	jne	2f

	movl	$0xc2,%ecx
	rdmsr

2:	popl   %ecx
	popl   %edx
	leave
	ret
#endif

