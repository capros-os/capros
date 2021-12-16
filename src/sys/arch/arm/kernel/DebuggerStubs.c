/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* Stub versions of debugger interfaces, so that we don't have
 * to do a full recompile to get a debugging-enabled kernel:
 */

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/Debug.h>
#include <kerninc/SymNames.h>

#ifdef OPTION_DDB
uint32_t funcSym_count = 0;
struct FuncSym funcSym_table[0];	/* defined in symnames.s */

uint32_t lineSym_count = 0;
struct LineSym lineSym_table[0];	/* defined in symnames.s */

void 
debug_Backtrace(const char *msg, bool shouldHalt)
{
  if (msg)
    printf("%s\n", msg);
  else
    printf("Stub backtrace called\n");
  
  if (shouldHalt)
    halt('a');
}
#endif

