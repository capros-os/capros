	.file	"triv_destroy.S"
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

/* triv_destroy -- stub program that destroys "iron-man" segments.  If
   your main program segment is a kept segment, this is not the right
   destroyer code for you.  If your main program segment is immutable
   except for it's stack, this is the destroyer code that you want.

   This destroyer assumes that your address space consists of a single
   node key, each slot of which contains void or page keys.  It
   fetches each slot of the node in turn.  If the key fetched is a
   read-write page key, it sells this page back to your space bank.

   Having done this, it then sells your address space node.

   Once the address space is gone it phones up your domain creator and
   says "destroy this domain."

   It proves that none of the calls we need to make take or return a
   string argument, so this code uses ESI and EDI as working storage.

   The tricky part to this code is that it must run from an
   IMMUTABLE segment!!!
   */


#include <eros/i486/asm.h>
	
#include <eros/target-asm.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/StdKeyType.h>

#include <idl/eros/key.h>

#include <domain/SpaceBankKey.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/Runtime.h>
	
#define KR_SEGNODE	0x1
#define KR_DOMCRE	0x2
#define KR_SPCBANK	0x3
#define KR_SCRATCH	0x4
#define KR_RESUME	0x5
	
#define OC eax
#define RC eax
	
	/* Call macro -- result code to EAX. */
#define CALL_OC(kr, karg, krcv) \
	movl	$kr,%ebp; 	/* key to invoke */  \
	movl	$karg,%edx; 	/* key arguments */  \
	shll	$16,%edx; 	/* shift to proper position */  \
	movl	$krcv,%ecx; 	/* key response */  \
	shll	$16,%ecx; 	/* shift to proper position */  \
	movl	$0x01000000,%ebx;   /* CALL, no strings */
	/* Call macro -- result code to EAX. */
	
#define CALL(oc, kr, karg, krcv) \
	movl	$oc,%eax; 	/* order code */  \
	CALL_OC(kr, karg, krcv)	; 


#ifdef __ELF__
ENTRY(_start)
#else
ENTRY(start)
#endif
	/* If root not a writable node key, do not destroy! */
	CALL(OC_eros_key_getType, KR_SEGNODE, KR_VOID, KR_VOID); 
	cmp	$AKT_Node,%RC
	jne	destroy_domain
	testl	$SEGMODE_WEAK,%ebx
	jnz	destroy_domain
	
return_data_pages:
	movl	$16,%esi
data_page_loop:	
	decl	%esi
	js	return_node	/* test sign bit for negative number */

	movl	$OC_Node_Copy,%OC
	movl	$0,%ebx
	addl	%esi,%OC
	/* retrieve the key... */
	CALL_OC(KR_SEGNODE, KR_VOID, KR_SCRATCH);
	
	/* fetch it's alleged key type */
	CALL(OC_eros_key_getType, KR_SCRATCH, KR_VOID, KR_VOID); 
	cmp	$AKT_Page,%RC
	jne	data_page_loop

	/* Sell this page back to the space bank */
	CALL(OC_SpaceBank_Reclaim1DataPage, KR_SPCBANK, KR_SCRATCH, KR_VOID)

	jmp	data_page_loop

return_node:
	
	/* Sell this node back to the space bank */
	CALL(OC_SpaceBank_Reclaim1Node, KR_SPCBANK, KR_SEGNODE, KR_VOID)


destroy_domain:
	/* Sell this node back to the space bank */
	CALL(OC_ProcCre_DestroyCallerAndReturn, KR_SPCBANK, \
		(KR_RESUME<<8)|KR_SPCBANK, KR_VOID)

	/* NOTREACHED */
