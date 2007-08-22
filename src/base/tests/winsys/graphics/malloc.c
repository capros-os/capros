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

/* design assumptions at end */

#include <stddef.h>
#include <eros/target.h>
#include <eros/KeyConst.h>
#include <domain/Runtime.h>
#include <eros/Invoke.h>
#include <domain/domdbg.h>

#include <stdlib.h>

#define ROUND_UP(x, y) ( ((x) % (y)) ? ( ((x) % (y)) + (y) ) : (x) )
extern void end();
static uint32_t top = (uint32_t) &end;

void *
malloc(size_t sz)
{
  uint32_t ptr;

  top = ROUND_UP(top,sizeof(uint32_t));
  ptr = top;
  top += sz;
  return (void*) ptr;
}
