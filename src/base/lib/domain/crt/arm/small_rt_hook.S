	.file	"small_rt_hook.S"
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/arch/arm/asm.h>
#include <eros/arch/arm/target-asm.h>
#include <eros/arch/arm/SWI.h>
#include <eros/StdKeyType.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <domain/ProtoSpace.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <idl/capros/GPT.h>
#include <domain/Runtime.h>
	
#define KR_OLDSPACE KR_TEMP0
#define KR_NEWSPACE KR_TEMP1
#define KR_NEWPAGE  KR_TEMP2
	
/*
   The application is expected to provide the startup code with the
   information as to what sort of initialization it requires by exporting
   the symbols __rt_stack_pages, __rt_stack_pointer, and __rt_runtime_hook.
   These symbols should live in read-only data space.

   This file defines __rt_runtime_hook to contain __rt_small_startup,
   which causes the code at _start in crt0.S to jump to this code.

   If __rt_stack_pages is nonzero, this code:
   1. purchases a node to be the new address space root.
   2. buys the number of stack pages indicated in __rt_stack_pages
      and populates the node with them beginning from the last slot
      and working down.
   If this overwrites your program, too bad -- in that case you should have
   used a VCS.  For programs with a VCS, __rt_stack_pages should be
   set to zero, as no stack pages need to be explicitly purchased.
   3. buys (read/write) pages for the data and bss sections and
      initializes and installs them. 

   Once the stack has been fabricated (or not, as the case may be),
   the startup code loads the value of __rt_stack_pointer
   (which should be 0x20000) into sp.

   On entry, we assume that KR_BANK and KR_SELF are set up according
   to the usual runtime conventions.

   This code must not damage key registers KR_RETURN (resume key)
   or KR_ARG(0) (first key arg).
   */

/* Note, we start off with NO read/write memory, so we have to pre-build
   some Message structures. */

#define Call1(label, snd_invKey, snd_code, snd_w1, snd_key0, rcv_key0) \
	.section .rodata ;\
	.balign 4 ;\
/* Declare a Message structure. */ \
label:	.byte snd_invKey, IT_Call, 0, 0 ;\
	.byte snd_key0,KR_VOID,KR_VOID,KR_VOID ;\
	.long 0		/* snd_len */ ;\
	.long snd_code ;\
	.long snd_w1 ;\
	.long 0,0	/* snd_w2 and _w3 */ ;\
	.long 0		/* snd_data */ ;\
/* #if RESUME_SLOT == 3 */ \
	.byte rcv_key0,KR_VOID,KR_VOID,KR_VOID ;\
	.long 0		/* rcv_limit */ ;\
	.long 0		/* rcv_data */ ;\
	/* The remainder of the Message structure is fields for values */ \
	/* that are returned in registers, not in the structure. */ \
	.text ;\
	ldr r0,=label ;\
	ldmia r0,{r1-r5} ;\
	ldr r14,[r0,#32] ;\
	ldr r12,[r0,#36] ;\
	swi SWI_Invoke

#define InvConstants(label, snd_invKey, snd_code, snd_w1, snd_key0, rcv_key0) \
	.section .rodata ;\
	.balign 4 ;\
/* Just declare the values to be loaded into registers. */ \
label:	.byte snd_invKey, IT_Call, 0, 0 ;\
	.byte snd_key0,KR_VOID,KR_VOID,KR_VOID ;\
	.long 0		/* snd_len */ ;\
	.long snd_code ;\
	.long snd_w1 ;\
/* #if RESUME_SLOT == 3 */ \
	.long 0		/* rcv_limit */ ;\
	.byte rcv_key0,KR_VOID,KR_VOID,KR_VOID ;\
	.text

#define simpleCall(label, snd_invKey, snd_code, snd_w1, snd_key0, rcv_key0) \
	InvConstants(label, snd_invKey, snd_code, snd_w1, snd_key0, rcv_key0) ;\
	ldr r0,=label ;\
	ldmia r0,{r1-r5,r12,r14} ;\
	ldr r0,=mk_node_key	/* a handy Message structure */ ;\
	swi SWI_Invoke
	
	
	.text
ENTRY(__rt_small_startup)
	/* Certain "Iron Man" domains pre-set their own stacks. These
	 * will have set __rt_stack_pages to 0 for us. */
	ldr r0,=__rt_stack_pages
	ldr r0,[r0]
	cmp r0,#0
	beq stack_ready
	
	simpleCall(sb_alloc_root_node, KR_BANK,
               OC_capros_SpaceBank_alloc1,
               capros_Range_otGPT, KR_VOID, KR_NEWSPACE)
// FIXME: If any of these allocations fails, what to do?

	Call1(mk_node_key, KR_NEWSPACE,
              OC_capros_GPT_setL2v, EROS_PAGE_LGSIZE, KR_VOID,
              KR_NEWSPACE)
	
	/* buy the page while the space bank is still hot in
	   the cache */
	simpleCall(buy_first_page, KR_BANK,
                   OC_capros_SpaceBank_alloc1,
                   capros_Range_otPage, KR_VOID, KR_NEWPAGE)

	/* fetch current (immutable) address space from our
	   process slot */
	Call1(fetch_cur_space, KR_SELF,
              OC_capros_Process_getAddrSpace, 0, KR_VOID,
              KR_OLDSPACE)
#if 0
	cmp r1,#0	/* check return value */
	bne ...
#endif
	
	simpleCall(clone_cur_space, KR_NEWSPACE,
                   OC_capros_GPT_clone, 0, KR_OLDSPACE,
                   KR_VOID)
#if 0
	cmp r1,#0	/* check return value */
	bne ...
#endif

	/* stick the stack page we bought into the new node */
	Call1(insert_stack_page, KR_NEWSPACE,
              OC_capros_GPT_setSlot, EROS_NODE_SIZE-1, KR_NEWPAGE, 
              KR_VOID)
#if 0
	cmp r1,#0	/* check return value */
	bne ...
#endif

	/* Switch to newly copied space -- note no need to change PC! */
	Call1(set_my_addr_space, KR_SELF,
              OC_capros_Process_swapAddrSpace, 0, KR_NEWSPACE,
              KR_VOID)

	/* We now have a stack page we can use for a Message structure! */
#define sizeof_Message 68
#define MessageAddr (EROS_PAGE_SIZE * EROS_NODE_SIZE - sizeof_Message)

	ldr r8,=__rt_stack_pages
	ldr r0,=MessageAddr	// r0 is preserved by invocation
	ldr r8,[r8]		// number of pages left to create +1
	mov r6,#(EROS_NODE_SIZE-1)	// next slot to write +1

	// Most fields in the Message structure are don't cares.

	/* buy any remaining stack pages we may need */
buy_stack:
	subs r8,r8,#1
	beq stack_ready
	subs r6,r6,#1		// slot to write
	
	InvConstants(buy_later_page, KR_BANK,
                     OC_capros_SpaceBank_alloc1,
                     capros_Range_otPage,
                     KR_VOID, KR_NEWPAGE)
	ldr r7,=buy_later_page
	ldmia r7,{r1-r5,r12,r14}
	swi SWI_Invoke
#if 0
	cmp r1,#0	/* check return value */
	bne ...
#endif

	/* stick the stack page we bought into the new node */
	InvConstants(set_later_page, KR_NEWSPACE,
                     OC_capros_GPT_setSlot, 0,
                     KR_NEWPAGE, KR_VOID)
	ldr r7,=set_later_page
	ldmia r7,{r1-r4,r12,r14}
	mov r5,r6	// snd_w1, slot to write
	swi SWI_Invoke

	b buy_stack
	
stack_ready:
	ldr sp,=__rt_stack_pointer
	ldr sp,[sp]
/* Now that we have a stack, call a C procedure to create read/write
   pages for the data and bss sections. */
	bl __rt_buy_data_space

	// Done with runtime hook.
	ldr r1,=__rt_runtime_hook
	mov r0,#0
	str r0,[r1]
	b _start	// re-start the initialization

	.data
	.globl	__rt_runtime_hook
	.balign 4
//	.type	__rt_runtime_hook,@object
//	.size	__rt_runtime_hook,4
__rt_runtime_hook:
	.long	__rt_small_startup

