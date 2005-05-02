	.file	"crtn.S"
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
	
#undef ALIGN
# define ALIGN 4

#if 0
# ifdef __ELF__
#  define EXT(x)  x
#  define LEXT(x) x## :
# else
#  define EXT(x)  _##x
#  define LEXT(x) _##x## :
# endif
# define	ENTRY(x)	.globl EXT(x); .align ALIGN; LEXT(x)
#endif
	
	.data
	
	/* Weak runtime symbols */
	
	.weak	__rt_runtime_hook
	.align 4
	.type	__rt_runtime_hook,@object
	.size	__rt_runtime_hook,4
__rt_runtime_hook:
	.long	0	# default: no runtime hook
	
	.weak	__rt_stack_pointer
	.align 4
	.type	__rt_stack_pointer,@object
	.size	__rt_stack_pointer,4
__rt_stack_pointer:
	.long	0x80000000	/* default: 2G, leaving 1G for mappings */
	
	.weak	__rt_stack_pages
	.align 4
	.type	__rt_stack_pages,@object
	.size	__rt_stack_pages,4
__rt_stack_pages:
	.long	0	/* default: keeper allocates */
	
	.weak	__rt_small_space
	.align 4
	.type	__rt_small_space,@object
	.size	__rt_small_space,4
__rt_small_space:
	.long	0	/* default: not a small space */
	
	.weak	__rt_unkept
	.align 4
	.type	__rt_unkept,@object
	.size	__rt_unkept,4
__rt_unkept:
	.long	0	/* default: should have a keeper */
	
	.section	.keydefs,"aw"
	
        .align 4
        .type    __KEYDEF_LIST_END__,@object
        /* .size    __KEYDEF_LIST_END__,4 */
__KEYDEF_LIST_END__:
