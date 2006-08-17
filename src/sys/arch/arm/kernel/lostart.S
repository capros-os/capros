/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <eros/arch/arm/asm.h>
#include <eros/arch/arm/target-asm.h>
#include <arch-kerninc/kern-target-asm.h>

	.text
	ENTRY(_start)
//#define REDBOOT
#ifndef REDBOOT
	.ascii "CRUS"	// magic header for Boot ROM
/* Enter here from boot loader. */
/* When we get here from the boot ROM, we are in Supervisor mode,
   in ARM (not Thumb) execution state, with IRQ and FIQ both disabled. */
/* We start running here in the Flash memory.
   We are linked to run at KTextVA, so until we copy the code to RAM
   and set up the MMU,
   all code must be position independent, and we must not
   use any global variables. */

/*
The kernel text section is loaded into address FlashMemPA in the ROM.
We will copy it to physical address KTextPA
and map it at MVA KTextVA.
 */
/* Check that KTextPackedSize isn't too small. */
	ldr r0,=_etext
	ldr r1,=_start
	sub r0,r0,r1
	cmp r0,#KTextPackedSize
	blo 1f
	b .	/* hang here if KTextPackedSize is too small */
1:
/* Copy text section to RAM. */
	ldr r6,=FlashMemPA	// Copy from here
	ldr r4,=KTextPA	// Copy to here
	ldr r5,=(FlashMemPA + KTextPackedSize)	// endpoint in ROM
1:	ldr r7,[r6],#4
	str r7,[r4],#4
	cmp r6,r5
	blo 1b

/* Jump to this code in RAM. */
	add pc,pc,#(KTextPA-FlashMemPA)
	nop

/*
The kernel data section is loaded into address 
(FlashMemPA + KTextPackedSize) in the ROM.
We will copy it to physical address KTextPA + 1MB
and map it at MVA KTextVA + 1MB.
 */
/* Copy data section to RAM. */
	ldr r3,=(1024*1024 - 1)
	add r4,r4,r3	// round up to section boundary
	bic r4,r4,r3	// ...
	ldr r5,=_edata	// data section (must be on word boundary)
	ldr r0,=__data_start
	sub r5,r5,r0	// size of data section
	add r5,r5,r4
1:	ldr r7,[r6],#4
	str r7,[r4],#4
	cmp r4,r5
	blo 1b
#else
	ldr r4,=_edata	// this is wrong
#endif

/* Set the kernel stack pointer. */
	ldr sp,=(kstackBot)
	ldr r0,=(-KTextVA+KTextPA)	// convert to physical address
	add sp,sp,r0
	str sp,[sp,#-4]	// test store

/* Zero bss. */
	ldr r5,=_end	// copy the text
	ldr r0,=_edata
	add r5,r5,r4
	sub r5,r5,r0
	mov r0,#0
1:	cmp r4,r5
	bhs 2f
	str r0,[r4],#4
	b 1b

2:

//#define EARLY_CONSOLE 1
#ifdef EARLY_CONSOLE
	bl SerialStream_Init	// this doesn't use data or bss and is PIC
	mov r0,#'2
	bl SerialStream_Put
#endif

	ldr r3,=(0x4000-1)
	add r4,r4,r3	// round up to 16KB boundary
	bic r4,r4,r3	// ...
// TODO: it would be nice to use the space we skipped over for pages or CPTs.
	mov r5,r4	// First Level Page Table
	ldr r0,=0x4000	// size of FLPT
	add r4,r4,r0
	mov r6,r4	// Coarse Page Table for exception vectors
	ldr r7,=0x400	// size of coarse page table
	add r4,r4,r7	// 
/* At this point:
	r4 - end of CPT for exception vectors, beginning of free RAM (used?)
	r5 - FLPT
	r6 - CPT for exception vectors
 */
/* Clear the FLPT and CPT */
	mov r0,#0
	mov r1,r5
1:	str r0,[r1],#4
	cmp r1,r4
	blo 1b

#ifdef EARLY_CONSOLE
	mov r0,#'4
	bl SerialStream_Put
#endif

/* Map whole sections (1MB). */
	ldr r3,=(1024*1024)	// size of a section
/* Set up device registers in FLPT. */
// AP=0b01 domain=0 C=0 B=0 section descriptor
	ldr r0,=(0x00000412 + 0x80000000)
	ldr r1,=(32*1024*1024)	// size of memory to map
	add r1,r1,r0
	ldr r2,=(DeviceRegsVA >> (20-2))
	add r2,r2,r5
// Temporarily map at the usual place too.
	ldr r8,=(0x80000000 >> (20-2))
	add r8,r8,r5
1:	str r0,[r2], #4
	str r0,[r8], #4
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

#define WriteBackOption 0x0 /* 0 for write-through, 4 for write-back */
/* Set up physical memory map in FLPT. */
// AP=0b01 domain=0 C=1 B=WriteBackOption section descriptor
	ldr r0,=(0x0000041a+WriteBackOption)
/* TODO: Would we benefit by making this not cacheable or not buffered? */
/* TODO: Need to fix this to handle memory that isn't contiguous with 
   address zero. */
/* TODO: Need to fix this to get actual memory size. */
	ldr r1,=(64*1024*1024)	// size of memory on EDB9315
	add r1,r1,r0
	ldr r2,=(PhysMapVA >> (20-2))
	add r2,r2,r5
1:	str r0,[r2], #4
	add r0,r0,r3
	cmp r0,r1
	bne 1b

/* Set up kernel code in FLPT. */
/* Assume less than 1MB. */
// AP=0b00 domain=0 C=1 B=WriteBackOption section descriptor
	ldr r0,=(0x0000001a + KTextPA + WriteBackOption)
	ldr r1,=(KTextVA >> (20-2))
	str r0,[r5, r1]
/* Temporarily map kernel code at KTextPA also so we can continue executing
   when we enable the MMU. */
	ldr r0,=(0x0000041a + KTextPA + WriteBackOption)	// map read-write
	ldr r1,=(KTextPA >> (20-2))
	str r0,[r5, r1]
#if 0
/* For debugging, temporarily map kernel data and bss in low memory. */
	ldr r0,=(0x0000041a + KTextPA + 0x00100000 + WriteBackOption)	// map read-write
	ldr r1,=((KTextPA + 0x00100000) >> (20-2))
	str r0,[r5, r1]
#endif

/* Set up kernel data and bss in FLPT. */
/* Assume less than 1MB. */
// AP=0b01 domain=0 C=1 B=WriteBackOption section descriptor
	ldr r0,=(0x0000041a + KTextPA + 0x00100000 + WriteBackOption)
	ldr r1,=((KTextVA + 0x00100000) >> (20-2))
	str r0,[r5, r1]

/* Set up CPT for exception vectors. */
// domain=0 CPT descriptor
	ldr r0,=0x11
	add r0,r0,r6	// PA of CPT
	ldr r1,=(0xfff00000 >> (20-2))
	str r0,[r5, r1]
/* Set up page for exception vectors, referring to physical address zero. */
/* Note, the vectors aren't there yet. They will be written below. */
	ldr r0,=(0x9 + KTextPA)	// AP=0b00, C=1, B=0
	str r0,[r6, #(0xf0 << 2)]

/* Enable the MMU. */
	mcr p15,0,r5,c2,c0,0	// write TTBR
	ldr r0,=0xffffffff	// all manager access
// TODO: should use client
	mcr p15,0,r0,c3,c0,0	// write DACR
	mov r0,#0
	mcr p15,0,r0,c8,c7,0	// invalidate TLBs
	mcr p15,0,r0,c10,c0,0	// clear D TLB lockdown
	mcr p15,0,r0,c10,c0,1	// clear I TLB lockdown
	mcr p15,0,r0,c7,c7,0	// invalidate ICache and DCache

#ifdef EARLY_CONSOLE
	mov r0,#'6
	bl SerialStream_Put
#endif

	mrc p15,0,r8,c1,c0,0	// read control register 1
	bic r8,r8,#0x200	// clear R bit
	orr r8,r8,#0x3100  // high exception vectors, enable ICache, set S bit
	orr r8,r8,#0x007	// enable DCache, alignmt fault, MMU
	mcr p15,0,r8,c1,c0,0	// write control register 1

	ldr sp,=(kstackBot)

// Go to new code location.
	add pc,pc,#(KTextVA - KTextPA)
	nop

#ifdef EARLY_CONSOLE
	mov r0,#'C
	bl SerialStream_Put
#endif

	ldr r0,=(kernelStackBot)	// initialize kernelStackBot
	str sp,[r0]

	ldr r0,=FLPT_FCSEPA
	str r5,[r0]	// save ptr to FLPT

#ifdef EARLY_CONSOLE
	mov r0,#'7
	bl SerialStream_Put
#endif

	bl kstream_InitStreams

#if 1
	/* Print some interesting values. */
#if 1
	ldr r4,=0x80930000
	ldr r1,[r4,#0x20]
#else
	ldr r4,=0x80080000	// Static Mem Controller regs
	ldr r1,[r4,#0x00]
	ldr r2,[r4,#0x18]
	ldr r3,[r4,#0x18]
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
	ldr r4,=0x80080000	// Static Mem Controller regs
	ldr r0,=0x200075c2
	str r0,[r4,#0x18]	// config bank 6

/*
Configure clocks.

The Cirrus EP93xx on reset runs the processor at 14.7456 MHz.
We want to crank it up to the max, which is 200 MHz except for the EP9301.
 */
	ldr r2,=0x80930000
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
	ldr r2,=0x80930000
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
	ldr r3,=0x80840020	// Port E data register
	mov r1,#0x3
	mov r2,#7
loop:	mov r0,r2
inner:	str r1,[r3]
	subs r0,r0,#1
	bne inner
	eor r1,r1,#1
	add r2, r2, r2, LSR #2	// slower next time
	b loop
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

