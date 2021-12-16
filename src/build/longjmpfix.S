	.file	"longjmpfix.S"
/*
 * Copyright (C) 2010, Strawberry Development Group.
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

/* This file contains a workaround for a bug in the version of newlib
in the cross-tools at
http://www.coyotos.org/YUM/coyotos/6/i386/Coyotos-Repository-3.2-2.fc6.noarch.rpm
as of May 1 2009.

longjmp() is configured to use cli and sti, which are illegal for
most user-mode programs.

This bug occurs only in the x86 cross-tools. 

Here is an implementation of longjmp() without cli or sti
that will override the one in libc.
Note, this version does not support returning a value of zero.
setjmp() is included because it is in the same file in libc.a.
*/

#include <eros/arch/i486/asm.h>

ENTRY(setjmp)
	push %ebp
	mov %esp,%ebp
	push %edi
	mov 8(%ebp),%edi	// address of jmp_buf
	mov %eax,  (%edi)	// save registers
	mov %ebx, 4(%edi)
	mov %ecx, 8(%edi)
	mov %edx,12(%edi)
	mov %esi,16(%edi)
	mov -4(%ebp),%eax	// saved %edi
	mov %eax,20(%edi)
	mov 0(%ebp),%eax	// saved %ebp
	mov %eax,24(%edi)
	mov %esp,%eax
	add $12,%eax		// %esp with %edi, %ebp, and ret addr popped
	mov %eax,28(%edi)
	mov 4(%ebp),%eax	// return address
	mov %eax,32(%edi)
	pop %edi
	mov $0,%eax
	leave
	ret

ENTRY(longjmp)
	push %ebp
	mov %esp,%ebp
	mov 8(%ebp),%edi	// address of jmp_buf
	mov 12(%ebp),%eax	// value to return
	mov 24(%edi),%ebp	// change to stack frame at setjmp
	// cli
	mov 28(%edi),%esp	// change to stack at setjmp
	pushl 32(%edi)		// eip to restore
	mov  4(%edi),%ebx
	mov  8(%edi),%ecx
	mov 12(%edi),%edx
	mov 16(%edi),%esi
	mov 20(%edi),%edi
	// sti
	ret
