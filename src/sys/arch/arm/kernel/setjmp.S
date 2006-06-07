/*
 * Copyright (C) 2006, Strawberry Development Group.
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
	
#include <eros/arch/arm/asm.h>

#ifdef OPTION_DDB	// these are only used by ddb

	.text

/*
typedef struct
{
  uint32_t r4tor11[10];
  uint32_t sp, pc;
} jmp_buf[1];
 */

/* int setjmp(jmp_buf); */
ENTRY(setjmp)
	stmia r0,{r4-r11,sp,lr}
	mov r0,#0	// return 0 the first time
	mov pc,lr

/* void longjmp(jmp_buf, int value) NORETURN; */
ENTRY(longjmp)
	ldmia r0,{r4-r11,sp,lr}
	movs r0,r1	// value to return
        moveq r0,#1	// if r1 is zero, return 1
	mov pc,lr
#endif
