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

#if __GNUC__ <= 2

/* The following goop is needed to keep g++ happy.
 * The purpose of this file is to be hauled in by the first pass
 * through collect.  This leads to an empty constructor list,
 * which allows the first pass to compile successfully.
 *
 * Collect then sticks its own __CTOR_LIST__ in the front of the
 * second pass, in effect hiding this one.
 *
 * What a crock!
 */

/*  Declare a pointer to void function type.  */

typedef void (*func_ptr) (void);

/* Declare the set of symbols use as begin and end markers for the lists
   of global object constructors and global object destructors.  */

#ifdef __ELF__

/* Force cc1 to switch to .data section, so that the following ASM op
   will place __CTOR_LIST__ in the .ctors section:  */
static func_ptr force_to_data[0] __attribute__ ((unused)) = { } ;

asm (".section\t.ctors,\"aw\"");	/* cc1 doesn't know that we are switching! */

#endif

static func_ptr __CTOR_LIST__[1] __attribute__ ((unused)) = { (func_ptr) (-1) };
#endif /* __GNUC__ <= 2 */
