	.file	"crt0.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <eros/i486/asm.h>
#include <eros/i486/target-asm.h>
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
ENTRY(_start)
	movl	EXT(__rt_runtime_hook),%eax
	cmpl	$0,%eax
	jz	1f

	jmp	*%eax
	
	
1:
	movl	EXT(__rt_stack_pointer),%ESP
	
	call	EXT(__rt_setup_keeper)
		
	call	EXT(__domain_init)	/* so we can __destroy() later */

#if defined(__ELF__) && 0
	/* someday support C++ */
	call	EXT(__main)
#endif
        
	pushl	$0	/* envp -- ANSI */
	pushl	$0	/* argv */
	pushl	$0	/* argc */

	call EXT(main)

LEXT(__exit)
	pushl	%eax
	/* we have exited.  destroy() ourselves: */
	call EXT(__domain_destroy)
	
	/* SHOULD NEVER GET HERE, BUT JUST IN CASE */
	hlt	

	.section	.keydefs,"aw"
	
        .align 4
        .type    __KEYDEF_LIST_START__,@object
        /* .size    __KEYDEF_LIST_START__,4 */
__KEYDEF_LIST_START__:
