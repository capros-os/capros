/*
 * Copyright (C) 2007, 2009, Strawberry Development Group.
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

#include <domain/ProtoSpaceDS.h>

void
protospace_destroy_small(uint32_t krProto, uint32_t retCode)
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
