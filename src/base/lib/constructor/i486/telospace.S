	.file "telospace.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* telospace -- trampoline code that lets a process destroy itself and
its own address space and return. 
This code runs in a read-only address space.
This code shares a page with protospace, because it can. */

#include <eros/machine/asm.h>
	
#include <eros/Invoke.h>

#include <idl/capros/SpaceBank.h>
#include <idl/capros/ProcCre.h>

#include <domain/ProtoSpaceDS.h>
#include <domain/Runtime.h>

/* This file is compiled into its own sections so we can load it
at a known address. */
#define rodataSect .rodatatelo
#define textSect .texttelo
	
#define KEYS(k0, k1, k2, k3) (k0 + k1 << 8 + k2 << 16 + k3 << 24)
	
#define NO_KEYS KEYS(KR_VOID, KR_VOID, KR_VOID, KR_VOID)
	
	.section rodataSect
	
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

destroy_caller:	
	.long	IT_Call
	.long	KR_CREATOR
	.long	0			/* snd len */
	.long	0			/* snd ptr */
	.long	KEYS(KR_BANK,KR_RETURN,KR_VOID,KR_VOID)	/* snd keys */
	.long	0			/* rcv len */
	.long	0			/* rcv data */
	.long	NO_KEYS			/* rcv keys */

	.section textSect
ENTRY(_start)
ENTRY(telospacestart)

/* We get here from protospace_destroy_small(), which calls:
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF, PSKR_PROTO,
    0x400,      // well known telospace address
    finalResult,    // w2_in
    &w1_out,
    PSKR_SPACE);

  The passed finalResult is returned in w1, which is placed in %ebx. */

	/* We have no stack, so we can save the finalResult in %esp. */
	movl	%ebx,%esp
	
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

	/* Ask our process creator to blow us away and return to the user */
	
	movl	$destroy_caller,%ebp
	
	movl	$OC_capros_ProcCre_destroyCallerAndReturn,%eax	/* r0 */
	movl	%esp,%ebx			// snd_w1 from finalResult
	xorl	%ecx,%ecx			/* r2 */
	xorl	%edx,%edx			/* r3 */

	int	$0x31			/* invocation trap */
	
	/* THIS POINT SHOULD NOT BE REACHED -- APPLICATION NOW DEAD */
	
	hlt
