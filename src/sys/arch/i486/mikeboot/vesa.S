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

	.file	"vesa.S"

#include <eros/i486/asm.h>

/*
 * VESA Function 00h
 *
 * AsmGetVbeControllerInfo(VbeInfoBlock) -- query the graphics card for
 *					    its VESA capabilities
 *			     VbeInfoBlock:  struct which will be populated
 */

ENTRY(AsmGetVbeControllerInfo)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%edi

	/* Grab the passed pointer (first arg): */
	movl	0x8(%ebp),%edi

	call	EXT(prot_to_real)	/* enter real mode */

	.code16

	movb	$0x4f, %ah
	movb	$0x0, %al

	sti
	int	$0x10
	cli

	movw	%ax,%bx

	call	EXT(real_to_prot)

	.code32

	xorl	%eax,%eax
	movw	%bx,%ax

	pop	%edi
	pop	%ebx
	pop	%ebp
	ret

/*
 * VESA Function 01h
 *
 * AsmGetVbeModeInfo(mode,modeInfo*) -- query the graphics card for info about
 *			      a specific VESA mode
 *		       mode:  mode number to ask about
 */

ENTRY(AsmGetVbeModeInfo)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%ecx
	push	%edi

	/* Grab the passed mode number (first arg): */
	movl	0x8(%ebp),%ecx
	/* Grab the passed pointer (second arg): */
	movl	0xc(%ebp),%edi

	call	EXT(prot_to_real)	/* enter real mode */

	.code16

	movb	$0x4f, %ah
	movb	$0x1, %al

	sti
	int	$0x10
	cli

	movw	%ax,%bx

	call	EXT(real_to_prot)

	.code32

	xorl	%eax,%eax
	movw	%bx,%ax

	pop	%edi
	pop	%ecx
	pop	%ebx
	pop	%ebp
	ret

/*
 * VESA Function 02h
 *
 * AsmSetVbeMode(mode, CRTCInfoBlock) -- set the graphics card to a
 *					 specific VESA mode
 *				   mode: mode number to set card to
 *			  CRTCInfoBlock: struct containing refresh rate
 *					 specs (optional; see VESA docs)
 */

ENTRY(AsmSetVbeMode)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%edi

	/* Grab the passed mode number (first arg): */
	movl	0x8(%ebp),%ebx
	/* Grab the passed pointer (second arg): */
	movl	0xC(%ebp),%edi

	call	EXT(prot_to_real)	/* enter real mode */

	.code16

	movb	$0x4f, %ah
	movb	$0x2, %al

	sti
	int	$0x10
	cli

	movw	%ax,%bx

	call	EXT(real_to_prot)

	.code32

	xorl	%eax,%eax
	movw	%bx,%ax

	pop	%edi
	pop	%ebx
	pop	%ebp
	ret

/*
 * VESA Function 03h
 *
 * AsmReturnCurrentVbeMode() -- return the current VBE mode number
 *
 *                      Return: returns current VBE mode number
 *                        Note: does NOT return standard VBE return value
 */

ENTRY(AsmReturnCurrentVbeMode)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx

	call	EXT(prot_to_real)	/* enter real mode */

	.code16

	movb	$0x4f, %ah
	movb	$0x3, %al

	sti
	int	$0x10
	cli

/*	movw %ax, %bx */

	call	EXT(real_to_prot)

	.code32

	xorl	%eax,%eax
	movw	%bx,%ax

	pop	%ebx
	pop	%ebp
	ret

/*
 * VESA Function 05h
 *
 * AsmDisplayWindowControl(getset, AorB, winNum) -- Control display window
 *					    getset: 0: set mem window
 *					            1: get mem window
 *					      AorB: 0: Window A
 *						    1: Window B
 *					    winNum: window number in
 *						    window granularity units
 * Return of current window not yet supported.
 */

ENTRY(AsmDisplayWindowControl)
	push	%ebp
	mov	%esp, %ebp
	push	%ebx
	push	%edx

	xorl	%ebx,%ebx
	xorl	%edx,%edx
	/* Grab the first arg (set/get):  */
	movb	0x8(%ebp),%bh
	/* Grab the second arg (win A/B): */
	movb	0xC(%ebp),%bl
	/* Grab the third arg (win num):  */
	movw	0x10(%ebp),%dx

	call	EXT(prot_to_real)	/* enter real mode */

	.code16

	movb	$0x4f, %ah
	movb	$0x5, %al

	sti
	int	$0x10
	cli

	movw	%ax,%bx

	call	EXT(real_to_prot)

	.code32

	xorl	%eax,%eax
	movw	%bx,%ax

	pop	%edx
	pop	%ebx
	pop	%ebp
	ret
