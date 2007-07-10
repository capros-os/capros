	.file "protospace.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* protospace -- trampoline code that lets a process initialize its 
 * own address space so that the constructor isn't tied up by
 * initialization time.  This code is trusted by the constructor,
 * and runs in a read-only space.
 */

#include <eros/machine/asm.h>
	
#include <eros/target-asm.h>
#include <eros/StdKeyType.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/NodeKey.h>

#include <idl/capros/key.h>
#include <idl/capros/Number.h>
#include <idl/capros/SpaceBank.h>

#include <domain/ConstructorKey.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>
#include <eros/arch/arm/SWI.h>
	
/* We must declare a Message structure for SWI_Invoke to use. */
	.section .rodata
	.balign 4
messageStruct:
	.byte 0, 0, 0, 0
	.byte 0, 0, 0, 0
	.long 0, 0, 0	/* snd_len, snd_code, snd_w1 */
	/* the values above are not used,
	   as the values are passed in registers */
	.long 0,0	/* snd_w2 and _w3 */
	.long 0		/* snd_data */
	.byte 0, 0, 0, 0	/* snd_keys, not used */
	.long 0, 0	/* rcv_limit and rcv_data, not used */
	/* The remainder of the Message structure is fields for values */
	/* that are returned in registers, not in the structure. */
	.text

#define InvConstants(label, snd_invKey, snd_code, snd_w1, \
                     snd_key0, snd_key1, rcv_key0, rcv_key1) \
	.section .rodata ;\
	.balign 4 ;\
/* Just declare the values to be loaded into registers. */ \
label:	.byte snd_invKey, IT_Call, 0, 0 ;\
	.byte snd_key0,snd_key1,KR_VOID,KR_VOID ;\
	.long 0		/* snd_len */ ;\
	.long snd_code ;\
	.long snd_w1 ;\
/* #if RESUME_SLOT == 3 */ \
	.long 0		/* rcv_limit */ ;\
	.byte rcv_key0,rcv_key1,KR_VOID,KR_VOID ;\
	.text

/* Can use simpleCall if not using w2-w3. */
#define simpleCall(label, snd_invKey, snd_code, snd_w1, snd_key0, snd_key1, rcv_key0) \
	InvConstants(label, snd_invKey, snd_code, snd_w1, snd_key0, snd_key1, rcv_key0, KR_VOID) ;\
	ldr r0,=label ;\
	ldmia r0,{r1-r5,r12,r14} ;\
	ldr r0,=messageStruct	/* our constant Message structure */ ;\
	swi SWI_Invoke

	.text
#ifdef __ELF__
ENTRY(_start)
#else
#error
ENTRY(start)
#endif
	/* On entry, if we came from the constructor, r3 has zero.
	If we came from protospace_destroy(),
	r3 (which is where rcv_w2 ends up) will have 
	1 if destroying a ??, or
	2 if destroying a small space.
	(Not sure where 3 comes from.) */

	cmp r3,#2
	beq delete_small_space
	cmp r3,#3
	beq delete_kept_space
	cmp r3,#0
	bne sepuku	// presumably 1
 
	/* The address space may be a constructor, in which case we need
	   to invoke it to obtain the true process address space. Check the
	   type of the address space key -- if it alleges to be a  
	   constructor, believe it and invoke it to get the real thing. */
	
	simpleCall(kt_addr_space, PSKR_SPACE, OC_capros_key_getType, 0,
		KR_VOID, KR_VOID, KR_VOID)
	cmp r1,#RC_OK
	bne perform_lobotomy	// didn't respond to getType?
	ldr r0,=AKT_ConstructorRequestor
	cmp r2,r0
	bne perform_lobotomy

	/* Run the constructor for the address space */
	
	simpleCall(fab_space, PSKR_SPACE, OC_Constructor_Request, 0,
		KR_BANK, KR_SCHED, PSKR_SPACE)
	cmp r1,#RC_OK
	bne delete_kept_space

perform_lobotomy:
	/* Fetch the program counter of this process out
	   of the startup key that holds it.  If we had just
	   one more register, we wouldn't have to get the #$)*^%
	   thing from a key */
	
	simpleCall(pc_value, PSKR_PROC_PC, OC_capros_Number_get, 0,
		KR_VOID, KR_VOID, KR_VOID)
	mov r6,r2	// save rcv_w1, which is the PC
		
	/* New program PC is now in r2 (w1).  Switch to the real
	   address space and proceed... */
	/* rcv_key1 is returned as void. Accept it in PSKR_PROC_PC
	to clear that slot that we used temporarily. */
	InvConstants(swap_space_and_pc, KR_SELF, OC_Process_SwapMemory32, 0, \
	             PSKR_SPACE, KR_VOID, PSKR_SPACE, PSKR_PROC_PC)
	ldr r0,=swap_space_and_pc
	ldmia r0,{r1-r4,r12,r14}
	mov r5,r6	// new PC in snd_w1
	ldr r0,=messageStruct	/* our constant Message structure */
	swi SWI_Invoke

	/* THIS POINT SHOULD NOT BE REACHED -- WE ARE NOW RUNNING THE
	   APPLICATION'S CODE */
	swi SWI_halt
	
delete_small_space:
	simpleCall(sb_reclaim_pages_from_node, KR_BANK,
                   OC_capros_SpaceBank_ReclaimDataPagesFromNode, 0,
		   PSKR_SPACE, KR_VOID, KR_VOID)
	
	/* Sell back the node itself */
	simpleCall(sb_reclaim_small_space_node, KR_BANK,
		   OC_capros_SpaceBank_free1, 0,
		   PSKR_SPACE, KR_VOID, KR_VOID)

	b sepuku
	
delete_kept_space:
	simpleCall(destroy_kept_space, PSKR_SPACE,
		   OC_capros_key_destroy, 0,
		   KR_VOID, KR_VOID, KR_VOID)
sepuku:
	/* construction of the address space failed.  Ask our process
	   creator to blow us away and report failure to the user */
	simpleCall(destroy_caller, KR_CREATOR,
		   OC_ProcCre_DestroyCallerAndReturn, 0,
		   KR_BANK, KR_RETURN, KR_VOID)
	
	/* THIS POINT SHOULD NOT BE REACHED -- APPLICATION NOW DEAD */
	swi SWI_halt
