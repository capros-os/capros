/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
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


#include <string.h>
#include <eros/target.h>
#include <domain/domdbg.h>
#include "assert.h"
#include "misc.h"
#include "spacebank.h"

#ifndef NDEBUG
int __assert(const char *expr, const char *file, int line)
{
  kpanic(KR_OSTREAM, "%s:%d: Assertion failed: '%s'\n",
	   file, line, expr);
  return 0;
}
#endif
