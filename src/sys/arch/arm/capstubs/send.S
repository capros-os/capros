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
#include "../kernel/SWI.h"

/* Note: Offsets in this code depend on the Message structure
   declared in Invoke.h. */
	
	.weak	EXT(SEND)
	.set	EXT(SEND),EXT(__rt_do_SEND)

/* On entry to SEND, r0 has a pointer to a Message structure. */
__rt_do_SEND:	.globl __rt_do_SEND
	stmfd sp!,{r4,r14}
/* Here we load into registers the most commonly used fields of
   the Message structure, because that is the fastest way to get
   the information into the kernel. */
	ldmia r0,{r1-r4}
	and r1,r1,#0xff
	orr r1,r1,#(IT_Send << 8)
/* r0 - ptr to Message structure
   r1 - low byte has snd_invKey
        next byte has invType
        high bytes have don't cares
   r2 - keys to send
   r3 - snd_len
   r4 - snd_code
 */
	swi SWI_Invoke
/* When we get back:
   r0 - ptr to Message structure
   r1 - rcv_code?
 */
	str r1,[r0,#48]	// store rcv_code in Message structure
	mov r0,r1	// rcv_code also returned as value in r0
	ldmfd sp!,{r4,pc}	// return
