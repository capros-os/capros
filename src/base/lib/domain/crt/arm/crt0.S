	.file	"crt0.S"
/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <eros/arch/arm/asm.h>
#include <eros/arch/arm/target-asm.h>
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
   
	.text
ENTRY(_start)
	ldr r0,=__rt_runtime_hook
	ldr r0,[r0]
	ldr r1,=__rt_stack_pointer
	cmp r0,#0
	movne pc,r0

	ldr sp,[r1]
	bl __rt_setup_keeper
	bl __domain_init	/* so we can __destroy() later */

#if defined(__ELF__) && 0
	bl __main	/* someday support C++ */
#endif

	mov r0,#0	/* argc */
	mov r1,#0	/* argv */
	mov r2,#0	/* envp -- ANSI */
	bl main
        
__exit:
	/* we have exited.  destroy() ourselves: */
	bl __domain_destroy
	
	/* SHOULD NEVER GET HERE, BUT JUST IN CASE */
	b .
