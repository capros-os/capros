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
#include <eros/Invoke.h>
#include <eros/arch/arm/SWI.h>

/* Note: Offsets in this code depend on the Message structure
   declared in Invoke.h:
 0 snd_invKey, invType
 4 snd_keys
 8 snd_len
12 snd_code
16 snd_w1
20 snd_w2
24 snd_w3
28 snd_data
32 rcv_keys
36 rcv_limit
40 rcv_data
44 rcv_code
48 rcv_w1
52 rcv_w2
56 rcv_w3
60 rcv_keyInfo
64 rcv_sent
 */
	
	.weak	EXT(INVOKECAP)
	.set	EXT(INVOKECAP),EXT(__rt_do_INVOKECAP)

/* On entry to INVOKECAP, r0 has a pointer to a Message structure. */
ENTRY(__rt_do_INVOKECAP)
	stmfd sp!,{r4,r14}
	ldmia r0,{r1-r4}
	b invokeCommon	// go with invType from Message structure
	
	.weak	EXT(NPRETURN)
	.set	EXT(NPRETURN),EXT(__rt_do_NPRETURN)

/* On entry to NPRETURN, r0 has a pointer to a Message structure. */
ENTRY(__rt_do_NPRETURN)
	stmfd sp!,{r4,r14}
	ldmia r0,{r1-r4}
	and r1,r1,#0xff
	orr r1,r1,#(IT_NPReturn<< 8)
	b invokeCommon
	
	.weak	EXT(RETURN)
	.set	EXT(RETURN),EXT(__rt_do_RETURN)

/* On entry to RETURN, r0 has a pointer to a Message structure. */
ENTRY(__rt_do_RETURN)
	stmfd sp!,{r4,r14}
	ldmia r0,{r1-r4}
	and r1,r1,#0xff
	orr r1,r1,#(IT_PReturn<< 8)
	b invokeCommon
	
	.weak	EXT(CALL)
	.set	EXT(CALL),EXT(__rt_do_CALL)

/* On entry to CALL, r0 has a pointer to a Message structure. */
ENTRY(__rt_do_CALL)
	stmfd sp!,{r4,r14}
/* Here we load into registers the most commonly used fields of
   the Message structure, because that is the fastest way to get
   the information into the kernel. */
	ldmia r0,{r1-r4}
	and r1,r1,#0xff
	orr r1,r1,#(IT_Call << 8)
invokeCommon:
/* r0 - ptr to Message structure
   r1 - low byte has snd_invKey
        next byte has invType
        high bytes may have garbage (from INVOKECAP)
   r2 - keys to send
   r3 - snd_len
   r4 - snd_code
 */
	ldr r14,[r0,#32]	// rcv_keys
	ldr r12,[r0,#36]	// rcv_limit

	swi SWI_Invoke
/* When we get back:
   r0 - ptr to Message structure
   r1 - rcv_code
   r2 - rcv_w1
   r3 - rcv_w2
   r4 - rcv_w3
   r12 - rcv_keyInfo
   r14 - rcv_sent
 */
	add r0,r0,#44
	stmia r0,{r1-r4,r12,r14}	// store values in Message structure
	mov r0,r1	// rcv_code also returned as value in r0
	ldmfd sp!,{r4,pc}	// return
