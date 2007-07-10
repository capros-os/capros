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
 * initialization time.  This code is truster by the constructor,
 * and runs in an immutable space.
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
	
#define KEYS(k0, k1, k2, k3) (k0 + k1 << 8 + k2 << 16 + k3 << 24)
	
#define NO_KEYS KEYS(KR_VOID, KR_VOID, KR_VOID, KR_VOID)
	
	.section .rodata
kt_addr_space:
	.long	IT_Call
	.long	PSKR_SPACE
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */

fab_space:
	.long	IT_Call
	.long	PSKR_SPACE
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long   KEYS(KR_BANK,KR_SCHED,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	KEYS(PSKR_SPACE,KR_VOID,KR_VOID,KR_VOID)	/* rcv keys */

pc_value:
	.long	IT_Call
	.long	PSKR_PROC_PC
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */
	
swap_space_and_pc:
	.long	IT_Call
	.long	KR_SELF
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	KEYS(PSKR_SPACE,KR_VOID,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	/* rcv_key1 is returned as void. Accept it in PSKR_PROC_PC
	to clear that slot that we used temporarily. */
	.long	KEYS(PSKR_SPACE,PSKR_PROC_PC,KR_VOID,KR_VOID)	/* rcv keys */
	
sb_reclaim_pages_from_node:	
	.long	IT_Call
	.long	KR_BANK
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	KEYS(PSKR_SPACE,KR_VOID,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */

sb_reclaim_small_space_node:	
	.long	IT_Call
	.long	KR_BANK
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	KEYS(PSKR_SPACE,KR_VOID,KR_VOID,KR_VOID)	/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */

destroy_kept_space:	
	.long	IT_Call
	.long	PSKR_SPACE
	.long	0		/* snd len */
	.long	0		/* snd ptr */
	.long	NO_KEYS		/* snd keys */
	.long	0		/* rcv len */
	.long	0		/* rcv data */
	.long	NO_KEYS		/* rcv keys */
	
destroy_caller:	
	.long	IT_Call
	.long	KR_CREATOR
	.long	0			/* snd len */
	.long	0			/* snd ptr */
	.long	KEYS(KR_BANK,KR_RETURN,KR_VOID,KR_VOID)	/* snd keys */
	.long	0			/* rcv len */
	.long	0			/* rcv data */
	.long	NO_KEYS			/* rcv keys */
	
	.text
#ifdef __ELF__
ENTRY(_start)
#else
ENTRY(start)
#endif
	/* On entry, if we came from the constructor, ECX has zero.
	If we came from protospace_destroy(),
	ECX (which is where rcv_w2 ends up) will have 
	1 if destroying a ??, or
	2 if destroying a small space.
	(Not sure where 3 comes from.) */
	cmpl	$2,%ecx
	je	EXT(delete_small_space)
	
	cmpl	$3,%ecx
	je	EXT(delete_kept_space)
	
	testl	$0,%ecx
	jnz	EXT(sepuku)
	
	/* The address space may be a constructor, in which case we need
	   to invoke it to obtain the true process address space. Check the
	   type of the address space key -- if it alleges to be a  
	   constructor, believe it and invoke it to get the real thing. */
	
	movl	$kt_addr_space,%ebp	/* invocation descriptor */
	
	movl	$OC_capros_key_getType,%eax	/* r0 */
	xorl	%ebx,%ebx		/* r1 */
	xorl	%ecx,%ecx		/* r2 */
	xorl	%edx,%edx		/* r3 */
	
	int	$0x31			/* invocation trap */

	/* check the KT value */
	cmpl	$AKT_ConstructorRequestor,%ebx
	jnz	EXT(perform_lobotomy)

	/* Run the constructor for the address space */
	
	movl	$fab_space,%ebp	/* invocation descriptor */
	
	movl	$OC_Constructor_Request,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	/* check return value */
	testl	$0,%eax
	jnz	EXT(delete_kept_space)

LEXT(perform_lobotomy)
	/* Fetch the program counter of this process out
	   of the startup key that holds it.  If we had just
	   one more register, we wouldn't have to get the #$)*^%
	   thing from a key */
	
	movl	$pc_value,%ebp
	
	movl	$OC_capros_Number_get,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	/* New program PC is now in %ebx.  Switch to the real
	   address space and proceed... */
	
	movl	$swap_space_and_pc,%ebp
	movl	$OC_Process_SwapMemory32,%eax	/* r0 */
	/* inherit %ebx from return from above, which is new PC */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */

	/* THIS POINT SHOULD NOT BE REACHED -- WE ARE NOW RUNNING THE
	   APPLICATION'S CODE */
	hlt
	
LEXT(delete_small_space)
	
	movl	$sb_reclaim_pages_from_node,%ebp
	
	movl	$OC_capros_SpaceBank_ReclaimDataPagesFromNode,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */
	
	int	$0x31			/* invocation trap */

	/* Sell back the node itself */
	movl	$sb_reclaim_small_space_node,%ebp
	
	movl	$OC_capros_SpaceBank_free1,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */
	
	int	$0x31			/* invocation trap */

	jmp	EXT(sepuku)
	
LEXT(delete_kept_space)
	movl	$destroy_kept_space,%ebp
	
	movl	$OC_capros_key_destroy,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */
	
	int	$0x31			/* invocation trap */

	
	
LEXT(sepuku)
	/* construction of the address space failed.  Ask our process
	   creator to blow us away and report failure to the user */
	
	movl	$destroy_caller,%ebp
	
	movl	$OC_ProcCre_DestroyCallerAndReturn,%eax	/* r0 */
	xorl	%ebx,%ebx			/* r1 */
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */
	
	/* THIS POINT SHOULD NOT BE REACHED -- APPLICATION NOW DEAD */
	
	hlt
