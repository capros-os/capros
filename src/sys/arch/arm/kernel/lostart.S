/*
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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

#include <eros/arch/arm/asm.h>
#include <eros/arch/arm/target-asm.h>
#include <eros/arch/arm/mach-ep93xx/ep9315.h>
#include <arch-kerninc/kern-target-asm.h>

//#define EARLY_CONSOLE 1

#define PUTC(c) \
	mov r0,c ; str r0,[r10] ; \
//	mov r0,#0x1000000 ; 0: subs r0,r0,#1 ; bne 0b

	.text
	ENTRY(_start)
#define REDBOOT
#ifndef REDBOOT
	.ascii "CRUS"	// magic header for Boot ROM
#else
	msr cpsr_c,#0xd3	// disable IRQ, FIQ, enter supervisor mode
	ldr r10,=(APB_PA + UART1_APB_OFS)	// for PUTC
	PUTC(#65)	// 'A
// Redboot starts us with MMU enabled, disable it now.
// FIXME: On EDB9315A, phys mem starts at 0xc0000000 and Redboot's map
// is not trivial. 
	mrc p15,0,r8,c1,c0,0	// read control register 1
	bic r8,r8,#0x1200	// disenable ICache, clear R bit
	orr r8,r8,#0x2100	// high exception vectors, set S bit
	bic r8,r8,#0x007	// disenable DCache, alignmt fault, MMU
	mcr p15,0,r8,c1,c0,0	// write control register 1
// Redboot leaves Ethernet controller active. Reset it.
	ldr r1,=(AHB_PA+EMAC_AHB_OFS)	// 0x80010000
	mov r0,#1		// RESET bit
	str r0,[r1,#0x20]	// SelfCtl register
#endif
/* Enter here from boot loader. */
/* When we get here from the boot ROM, we are in Supervisor mode,
   in ARM (not Thumb) execution state, with IRQ and FIQ both disabled. */
getpc:	mov r6,pc	// record the pc
	sub r6,r6,#(getpc+8-_start)
	// r6 has phys addr of start of .text section.
/* If started by the boot ROM, we are running out of flash memory.
   Presumably this is at FlashMemPA, but rather than assume that,
   we use the value in the PC. 

   If started by Redboot, we are running out of RAM, presumably at
   0x00200000, but rather than assume that,
   we use the value in the PC.
   The .data section is at 0x00100000, and the imgdata is at 0x00300000. */

/* We are linked to run at KTextVA, so until we copy the code to RAM
   and set up the MMU,
   all code must be position independent, and we must not
   use any global variables. */

/* Copy the kernel text section (where we are running)
   to physical address KTextPA in RAM.
   Text section here includes rodata. */
	ldr r4,=KTextPA	// Copy to here
	ldr r5,=_etext
	ldr r3,=_start
	sub r5,r5,r3	// size of text
	add r5,r5,r4	// PA of end of new location for text
	sub r8,r4,r6	// distance we are moving the text section

1:	ldr r7,[r6],#4
	str r7,[r4],#4
	cmp r4,r5
	blo 1b

/* Jump to this code in RAM. */
	add pc,pc,r8
	nop

	PUTC(#66)	// 'B

/* If using Redboot, the kernel data section is loaded at physical 0x00100000,
   which is where we want it. */

/* Set the kernel stack pointer. */
	ldr sp,=(kstackBot)
	ldr r0,=(-KTextVA+KTextPA)	// convert to physical address
	add sp,sp,r0
	str sp,[sp,#-4]	// test store

/* Zero bss. */
	ldr r4,=_edata	// beg of bss section (must be on word boundary)
	ldr r5,=_end	// end of bss section
	add r4,r4,r0	// convert to physical address
	add r5,r5,r0	// convert to physical address
	mov r0,#0
1:	cmp r4,r5
	bhs 2f
	str r0,[r4],#4
	b 1b

2:

#ifndef OPTION_NO_MMU
	ldr r3,=(0x4000-1)
	add r4,r4,r3	// round up to 16KB boundary
	bic r4,r4,r3	// ...
// TODO: it would be nice to use the space we skipped over for pages or CPTs.
	add r4,r4,#0x4000	// reserve 16KB for the Null FLPT
	mov r5,r4	// FCSE First Level Page Table
	add r4,r4,#0x4000	// size of FLPT
	mov r7,r4	// Coarse Page Table for exception vectors
	add r4,r4,#0x400	// size of coarse page table
/* At this point (all are physical addresses):
	r4 - end of CPT for exception vectors
	r5 - FLPT
	r7 - CPT for exception vectors
 */

	PUTC(#67)	// 'C

/* Clear the FLPT and CPT */
	mov r0,#0
	mov r1,r5
1:	str r0,[r1],#4
	cmp r1,r4
	blo 1b

/* Map whole sections (1MB). */
	ldr r3,=(1024*1024)	// size of a section
/* Set up device registers in FLPT. */
// AP=0b01 domain=0 C=0 B=0 section descriptor
	ldr r0,=(0x00000412 + 0x80000000)
	ldr r1,=(32*1024*1024)	// size of memory to map
	add r1,r1,r0
	ldr r2,=(DeviceRegsVA >> (20-2))
	add r2,r2,r5
1:	str r0,[r2], #4
	add r0,r0,r3
	cmp r0,r1
	bne 1b

/* Map flash memory in FLPT. */
// AP=0b01 domain=0 C=0 B=0 section descriptor
	ldr r0,=(0x00000412 + FlashMemPA)
	ldr r1,=(PhysMapVA - FlashMemVA)	// size of memory to map
	add r1,r1,r0
	ldr r2,=(FlashMemVA >> (20-2))
	add r2,r2,r5
1:	str r0,[r2], #4
	add r0,r0,r3
	cmp r0,r1
	bne 1b

#ifdef OPTION_WRITEBACK
#define WriteBackOption 0x4 /* PTE_BUFFERABLE for write-back */
#else
#define WriteBackOption 0x0 /* write-through */
#endif
	PUTC(#68)	// 'D

/* Set up physical memory map in FLPT. */
// AP=0b01 domain=0 C=1 B=0 section descriptor
// Always use write-through, because this is mainly used for mapping tables.
	ldr r0,=(0x0000041a)
/* TODO: Would we benefit by making this not cacheable or not buffered? */
/* TODO: Need to fix this to handle memory that isn't contiguous with 
   address zero. */
/* TODO: Avoid mapping memory that doesn't exist. */
	ldr r1,=0x30000000	// size of phys mem map
	add r1,r1,r0
	ldr r2,=(PhysMapVA >> (20-2))
	add r2,r2,r5
1:	str r0,[r2], #4
	add r0,r0,r3
	cmp r0,r1
	bne 1b

/* Set up kernel code in FLPT. */
/* Assume less than 1MB. */
// AP=0b00 domain=0 C=1 B=0 section descriptor
	ldr r0,=(0x0000001a + KTextPA)
	ldr r1,=(KTextVA >> (20-2))
	str r0,[r5, r1]
/* Temporarily map kernel code at KTextPA also so we can continue executing
   when we enable the MMU. 
   This map will be cleared later in map_HeapInit. */
	ldr r0,=(0x0000041a + KTextPA)	// map read-write
	ldr r1,=(KTextPA >> (20-2))
	str r0,[r5, r1]

/* Set up kernel data and bss in FLPT. */
/* Assume less than 1MB. */
// AP=0b01 domain=0 C=1 B=WriteBackOption section descriptor
	ldr r0,=(0x0000041a + KTextPA + 0x00100000 + WriteBackOption)
	ldr r1,=((KTextVA + 0x00100000) >> (20-2))
	str r0,[r5, r1]

/* Set up CPT for exception vectors. */
// domain=0 CPT descriptor
	ldr r0,=0x11
	add r0,r0,r7	// PA of CPT
	ldr r1,=(0xfff00000 >> (20-2))
	str r0,[r5, r1]
/* Set up page for exception vectors, referring to physical address zero. */
/* Note, the vectors aren't there yet. They will be written below. */
	ldr r0,=(0x9 + KTextPA)	// AP=0b00, C=1, B=0
	str r0,[r7, #(0xf0 << 2)]

	PUTC(#69)	// 'E

/* Enable the MMU. */
	mcr p15,0,r5,c2,c0,0	// write TTBR
	ldr r0,=0x55555555	// all client access
	mcr p15,0,r0,c3,c0,0	// write DACR
	mov r0,#0
	mcr p15,0,r0,c8,c7,0	// invalidate TLBs
	mcr p15,0,r0,c10,c0,0	// clear D TLB lockdown
	mcr p15,0,r0,c10,c0,1	// clear I TLB lockdown
	mcr p15,0,r0,c7,c7,0	// invalidate ICache and DCache
#endif // OPTION_NO_MMU

	PUTC(#70)	// 'F

#ifdef EARLY_CONSOLE
#error	/* Don't use SerialStream before enabling the MMU, because it
	accesses device registers at their virtual address. */
	bl SerialStream_Init	// this doesn't use data or bss and is PIC
	mov r0,#'2
	bl SerialStream_Put
#endif

#ifndef OPTION_NO_MMU	// no_mmu also means no cache
	mrc p15,0,r8,c1,c0,0	// read control register 1
	bic r8,r8,#0x200	// clear R bit
	orr r8,r8,#0x3100  // high exception vectors, enable ICache, set S bit
	orr r8,r8,#0x007	// enable DCache, alignmt fault, MMU
	mcr p15,0,r8,c1,c0,0	// write control register 1
#endif // OPTION_NO_MMU

	ldr sp,=(kstackBot)

// Go to new code location.
	add pc,pc,#(KTextVA - KTextPA)
	nop

#ifdef EARLY_CONSOLE
	mov r0,#'C
	bl SerialStream_Put
#endif

#ifndef OPTION_NO_MMU
	ldr r0,=FLPT_FCSEPA
	str r5,[r0]	// save ptr to FLPT
#endif // OPTION_NO_MMU

#ifdef EARLY_CONSOLE
	mov r0,#'7
	bl SerialStream_Put
#endif

	bl kstream_InitStreams

#if 1
	/* Print some interesting values. */
#if 1
	ldr r4,=(APB_VA+SYSCON_APB_OFS)
	ldr r1,[r4,#0x9c]	// SysCfg
	ldr r2,[r4,#0x80]	// DeviceCfg
	// mrc p15,0,r2,c9,c0,0	// read control register 9
	mrc p15,0,r3,c10,c0,0	// read control register 10
#else
	ldr r4,=(AHB_VA+SDRAM_AHB_OFS)	// SDRAM Mem Controller regs
#if 1
	ldr r1,[r4,#0x04]
	ldr r2,[r4,#0x08]
	ldr r3,[r4,#0x0c]
#else
	ldr r1,[r4,#0x10]
	ldr r2,[r4,#0x14]
	ldr r3,[r4,#0x18]
#endif
#endif
	ldr r0,=print_values_string
	bl printf
	b 1f
print_values_string: .asciz "0x%x 0x%08x 0x%08x\n"
	.balign 4
1:
#endif

/* Configure flash memory for 100MHz HCLK. */
/* Not sure if these values are optimal, but should work. */
	ldr r4,=(AHB_VA+SMC_AHB_OFS)	// Static Mem Controller regs
	ldr r0,=0x200075c2
	str r0,[r4,#0x18]	// config bank 6

/* Configure SDRAM memory for 100MHz HCLK. */
/* The SDRAM on the EDB9315 requires 64ms refresh. */
/* The EP9315 Users Guide is unclear; it says the value of RefrshTimr 
is in units of the HCLK period (10 ns in this case).
But the example uses units of 1000 HCLK periods, so follow that. */
	ldr r4,=(AHB_VA+SDRAM_AHB_OFS)	// SDRAM Mem Controller regs
	ldr r0,=(64000/10)	
	str r0,[r4,#0x8]	// RefrshTimr

/*
Configure clocks.

The Cirrus EP93xx on reset runs the processor at 14.7456 MHz.
We want to crank it up to the max, which is 200 MHz except for the EP9301.
 */
	ldr r2,=(APB_VA+SYSCON_APB_OFS)
	ldr r1,[r2]	// read PwrSts register
	and r0,r1,#0xff000000	// chipman field (manufacturer)
	cmp r0,#0x43000000	// Cirrus
	bne NoClockConfig		// others not supported
	and r0,r1,#0x00ff0000	// chipid field
	cmp r0,#0x00200000
	bne NoClockConfig	// others not supported

	ldr r0,=EP93xxMsg
	bl printf

	/* Changing clocks messes up the UART, so let it finish. */
	bl SerialStream_Flush

#if 0
	b NoClockConfig
#endif

/*
Configure PLL1 to output:
  14.7456 MHz * 21 * (1/24) * 31 * (1/2) = 199.987 MHz
  Use that as FCLK.
  Divide by 2 to get 100 MHz for HCLK (aka BCLK).
  Divide that by 2 to get 50 MHz for PCLK.
 */
	ldr r2,=(APB_VA+SYSCON_APB_OFS)
	ldr r0,=0x0095a3d7	// for 200 MHz
//	ldr r0,=0x0099a3d7	// for 100 MHz
//	ldr r0,=0x008398e7	// reset value + nBYP1
	str r0,[r2,#0x20]	// ClkSet1 register
// After writing to ClkSet1, must flush the instruction pipeline:
	nop
	nop
	nop
	nop
	nop

/* Change from FastBus mode to Asynchronous mode.
   This will run the CPU at the faster FCLK instead of HCLK/BCLK.
   (Not sure whether Synchronous mode is better or possible.) */
	mrc p15,0,r0,c1,c0,0	// read CP15 control reg 1
	orr r0,r0,#0xc0000000	// Asynchronous mode
	mcr p15,0,r0,c1,c0,0	// write CP15 control reg 1

NoClockConfig:
/* Set up exception vectors.
   These will overwrite the first few instructions of this file. */
	ldr r2,=(KTextPA + PhysMapVA)	
		// VA of page containing exception vectors
	ldr r3,=0xffff0008	// VA at which vector will execute,
			// + 8 for value pc will have when it executes
	bl setVector
	.word ResetExceptionHandler
	bl setVector
	.word UndefinedExceptionHandler
	bl setVector
	.word SWIExceptionHandler
	bl setVector
	.word PrefetchAbortExceptionHandler
	bl setVector
	.word DataAbortExceptionHandler
	bl setVector
	.word UnusedExceptionHandler
	bl setVector
	.word IRQExceptionHandler
	bl setVector
	.word FIQExceptionHandler

// Drain write buffer so exception instructions will be seen.
	mcr p15,0,r0,c7,c10,4

//	bl eraseFlash

#ifdef REDBOOT
	ldr r0,=PhysMapVA
#else
	ldr r0,=(FlashMemVA-FlashMemPA)
#endif
	bl GrubEmul
#if 1
	bl main		// does not return
#else
	mrc p15,0,r0,c1,c0	// get CP15 reg 1 for display
	bl mainTemp
//	bl invmon

	mov r0,#'9
	bl SerialStream_Put

#if 1
// Blink the green LED.
	ldr r3,=(APB_VA+GPIO_APB_OFS+0x20)	// Port E data register
	mov r1,#0x3
	mov r2,#7
2:	mov r0,r2
1:	str r1,[r3]
	subs r0,r0,#1
	bne 1b
	eor r1,r1,#1
	add r2, r2, r2, LSR #2	// slower next time
	b 2b
#endif
#endif

/* Procedure to store a branch to the address in r1. */
setVector:
	ldr r1,[lr]	// get data following call
	sub r1,r1,r3
	mov r1,r1, ASR #2	// high 9 bits must be the same now
	bic r1,r1,#0xff000000
	orr r1,r1,#0xea000000	// branch instruction
	str r1,[r2], #4
	add r3,r3,#4
	add pc,lr,#4	// return, skipping data word

ENTRY(halt)
	b .

EP93xxMsg: .asciz "Recognized EP93xx\n"

	.data		// beginning of data section
	ENTRY(kstack)
	.long 0
	. = kstack + EROS_KSTACK_SIZE
	.balign CACHE_LINE_SIZE // align on cache line for performance
	ENTRY(kstackBot)

