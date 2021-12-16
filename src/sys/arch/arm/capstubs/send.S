/*
 * Copyright (C) 2006, 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <eros/arch/arm/asm.h>
#include <eros/Invoke.h>
#include <eros/arch/arm/SWI.h>

/* Note: Offsets in this code depend on the Message structure
   declared in Invoke.h. */
	
	.weak	EXT(SEND)
	.set	EXT(SEND),EXT(__rt_do_SEND)

/* On entry to SEND, r0 has a pointer to a Message structure. */
__rt_do_SEND:	.globl __rt_do_SEND
	stmfd sp!,{r4,r5,r14}
/* Here we load into registers the most commonly used fields of
   the Message structure, because that is the fastest way to get
   the information into the kernel. */
	ldmia r0,{r1-r5}
	and r1,r1,#0xff
	orr r1,r1,#(IT_Send << 8)
/* r0 - ptr to Message structure
   r1 - low byte has snd_invKey
        next byte has invType
        high bytes have don't cares
   r2 - keys to send
   r3 - snd_len
   r4 - snd_code
   r5 - snd_w1
 */
	swi SWI_Invoke
/* When we get back:
   r0 - ptr to Message structure
 */
	ldmfd sp!,{r4,r5,pc}	// return
	
	.weak	EXT(PSEND)
	.set	EXT(PSEND),EXT(__rt_do_PSEND)

/* On entry to PSEND, r0 has a pointer to a Message structure. */
__rt_do_PSEND:	.globl __rt_do_PSEND
	stmfd sp!,{r4,r5,r14}
	ldmia r0,{r1-r5}
	and r1,r1,#0xff
	orr r1,r1,#(IT_PSend << 8)
	swi SWI_Invoke
	ldmfd sp!,{r4,r5,pc}	// return
