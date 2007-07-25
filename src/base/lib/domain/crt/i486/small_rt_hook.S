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

/*
 * small_rt_hook.S implements the startup-time hooks for small space
 * initialization. Some of this will go away with the introduction
 * of the small space keeper.
 */

#include <eros/arch/i486/asm.h>
#include <eros/arch/i486/target-asm.h>
#include <eros/StdKeyType.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <domain/ProtoSpace.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <domain/Runtime.h>
	
#define KR_OLDSPACE KR_APP(0)
#define KR_NEWSPACE KR_APP(1)
#define KR_NEWPAGE  KR_APP(2)
	
/* Startup code for processes.  Handles four cases:

   1. Applications that need to construct for themselves
      a small space.  This last class is immutable when
      first entered.
   2. Applications with VCS spaces
   3. Applications that run out of a small space which has
      already been initialized.  This uses a special entry point,
      and applies only to a small number of processes early in the
      EROS bootstrap process.  It probably should NOT be used by
      other programs.

   The application is expected to provide the startup code with the
   information as to which sort of process it is by exporting a
   symbol called __rt_stack_pages and another called
   __rt_stack_pointer.  These symbols should live in read-only data
   space.

   If __rt_stack_pages is nonzero, the startup code first purchases
   a capage to be the new address space root.  It then buys the number
   of stack pages indicated in __rt_stack_pages and populates the
   capage with them beginning from slot 15 and working down.  If this
   overwrites your program, too bad -- in that case you should have
   used a VCS.  For programs with a VCS, __rt_stack_pages should be
   set to zero, as no stack pages need to be explicitly purchased.

   Once the stack has been fabricated (or not, as the case may be),
   the startup code loads the value of __rt_stack_pointer into %ESP.

   (SOON) The startup code then calls the entry point for C++
   constructors.  This relies on the fact that a dummy stub is
   provided by crtn.o in the case where this domain was written in C.

   Finally, the startup code calls main(), at which point control has
   been transferred to the user's code.

   (SOON) On exit from main, the startup code calls __exit(), which
   attempts to self-destruct the process.  Since the user has probably
   left the key registers in a disordered state, this probably fails
   unless main() has taken appropriate measures to ensure that it
   succeeds.

   On entry, we assume that KR_BANK and KR_SELF are set up according
   to the usual runtime conventions.

   This code must not damage key registers 0xf (resume key) or 0xc 
   (first key arg).
   */
	
#define KEYS(k0, k1, k2, k3) (k0 + k1 << 8 + k2 << 16 + k3 << 24)
	
#define NO_KEYS KEYS(KR_VOID, KR_VOID, KR_VOID, KR_VOID)

#define MSG(label, snd_invKey, sndKey, rcvKey) \
	.section .rodata ;\
label:	.long	IT_Call ;\
	.long	snd_invKey ;\
	.long	0		/* snd len */ ;\
	.long	0		/* snd ptr */ ;\
	.long	KEYS(sndKey, KR_VOID, KR_VOID, KR_VOID)	/* snd keys */ ;\
	.long	0		/* rcv len */ ;\
	.long	0		/* rcv data */ ;\
	.long	KEYS(rcvKey, KR_VOID, KR_VOID, KR_VOID)	/* rcv keys */

#define IVK1(msgLabel, code, snd_w1) \
	movl	$msgLabel,%ebp ;\
	movl	$code,%eax	/* w0 */ ;\
	movl	$snd_w1,%ebx	/* w1 */ ;\
	xorl	%ecx,%ecx	/* w2 */ ;\
	xorl	%edx,%edx	/* w3 */ ;\
	int	$0x31		/* invocation trap */
   
	MSG(sb_alloc_root_node, KR_BANK, KR_VOID, KR_NEWSPACE)
   
	MSG(mk_node_key, KR_NEWSPACE, KR_VOID, KR_NEWSPACE)
   
	MSG(buy_first_page, KR_BANK, KR_VOID, KR_NEWPAGE)
   
	MSG(fetch_cur_space, KR_SELF, KR_VOID, KR_OLDSPACE)
   
	MSG(clone_cur_space, KR_NEWSPACE, KR_OLDSPACE, KR_VOID)
   
	MSG(insert_stack_page, KR_NEWSPACE, KR_NEWPAGE, KR_VOID)
   
	MSG(set_my_addr_space, KR_SELF, KR_NEWSPACE, KR_VOID)
	
	
	.text
ENTRY(__rt_small_startup)
	movl	EXT(__rt_stack_pointer),%ESP
	
	/* Certain "Iron Man" domains pre-set their own stacks. These
	 * will have set __rt_stack_pages to 0 for us. */
	cmpl	$0,EXT(__rt_stack_pages)
        jz	EXT(__stack_ready)
	
	IVK1(sb_alloc_root_node, OC_capros_SpaceBank_alloc1, capros_Range_otGPT)
	
	IVK1(mk_node_key, OC_capros_GPT_setL2v, EROS_PAGE_LGSIZE)
	
	/* buy the page while the space bank is still hot in
	   the data cache */
	IVK1(buy_first_page, OC_capros_SpaceBank_alloc1, capros_Range_otPage)

	/* fetch current (immutable) address space from our
	   process slot */
	IVK1(fetch_cur_space, OC_Process_Copy, ProcAddrSpace)

#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif
	
	IVK1(clone_cur_space, OC_capros_GPT_clone, 0)

#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif

	/* stick the stack page we bought into the new node */
	IVK1(insert_stack_page, OC_capros_GPT_setSlot, (EROS_NODE_SIZE-1))
	
#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif

	IVK1(set_my_addr_space, OC_Process_Swap, ProcAddrSpace)

#define NUMPG  0x1FFFC
#define PGSLOT 0x1FFF8
#define CALLBUF 0x1FFD0
	
	/* WE NOW HAVE A STACK PAGE WE CAN USE FOR COUNTERS!!! */

	movl	EXT(__rt_stack_pages),%eax
	movl	%eax,NUMPG
	movl	$CALLBUF,%ebp
	movl	$(EROS_NODE_SIZE-1),PGSLOT	/* next slot to write */
	
LEXT(buy_stack)
	decl	NUMPG
	/* buy any remaining stack pages we may need... */
	jz	EXT(__stack_ready)
	decl	PGSLOT
	

	movl	$OC_capros_SpaceBank_alloc1,%eax	/* w0 */
	movl	$capros_Range_otPage,%ebx		/* w1 */
	xorl	%ecx,%ecx			/* w2 */
	xorl	%edx,%edx			/* w3 */

	movl	$IT_Call,(%ebp)		/* inv type */
	movl	$KR_BANK,4(%ebp)	/* invoked key */
	movl	$0,8(%ebp)		/* snd len */
	movl	$0,12(%ebp)		/* snd ptr */
	movl	$NO_KEYS,16(%ebp)	/* snd keys */
	movl	$0,20(%ebp)		/* rcv len */
	movl	$0,24(%ebp)		/* rcv data */
	movl	$KEYS(KR_NEWPAGE,KR_VOID,KR_VOID,KR_VOID),28(%ebp) /* rcv keys */
	
	int	$0x31			/* invocation trap */
	
	/* stick the stack page we bought into the new node */
	movl	$OC_capros_GPT_setSlot,%eax	/* w0 */
	movl	PGSLOT,%ebx		/* w1 */
	xorl	%ecx,%ecx		/* w2 */
	xorl	%edx,%edx		/* w3 */

	movl	$NO_KEYS,28(%ebp)
	movl	$KEYS(KR_NEWPAGE,KR_VOID,KR_VOID,KR_VOID),16(%ebp)		/* snd new pg */
	movl	$KR_NEWSPACE,4(%ebp)	/* into new node */
	
	int	$0x31			/* invocation trap */

#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif

	jmp	EXT(buy_stack)
	
LEXT(__stack_ready)
	call	EXT(__rt_buy_data_space)
        movl	$0,__rt_runtime_hook

	# Done with runtime hook. Re-start at start symbol.
	jmp	EXT(_start)

	.data
	.globl	__rt_runtime_hook
	.align 4
	.type	__rt_runtime_hook,@object
	.size	__rt_runtime_hook,4
__rt_runtime_hook:
	.long	__rt_small_startup

