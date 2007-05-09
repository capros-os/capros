/*
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */
	
#include <eros/arch/arm/asm.h>

#ifndef NDEBUG
// Optional debugging check:
#define DISABLE_DEPTH_CHECK
#endif

#define MASK_CPSR_IRQDisable        0x00000080

	.text
/* Assorted short assembler routines,
   not performance-critical enough to warrant declaring inline. */

	ENTRY(irq_DISABLE)
	ldr r1,=irq_DisableDepth
	/* Disable BEFORE updating irq_DisableDepth. */
	mrs r0,cpsr
	orr r2,r0,#MASK_CPSR_IRQDisable	/* may already be set */
	msr cpsr_c,r2
#ifdef DISABLE_DEPTH_CHECK
	tst r0,#MASK_CPSR_IRQDisable
	bne 1f
	/* IRQ is enabled */
	ldr r2,[r1]	// irq_DisableDepth must be zero
	cmp r2,#0
	beq 1f
	ldr r0,=disabMsg
	mov r1,r2
	bl printf
	b halt
disabMsg: .asciz "irq_DISABLE: IRQ enab and depth=%d "
	.balign 4
1:
#endif
	ldr r2,[r1]
	add r2,r2,#1
	str r2,[r1]
	mov pc,lr

	ENTRY(irq_ENABLE)
	ldr r1,=irq_DisableDepth
	/* Update irq_DisableDepth BEFORE re-enabling. */
	/* N.B. Instruction order is thus to avoid pipeline interlocks. */
	mrs r0,cpsr
	ldr r2,[r1]
#ifdef DISABLE_DEPTH_CHECK
	tst r0,#MASK_CPSR_IRQDisable
	bne 1f
	/* IRQ is already enabled */
	ldr r0,=enabMsg
	mov r1,r2
	bl printf
	b halt

1:	cmp r2,#0
	bne 1f
	ldr r0,=ovfMsg
	mov r1,r2
	bl printf
	b halt
enabMsg: .asciz "irq_ENABLE: already enab, and depth=%d "
ovfMsg: .asciz "irq_ENABLE: depth=%d, overflowing"
	.balign 4
1:
#endif
	bic r0,r0,#MASK_CPSR_IRQDisable	/* in case we need to enable */
	subs r2,r2,#1
	str r2,[r1]
	msreq cpsr_c,r0
	mov pc,lr

	ENTRY(mach_FlushBothTLBs)
	mov r0,#0
	mcr p15,0,r0,c8,c7,0
	ldr r1,=PteZapped
	strb r0,[r1]	// clear the flag
	mov pc,lr

	ENTRY(mach_ReadCacheType)
	mrc p15,0,r0,c0,c0,1	// read cache type register to r0
	mov pc,lr

	ENTRY(mach_FlushTLBsCaches)
	mov r0,#0
	mcr p15,0,r0,c8,c7,0
	ldr r1,=PteZapped
	strb r0,[r1]	// clear the flag
	// fall into mach_FlushBothCaches

/* "Flush" here means "clean and invalidate".
   However, since we never use writeback, we never have to clean. */
	ENTRY(mach_FlushBothCaches)
#if 0	// if we *were* to use writeback, here's how to clean:
	ldr r1,=cacheSetIndexIncrement
	ldr r2,=cacheSetIndexCarry
	ldr r1,[r1]
	ldr r2,[r2]
	mov r0,#0
0:	mcr p15,0,r0,c7,c14,2	// clean and invalidate data cache
	add r0,r0,r1	// add increment for next set
	add r0,r0,r2	// propagate any carry to index field
	bics r0,r0,r2	// prepare for next carry
	bne 0b
#endif
	mov r0,#0
	mcr p15,0,r0,c7,c7,0	// invalidate entire cache(s)
	ldr r1,=flushCache
	strb r0,[r1]	// clear the flag
	mov pc,lr

	ENTRY(mach_LoadTTBR)
	mcr p15,0,r0,c2,c0	// load translation table base register
	mov pc,lr

	ENTRY(mach_LoadPID)
	mcr p15,0,r0,c13,c0	// load Process ID register
	mov pc,lr

	ENTRY(mach_LoadDACR)
	mcr p15,0,r0,c3,c0	// load domain access control register, DACR
	mov pc,lr

/* LoadWordFromUserSpace
   Returns true iff successful. */
	ENTRY(LoadWordFromUserSpace)
/* The following instruction will fault if the user does not have access.
   The fault handler recognizes this instruction address. */
	ENTRY(LoadWordFromUserSpaceInstr)
	ldrt r2,[r0]
	str r2,[r1]
	mov r0,#1	/* successful, return true */
	mov pc,lr

/* If the above access failed, we get here. */
	ENTRY(LoadWordFromUserSpaceReturn)
	mov r0,#0	/* return false */
	mov pc,lr

	ENTRY(act_Yield)
	ldr sp,=(kstackBot)	/* Empty the stack! */
	b act_HandleYieldEntry

	ENTRY(Debugger)
	sub sp,sp,#80	// space for a register save area
/*
   Savearea has:
     CPSR
     r15
     r0-r14
     three more words
 */
	str r0,[sp,#8]
	str r14,[sp,#4]	// pc to return to
	add r0,sp,#8
	stmib r0!,{r1-r12}
	mrs r1,cpsr
	str r1,[sp]
	add r1,sp,#80	// sp at time of call
	ldr r2,=0xbadbad14	// r14 at time of call unknown
	stmib r0,{r1,r2}

	mov r0,#3	// T_BPTFLT
	mov r1,#0	// not used
	mov r2,sp
	bl kdb_trap

	// Return per savearea on stack.
	ldmia sp!,{r0,r14}	// cpsr and pc
	msr cpsr,r0
	ldmia sp,{r0-r13}
	mov pc,r14

