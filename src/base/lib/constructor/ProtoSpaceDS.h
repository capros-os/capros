#ifndef __PROTOSPACEDS_H__
#define __PROTOSPACEDS_H__

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

#include <idl/capros/Process.h>
#include <domain/Runtime.h>

#define PSKR_SPACE    KR_APP(0)

/* protospace_destroy_small is used by a process with a "small" address
space to destroy itself and its address space and return to a caller.

A "small" space is one with just one GPT.
The GPT and any writeable pages it contains will be returned to the bank.

The following standard slots must be set up:
KR_SELF    - the running process
KR_BANK    - the space bank to return the address space and process to
KR_CREATOR - this process's creator
KR_RETURN  - the key to which to return

krProto is the key register containing a key to the protospace address space.
krProto must be different from the four keys above,
but it may be equal to PSKR_SPACE.

retCode is the return code to be passed to KR_RETURN.
No other data or keys may be returned.
*/

#ifndef __ASSEMBLER__
INLINE void protospace_destroy_small(uint32_t krProto, uint32_t retCode)
{
  uint32_t w1_out;

  /* The following invocation replaces our own address space and
  changes our PC, therefore the code after the invocation is never executed. 
  w2_in is received in a register, and w1_out is never used. */
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF, krProto,
    0x400,	// well known telospace address
    retCode,	// w2_in
    &w1_out,	// won't be used
    PSKR_SPACE); 
  /* NOTREACHED */
}
#endif

#endif /* __PROTOSPACEDS_H__ */
