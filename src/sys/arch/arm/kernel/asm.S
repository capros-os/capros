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

	.text
/* Assorted short assembler routines,
   not performance-critical enough to warrant declaring inline. */


	ENTRY(mach_FlushBothTLBs)
	mov r0,#0
	mcr p15,0,r0,c8,c7,0
	ldr r1,=PteZapped
	strb r0,[r1]	// clear the flag
#if 0
	ldr r0,=TLBMsg
	b printf
	.section .rodata
TLBMsg:	.asciz "Flushed TLB\n"
	.text
#else
	mov pc,lr
#endif

	ENTRY(mach_ReadCacheType)
	mrc p15,0,r0,c0,c0,1	// read cache type register to r0
	mov pc,lr

	ENTRY(mach_FlushTLBsCaches)
	mov r0,#0
	mcr p15,0,r0,c8,c7,0
	ldr r1,=PteZapped
	strb r0,[r1]	// clear the flag
	// fall into mach_FlushBothCaches

/* "Flush" here means "clean and invalidate". */
	ENTRY(mach_FlushBothCaches)
#ifdef OPTION_WRITEBACK
	/* Here is the downside of OPTION_WRITEBACK.
           This takes at least 12.8 microseconds on the EDB9315. */
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
#if 0
	ldr r0,=TLBCacheMsg
	b printf
	.section .rodata
TLBCacheMsg:	.asciz "Flushed TLB&Cache\n"
	.text
#else
	mov pc,lr
#endif

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

/* SafeLoadByte
   Returns true iff successful. */
	ENTRY(SafeLoadByte)
/* The following instruction will fault if the user does not have access.
   The fault handler recognizes this instruction address. */
	ENTRY(SafeLoadByteInstr)
	ldrb r2,[r0]
	strb r2,[r1]
	mov r0,#1	/* successful, return true */
	mov pc,lr

/* If the above access failed, we get here. */
	ENTRY(SafeLoadByteReturn)
	mov r0,#0	/* return false */
	mov pc,lr

/* SafeStoreByte
   Returns true iff successful. */
	ENTRY(SafeStoreByte)
/* The following instruction will fault if the user does not have access.
   The fault handler recognizes this instruction address. */
	ENTRY(SafeStoreByteInstr)
	strb r1,[r0]
	mov r0,#1	/* successful, return true */
	mov pc,lr

/* If the above access failed, we get here. */
	ENTRY(SafeStoreByteReturn)
	mov r0,#0	/* return false */
	mov pc,lr


	ENTRY(mach_Yield)
	ldr sp,=(kstackBot)	/* Empty the stack! */
#ifndef NDEBUG
	mov fp,#0		// to terminate stack traceback
#endif
	b act_HandleYieldEntry

#ifndef NDEBUG
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
#endif
