	.file	"lostart.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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
 * On entry, the kernel is sitting at physical 0x100000 (KERNPBASE),
 * which puts it in upper memory and avoids DMA-constrained lower memory.
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
	
 * A single kernel argument, which is a pointer to the BootInfo structure, 
 * is sitting on a temporary stack in low memory.  The BootInfo structure 
 * sits in a region of physical memory that has been reserved by the
 * bootstrap code for its exclusive use, along with all of the other
 * bootstrap-related data structures.
 *
 * While the GDT set up by the bootstrap loader is still valid and 
 * addressable, the kernel should set up an official one as quickly as 
 * possible.  The one created by the bootstrap loaded lives someplace
 * that the kernel is almost certainly just about to reclaim.
 */

#include <eros/i486/asm.h>
#include <eros/i486/target-asm.h>

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

	/*
	 * Zero the bss section.  If any of BSS got put in the data section,
	 * that was already zeroed by the compiler, but the BSS section needs
	 * to be zapped by us.
	 *
	 * It is very important to do this BEFORE we run from our own stack!
	 * It is also important that the BootInfo structure passed by the 
	 * bootstrap reside in a "safe" place.
	 */

	cld

	movl	$ EXT(edata), %edi	/* destination */
	movl	$ EXT(end), %ecx	/* count */
	subl	$ EXT(edata), %ecx
	mov	$0x0, %eax		/* value */

	rep
	stosb

	/*
	 * Throw something on the screen so we know we got here:
	 */

	movl	$0x000b8000,%eax
	movl	$0x07520745,(%eax)	/* "ER" */

	/*
	 * Copy the sysinfo pointer to a safe place before we
	 * rearrange things too drastically.  If the address
	 * looks odd, keep in mind that we got here via a branch,
	 * not through the usual and customary calling
	 * convention.
	 */

	movl	(%esp),%eax
	movl	%eax,EXT(BootInfoPtr)

	/* Now set up the stack for the first CPU (CPU0). This lives
	 * just above the /end/. Stacks for other CPUs will be set up
	 * by the C code.
	 */
	movl	$ EXT(end), %esp
	/* Add one page to /end/, because the page after end is used by the
	   master kernel mapping directory: */
	addl	$EROS_PAGE_SIZE,%esp
	/* Now round up to the next legal CPU stack boundary for the CPU0 
	   stack */
	addl	$EROS_KSTACK_SIZE,%esp
	sub	$1,%esp
	andl	$EROS_KSTACK_MASK,%esp

	/*
	 * %esp now contains a pointer to a properly-aligned BOTTOM of the 
	 * stack. Add another EROS_KSTACK_SIZE to that so we actually have
	 * a stack.
	 */
	addl	$EROS_KSTACK_SIZE,%esp
	
#if 0
	/*
	 * Reserve some room at the top of this stack so that when the 
	 * kernel thread gets initialized it will not overwrite the
	 * __main return address.
	 */

	subl	$32,%ESP
#endif	
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
	
	/*
	 * Put up the rest of "EROS" before we call main.
	 */

	movl	$0x000b8004,%eax
	movl	$0x0753074F,(%eax)	/* "OS" */

#if __GNUC__ <= 2
	/* As of GCC3 the handling of constructors changed, and we weren't
	 * supporting kernel C++ code anyway, so bypass the call to __main()
	 * when building with the new compiler.
	 */
#ifdef __ELF__
	/*
	 * Fix: Under a.out format the g++ compiler rearranges the 'main'
	 * function to call __main as it's first action.  Under ELF, the 
	 * '.init' section is used to achieve the same result without the
	 * need to alter code in main(). 
	 *
	 * Undoubtedly, there is some officially approved way to run
	 * the code in the init section, but I don't know what it is
	 * and I don't have time to figure it out.  Instead, I just
	 * call __main by hand here.
	 *
	 * Beware - global constructors run in a VERY strange context.
	 */
	call	EXT(__main)
#endif
#endif /* __GNUC__ <= 2 */
	
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
	
	/* Zero PerfCtr0 */
	xorl %edx, %edx
	xorl %eax, %eax
	movl $0xC1, %ecx
	wrmsr	/* AMD Athlon XP-M crashes here */
	
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
		
	popl	%ecx
	popl	%edx
	popl	%eax

1:	
	/*
	 * Congratulations. We now have a stack.
 	 * Push the arguments to main and let's get going!
	 */

	pushl	$1	/* isboot = 1 */

	call	EXT(main)

	/*
	 * Just in case anybody ever returns here:
	 */

	call	EXT(halt)


	
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
	 * GetCpuType -- memoizes the result, so that we only have to
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

#if 0
	/* 
	 * We don't set a special value for cyrix - just the 
	 * vendor string 
	 */
	movl	$CPU_486DLC,_cpu-KERNBASE /* set CPU value for Cyrix */
#endif
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
#if 0	
	cmpl	$5,%eax
	jae	1f

	/* less than Pentium; must be 486 */
	movl	$CPU_486,_cpu-KERNBASE
	jmp	4f

1:	movl	$CPU_586,_cpu-KERNBASE
2:
#endif
	
	
4:	
	popal

	/*
	 * Return CPU Type as return value
	 */
	movl	EXT(CpuType),%eax
	
	leave
	ret
	
ENTRY(GetFlags)
	pushl	%ebp
	movl	%esp,%ebp
	pushf
	popl	%eax
	leave
	ret

ENTRY(SetFlags)
	pushl	%ebp
	movl	%esp,%ebp
	movl	0x8(%ebp),%eax
	pushl	%eax
	popf
	leave
	ret

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
	movl	$0x000b8000,%eax
	movl	$0x07620741,(%eax)
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

#if 1
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
