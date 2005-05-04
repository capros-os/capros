	.file "fbfault.S"
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* fbfault -- small piece of trampoline code that is used when
 * the frame buffer manager takes a fault while accessing a
 * client-supplied window content address space. Generally,
 * such a fault means that the client has reclaimed the space.
 */

#include <eros/i486/asm.h>

	.text
ENTRY(fbfault_trampoline)
	/* When we pick up execution here in fbfault, it means that
	 * the keeper has hand-reset our PC to here. Push the 
	 * jmp_buf address, arrange for longjmp to return 1,
	 * and call longjmp. */

	pushl	$1
	pushl	$EXT(fbfault_jmp_buf)
	call	EXT(longjmp)
