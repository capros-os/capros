#ifndef __INTERPRETERDESTROY_H__
#define __INTERPRETERDESTROY_H__

/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

#include <idl/capros/Process.h>
#include <domain/Runtime.h>

/* This procedure loads a (read-only) address space built with
InterpreterTable.h and begins execution at the destruction entry point.

The following standard slots must be set up:
KR_SELF    - the running process
plus any slots required by the destruction sequence.

telospaceCap is the key register containing a key to the interpreter
address space.

oldSpaceCap is the key register to receive a key to the current
address space.

finalResult is the return code to be used by the MsgDestroyProcess macro.
No other data or keys may be returned.
*/

INLINE void
InterpreterDestroy(cap_t telospaceCap,
  cap_t oldSpaceCap,
  result_t finalResult)
{
  /* The following invocation replaces our own address space and
  changes our PC, therefore the code after the invocation is never executed. 
  w2_in is received in a register, and w1_out is never used. */
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF,
    telospaceCap,
    0x20,	// well-known address of destruction interpreter
    finalResult,
    NULL,	// won't be received by this code
    oldSpaceCap);
  /* NOTREACHED */
}

#endif /* __INTERPRETERDESTROY_H__ */
