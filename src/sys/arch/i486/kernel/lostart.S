	.file	"lostart.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2008, 2009, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/*
 * The basic job of the startup code is to load known (reasonable) values
 * into all of the segment registers, create a stack, initialize the
 * first thread, establish a more permanent GDT, and branch into main
 * as quickly as possible. The theory is that C/C++ code is much more
 * maintainable and portable than assembly code. 
 * 
 * On entry, the kernel is sitting at physical 0x100000 (KERNPBASE),
 * which puts it in upper memory and avoids DMA-constrained lower memory.
 *
   The kernel is multiboot compliant. On entry, register EBX contains
   the physical address of the Multiboot Information structure. 
   We are in protected mode (not virtual 8086 mode). Paging is disabled. 
   All segment registers define 32-bit 4GB flat segments (virtual = physical). 
   Interrupts are disabled.
   The A20 gate is enabled. 
 */

#include <eros/arch/i486/asm.h>
#include <eros/arch/i486/target-asm.h>
#define ASM_FILE
#include <kerninc/multiboot.h>

#define REL16(x) .word EXT(x)-0f; 0:;
#define REL8(x)  .byte EXT(x)-0f; 0:;
#define jc_16_short     .byte 0x72;
#define jne_16_short    .byte 0x75;
#define jl_16_short     .byte 0x7c;
#define jg_16_short     .byte 0x7f;
#define jmp_16_short    .byte 0xeb;
                                                                                
#define HANG 0: jmp_16_short REL8(0b)

	.text
#ifdef __ELF__
GEXT(_start)
#endif
ENTRY(start)

	/* Throw something on the screen so we know we got here: */
	movl	$0x000b8000,%ecx
	movl	$0x07610743,(%ecx)	/* "Ca" */

	cmpl	$GRUB_MB_MAGIC2,%eax
0:	jne	0b	/* hang here if not multiboot-loaded */

	/* Throw something on the screen so we know we got here: */
	movl	$0x000b8004,%ecx
	movl	$0x07520770,(%ecx)	/* "pR" */

	movl	%ebx,EXT(MultibootInfoPtr)

	/* Check required flags. */
#define REQUIRED_FLAGS (GRUB_MB_INFO_BOOTDEV+GRUB_MB_INFO_MEM_MAP)
	movl	(%ebx),%eax	/* Multiboot flags */
	andl	$REQUIRED_FLAGS,%eax
	cmpl	$REQUIRED_FLAGS,%eax
0:	jne	0b	/* hang here if some flags are zero */

	/* Throw something on the screen so we know we got here: */
	movl	$0x000b8008,%ecx
	movl	$0x0753074F,(%ecx)	/* "OS" */

	/* Now set up the stack (for this CPU). */
	movl	$(EXT(kernelStack)+EROS_KSTACK_SIZE),%esp

	xorl	%ebp,%ebp	/* mark the end of the stack */
	
	/* 
	 * Now that we have a proper stack, go figure out
	 * the CPU type:
	 */	
	
	call	EXT(GetCpuType)
#if 0
	pushl	%ecx
/* Display CpuType */
	movl	$0x000b8008,%ecx
	cmpl	$9,%eax
	ja      0f
	addl	$0x07200730,%eax
	movl	%eax,(%ecx)
0:	
/* Display features */
	movl	$0x000b800c,%ecx
	movl	EXT(CpuFeatures),%eax
	rorl	$5,%eax
	andl	$0x1,%eax
	roll	$16,%eax
	addl	$0x07300735,%eax
	movl	%eax,(%ecx)
	
	popl	%ecx
#endif
	
	cmpl	$5,EXT(CpuType)
	jb	1f		/* don't call it if not supported */
	
	pushl	%eax
	pushl	%edx
	pushl	%ecx

#ifdef SINGLE_PIPE
	/* Inhibit the V pipe, just out of curiosity: */
	movl	$0xe,%ecx	/* TR12 */
	movl	$0x4,%eax
	xorl	%edx,%edx
	wrmsr
#endif

#ifdef OPTION_ALLOW_USER_RDTSC
	/* Enable use of the time stamp counter from user-mode 
	 * software: */
	movl	%cr4,%eax
	andl	$0xffffffB,%eax
	movl	%eax,%cr4
#endif
	
#ifdef ALLOW_RDPMC
	/* Note: This seems like a bad idea, because it opens
	   a huge covert channel. */
	/* Enable user mode access to PMC registers: */
	movl	%cr4,%eax
	orl     $0x100,%eax     /* Enable user mode rdpmc bit-8-cr4*/
	movl	%eax,%cr4
	
#if HAVE_PERFCTRS	/* disabled, because not all models have it */
	/* Zero PerfCtr0 */
	xorl %edx, %edx
	xorl %eax, %eax
	movl $0xC1, %ecx
	wrmsr
	
	/* Program PerfCtr0 to count user mode only, clock cycles */
	/* Count clock cycles in counter0 */
	movl $0x186, %ecx
	rdmsr
	andl  $0x00000000, %eax
	orl   $0x00510079, %eax  /* User mode events only & enable counters */
	wrmsr

	/* Zero PerfCtr1 */
	xorl %edx, %edx
	xorl %eax, %eax
	movl $0xC2, %ecx
	wrmsr

	/* Program PerfCtr1 to count kernel mode,segment register loads  */
	movl $0x187, %ecx
	rdmsr
	andl  $0x00000000, %eax
	orl   $0x00520006, %eax  /* Supervisor mode only & enable counters */
	wrmsr
#endif	
#endif
		
	popl	%ecx
	popl	%edx
	popl	%eax

1:	

  /* Set up the boot console by hand so that we can do kernel
   * diagnostics during startup.  Note that the boot console is output
   * only unless a kernel debugger is present, and we will enable
   * debugger input later after interrupts have been initialized.
   */
	call	EXT(kstream_InitStreams)

	movl $EXT(ImageStart),EXT(NPObDescr)
	call	EXT(main)

	/*
	 * Just in case anybody ever returns here:
	 */

	call	EXT(halt)

	/* Multiboot header */
	.align 4
	.long	GRUB_MB_MAGIC
	.long	GRUB_MB_PAGE_ALIGN + GRUB_MB_MEMORY_INFO
	.long	- GRUB_MB_MAGIC - GRUB_MB_PAGE_ALIGN - GRUB_MB_MEMORY_INFO
	
	.data
ENTRY(CpuType)	
	.long	0	/* 386, 486, 586, etc... */
ENTRY(CpuStepping)	
	.long	0	/* stepping ID */
ENTRY(CpuIdHi)
	.long	0	/* highest arg to CPUID */
ENTRY(CpuFeatures)
	.long	0	/* features */
ENTRY(CpuVendor)
	.space	20	/* CPU origin code */
	
	/*
	 * GetCpuType -- memorizes the result, so that we only have to
	 * do it the hard way at system startup.
	 */	
	.text
ENTRY(GetCpuType)
	pushl	%ebp
	movl	%esp,%ebp

	pushal
	
	movl	EXT(CpuType),%eax
	cmpl	$0,%eax
	jne     4f

	/*
	 * First time, get the info from the processor.
	 */
	
	/*
	 * Vendor is unknown until proven otherwise:
	 */
	movl	$0x6e6b6e55,EXT(CpuVendor)	/* "Unkn" */
	movl	$0x006e776f,EXT(CpuVendor)+4	/* "own\0" */
	/*
	 * Zero fill the rest so vendor string will be null terminated
	 */
	movl	$0,EXT(CpuVendor)+8
	movl	$0,EXT(CpuVendor)+12
	movl	$0,EXT(CpuVendor)+16

	/* It's a shit-for-brains part until we prove otherwise: */
	movl    $3, EXT(CpuType)
	
	/* Try to toggle alignment check flag; does not exist on 386. */
	pushfl
	popl	%eax
	movl	%eax,%ecx
	xorl	$0x40000,%eax	/* flip AC bit in EFLAGS */
	pushl	%eax
	popfl
	pushfl
	popl	%eax
	xorl	%ecx,%eax
	andl	$0x40000,%eax   /* see if it stayed changed */
	jz	4f		/* if not, it's a 386 */

	/* We are on at least a 486 */
	movl    $4, EXT(CpuType)
	
	pushl	%ecx		/* put the alignment bit back */
	popfl

	/* 
	 * Try to toggle the CPUID flag.  If this is a 486DX, 486SX, or 
	 * 487SX we will be unable to change it:
	 */
	
	movl %ecx,%eax
	xorl $0x200000,%eax	/* check ID flag */
	pushl %eax
	popfl
	pushfl
	popl %eax
	xorl %ecx,%eax
	andl $0x200000,%eax     /* see if it changed */
	jnz use_cpuid		/* has CPUID bit */

	/*
	 * FIX:	 The freebsd code checks for the Cyrix 486DLC here.
	 * We should probably do so too, but at the moment
	 * I'm too lazy, and I don't understand the "caching of the ISA 
	 * hole" issue.
	 */

	/* check for Cyrix 486DLC -- based on check routine  */
	/* documented in "Cx486SLC/e SMM Programmer's Guide" */
	/* I got this from the FreeBSD code, so don't blame me! */
	xorw	%dx,%dx
	cmpw	%dx,%dx			/* set flags to known state */
	pushfw
	popw	%cx			/* store flags in ecx */
	movw	$0xffff,%ax
	movw	$0x0004,%bx
	divw	%bx
	pushfw
	popw	%ax
	andw	$0x08d5,%ax		/* mask off important bits */
	andw	$0x08d5,%cx
	cmpw	%ax,%cx

	jnz	4f			/* if flags changed, Intel chip */

	/* It's Cyrix. */
	movl	$0x69727943,EXT(CpuVendor) /* store vendor string */
	movw	$0x0078,EXT(CpuVendor)+4

	/* 
	 * Start with guaranteed clean cache.  This came from FreeBSD.
	 * I'm not sure sure why they care,  but it will not hurt any.
	 */
	invd
	jmp	4f
	
use_cpuid:
	/* 
	 * At this point we are looking at something that answers to
	 * the CPUID operation.  Now it's easy.  Ask the part what
	 * it is, at which point we really don't know anything more than 
	 * we did before :-)
	 */

	xorl	%eax,%eax
	.byte	0x0f,0xa2			/* cpuid 0*/
	movl	%eax,EXT(CpuIdHi)		/* highest CPUID question */
	movl	%ebx,EXT(CpuVendor)		/* vendor string */
	movl	%edx,EXT(CpuVendor)+4
	movl	%ecx,EXT(CpuVendor)+8

	/*
	 * Now ask for CPU id and features:
	 */

	movl	$1,%eax
	.byte	0x0f,0xa2			/* cpuid 1 */
	movl	%eax,EXT(CpuStepping)		/* CPU stepping */
	movl	%edx,EXT(CpuFeatures)		/* CPU features */

	/*
	 * Extract the CPU type:
	 */
	rorl	$8,%eax
	andl	$15,%eax
	movl	%eax,EXT(CpuType)
	
4:	
	popal

	/*
	 * Return CPU Type as return value
	 */
	movl	EXT(CpuType),%eax
	
	leave
	ret
