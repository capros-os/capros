/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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

#define ALIGN 16

#ifdef __ELF__
#define EXT(x)  x
#define LEXT(x) x## :
#else
#define EXT(x)  _##x
#define LEXT(x) _##x## :
#endif
#define GEXT(x) .globl EXT(x); LEXT(x)

#define	ALIGNEDVAR(x,al) .globl EXT(x); .align al; LEXT(x)
#define	VAR(x)		ALIGNEDVAR(x,4)
#define	ENTRY(x)	.globl EXT(x); .type EXT(x),@function; LEXT(x)
#define	GDATA(x)	.globl EXT(x); .align ALIGN; LEXT(x)

/* GNU assembler does not accept trailing 'u' in constants. */
#define _ASM_U(x) x
