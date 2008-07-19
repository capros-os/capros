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
#include "PTEarm.h"

	.text
/* Assorted short assembler routines,
   not performance-critical enough to warrant declaring inline. */

/** mach_DoMapWork

r0 bits MapWork_User* have the work to be done. */
	ENTRY(mach_DoMapWork)
	tst r0,#MapWork_UserTLBWrong
	beq 1f
	// TLB has a problem
	ldr r1,=mapWork
	ldr r2,[r1]
	// If it hasn't been corrected, do so now.
	tst r2,#MapWork_KernClearedTLB
	bne 1f		// already cleared it
	stmfd sp!,{r0,lr}
	bl mach_FlushBothTLBs
	ldmfd sp!,{r0,lr}
1:	// Fall into mach_DoCacheWork

/** mach_DoCacheWork

r0 bits MapWork_UserCacheWrong and MapWork_UserDirtyWrong
have the work to be done. */
	ENTRY(mach_DoCacheWork)
	tst r0,#MapWork_UserDirtyWrong
	/* Cleaning the cache must be done before any invalidating.
	If MapWork_UserCacheWrong is set,
	MapWork_UserDirtyWrong must also be set. */
	moveq pc,lr	// nothing to clean, therefore nothing to invalidate
	ldr r1,=mapWork
	ldr r2,[r1]
	tst r2,#MapWork_KernCleanedCache
	movne pc,lr	// already cleaned, therefore already invalidated too
	orr r2,r2,#MapWork_KernCleanedCache
	str r2,[r1]	// set the bit to indicate we cleaned the cache
/* We need to do the above store BEFORE cleaning the cache.
Otherwise a subsequent InvalidateCache will lose the data. */
	// Clean the cache.
#if 0
	stmfd sp!,{r0,r1,lr}
	ldr r0,=2f
	bl printf
	.section .rodata
2:	.asciz "Cleaned Cache\n"
	.text
	ldmfd sp!,{r0,r1,lr}
#endif
#ifdef OPTION_WRITEBACK
	/* Here is the downside of OPTION_WRITEBACK.
           This takes at least 12.8 microseconds on the EDB9315. */
	ldr r1,=cacheSetIndexIncrement
	ldr r2,=cacheSetIndexCarry
	ldr r1,[r1]
	ldr r2,[r2]
	mov r3,#0
0:	mcr p15,0,r3,c7,c10,2	// clean data cache
	add r3,r3,r1	// add increment for next set
	add r3,r3,r2	// propagate any carry to index field
	bics r3,r3,r2	// prepare for next carry
	bne 0b
#endif
	// r0 still has work to be done

3:	tst r0,#MapWork_UserCacheWrong
	moveq pc,lr		// if not invalidating, return
	ldr r1,=mapWork
	ldr r2,[r1]
	tst r2,#MapWork_KernInvalidatedCache
	movne pc,lr		// already invalidated
	orr r2,r2,#MapWork_KernCleanedCache
	// Invalidate the cache.
	mov r3,#0
	mcr p15,0,r3,c7,c7,0	// invalidate entire cache(s)
#if 0
	stmfd sp!,{r1,r2,lr}
	ldr r0,=2f
	bl printf
	ldmfd sp!,{r1,r2,lr}
	.section .rodata
2:	.asciz "Invalidated Cache\n"
	.text
#endif
	/* The following store must be done AFTER invalidating the cache,
	else it will be lost. */
	str r2,[r1]	// set the bit to indicate we cleaned the cache
	mov pc,lr		// return


	ENTRY(mach_FlushBothTLBs)
	mov r0,#0
	mcr p15,0,r0,c8,c7,0
#if 0
	str lr,[sp, #-4]!	/* push return address */
	ldr r0,=TLBMsg
	bl printf
	ldr lr,[sp],#4		/* pop return address */
	.section .rodata
TLBMsg:	.asciz "Flushed TLB\n"
	.text
#endif
	ldr r1,=mapWork
	ldr r0,[r1]
	orr r0,r0,#MapWork_KernClearedTLB
	str r0,[r1]
	mov pc,lr


	ENTRY(mach_DrainWriteBuffer)
	mov r0,#0
	mcr p15,0,r0,c7,c10,4
	mov pc,lr

	ENTRY(mach_ReadCacheType)
	mrc p15,0,r0,c0,c0,1	// read cache type register to r0
	mov pc,lr

	ENTRY(mach_ReadTTBR)
	mrc p15,0,r0,c2,c0
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
   Returns true iff successful.
   Before calling, you must call UpdateTLB() so the user-mode map is correct. */
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
