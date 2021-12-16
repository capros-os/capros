/*
 * Copyright (C) 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <sys/types.h>

struct _reent;

void *
_sbrk(ptrdiff_t incr)
{
  extern char end;		/* Defined by the linker */
  static char * heap_end;
  char * prev_heap_end;

  if (heap_end == 0) {
    heap_end = (char *)((((ptrdiff_t) &end) + 7) & ~(ptrdiff_t)7);
  }
  prev_heap_end = heap_end;
  /* FIXME: Check that we aren't running into the stack. */
  heap_end += incr;

  return prev_heap_end;
}
