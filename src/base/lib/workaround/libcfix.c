/*
 * Copyright (C) 2009, Strawberry Development Group.
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

/* This file contains a workaround for a bug in the version of newlib
in the cross-tools at
http://www.coyotos.org/YUM/coyotos/6/i386/Coyotos-Repository-3.2-2.fc6.noarch.rpm
as of May 1 2009.

stdint.h defines SIZE_MAX as 0x7fffffff. It should be 0xffffffff.
At the moment, no CapROS code uses SIZE_MAX, so there is no workaround for this.

The libc procedure strstr() may make an invalid memory reference.
This is because it was built using the incorrect SIZE_MAX.
See http://sourceware.org/ml/newlib/2008/msg00516.html and related messages
for an explanation.

This bug occurs in the arm cross-tools. 
I don't know whether it occurs in the x86 cross-tools.

Here is a brute-force implementation of strstr()
that will override the one in libc. */

#include <string.h>

char *
strstr(const char * haystack, const char * needle)
{
  const char * p;
  for (p = haystack; ; p++) {
    // Does the needle match at position p?
    const char * q;
    const char * r;
    for (q = needle, r = p; *q; q++, r++) {
      if (! *r)
        return NULL;
      if (*q != *r)
        goto nextpos;		// no match at this position
    }
    return (char *)p;		// match here
nextpos: ;
  }
}
