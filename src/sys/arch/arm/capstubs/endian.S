	.file	"endian.S"
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

#include <eros/arch/arm/asm.h>
#include <eros/arch/arm/target-asm.h>
	
	.text
// Swap bytes in a 32-bit unsigned long. 
ENTRY(_byte_swap_long_variable)
	eor r1,r0,r0,ror #16
	bic r1,r1,#0xff0000
	mov r0,r0,ror #8
	eor r0,r0,r1,lsr #8
	mov pc,lr

// Swap bytes in a 16-bit unsigned short.
ENTRY(_byte_swap_word_variable)
				// r0 = 0 0 A B
	orr r0,r0,r0,lsl #16	// r0 = A B A B
	mov r0,r0,lsr #8	// r0 = 0 A B A
	bic r0,r0,#0xff0000	// r0 = 0 0 B A
	mov pc,lr
