	.file	"small_rt_hook.S"
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

/*
 * small_rt_hook.S implements the startup-time hooks for small space
 * initialization. Some of this will go away with the introduction
 * of the small space keeper.
 */

#include <eros/arch/i486/asm.h>
#include <eros/arch/i486/target-asm.h>
#include <eros/StdKeyType.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <domain/ProtoSpace.h>
#include <domain/SpaceBankKey.h>
#include <domain/Runtime.h>
	
#define KR_OLDSPACE KR_APP(0)
#define KR_NEWSPACE KR_APP(1)
#define KR_NEWPAGE  KR_APP(2)
	
#define KEYS(k0, k1, k2, k3) (k0 + k1 << 8 + k2 << 16 + k3 << 24)
	
#define NO_KEYS KEYS(KR_VOID, KR_VOID, KR_VOID, KR_VOID)
	
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
   
	.section .rodata
sb_alloc_root_node:
	.long	IT_Call
	.long	KR_BANK
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	KEYS(KR_NEWSPACE,KR_VOID,KR_VOID,KR_VOID)	/* rcv keys */
	
mk_node_key:
	.long	IT_Call
	.long	KR_NEWSPACE
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	KEYS(KR_NEWSPACE,KR_VOID,KR_VOID,KR_VOID)	/* rcv keys */

buy_first_page:	
	.long	IT_Call
	.long	KR_BANK
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	KEYS(KR_NEWPAGE,KR_VOID,KR_VOID,KR_VOID)	/* rcv keys */

	xorl	%edi,%edi		/* rcv len */
	
fetch_cur_space:	
	.long	IT_Call
	.long	KR_SELF
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	KEYS(KR_OLDSPACE,KR_VOID,KR_VOID,KR_VOID)	/* rcv keys */
	
clone_cur_space:	
	.long	IT_Call
	.long	KR_NEWSPACE
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	KEYS(KR_OLDSPACE,KR_VOID,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */
	
insert_stack_page:	
	.long	IT_Call
	.long	KR_NEWSPACE
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	KEYS(KR_NEWPAGE,KR_VOID,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */
	
set_my_addr_space:	
	.long	IT_Call
	.long	KR_SELF
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	KEYS(KR_NEWSPACE,KR_VOID,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */
	
	
	.text
ENTRY(__rt_small_startup)
	movl	EXT(__rt_stack_pointer),%ESP
	
	/* Certain "Iron Man" domains pre-set their own stacks. These
	 * will have set __rt_stack_pages to 0 for us. */
	cmpl	$0,EXT(__rt_stack_pages)
        jz	EXT(__stack_ready)
	
	movl	$sb_alloc_root_node,%ebp
	
	movl	$OC_SpaceBank_Alloc1Node,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	
	movl	$mk_node_key,%ebp
	
	movl	$OC_Node_MakeNodeKey,%eax	/* r0 */
#if EROS_PAGE_BLSS == 2
	movl	$3,%ebx				/* Desired BLSS */
#elif EROS_PAGE_BLSS == 1
	movl	$2,%ebx				/* Desired BLSS */
#elif EROS_PAGE_BLSS == 0
	movl	$1,%ebx				/* Desired BLSS */
#endif
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	
	/* buy the page while the space bank is still hot in
	   the data cache */
	movl	$buy_first_page,%ebp
	
	movl	$OC_SpaceBank_Alloc1DataPage,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	/* fetch current (immutable) address space from our
	   process slot */
	movl	$fetch_cur_space,%ebp
	
	movl	$OC_Process_Copy,%eax		/* r0 */
	movl	$ProcAddrSpace,%ebx		/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif
	

	movl	$clone_cur_space,%ebp
	
	movl	$OC_Node_Clone,%eax		/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif

	movl	$insert_stack_page,%ebp
	
	/* stick the stack page we bought into the new node */
	movl	$OC_Node_Swap,%eax		/* r0 */
	movl	$EROS_NODE_SLOT_MASK,%ebx	/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	
#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif


	movl	$set_my_addr_space,%ebp
	
	/* Switch to newly copied space -- note no need to change PC! */
	movl	$OC_Process_Swap,%eax		/* r0 */
	movl	$ProcAddrSpace,%ebx		/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */


#define NUMPG  0x1FFFC
#define PGSLOT 0x1FFF8
#define CALLBUF 0x1FFD0
	
	/* WE NOW HAVE A STACK PAGE WE CAN USE FOR COUNTERS!!! */

	movl	EXT(__rt_stack_pages),%eax
	movl	%eax,NUMPG
	movl	$CALLBUF,%ebp
	movl	$EROS_NODE_SLOT_MASK,PGSLOT	/* next slot to write */
	
LEXT(buy_stack)
	decl	NUMPG
	/* buy any remaining stack pages we may need... */
	jz	EXT(__stack_ready)
	decl	PGSLOT
	

	movl	$OC_SpaceBank_Alloc1DataPage,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

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
	movl	$OC_Node_Swap,%eax		/* r0 */
	movl	PGSLOT,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	movl	$NO_KEYS,28(%ebp)		/* refuse old pg */
	movl	$KEYS(KR_NEWPAGE,KR_VOID,KR_VOID,KR_VOID),16(%ebp)		/* snd new pg */
	movl	$KR_NEWSPACE,4(%ebp)		/* into new node */
	
	int	$0x31			/* invocation trap */

#if 0
	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)
#endif

	jmp	EXT(buy_stack)
	
ENTRY(__stack_ready)
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

