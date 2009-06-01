/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/cap-instr.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpace.h>
#include <idl/capros/Process.h>

/* NOTE: This code assumes that the resume key to which we should
   return is sitting in KR_RESUME. */
void
protospace_destroy(uint32_t krReturner, uint32_t krProto, uint32_t krMyDom,
		   uint32_t krMyProcCre,
		   uint32_t krBank, int smallSpace)
{
  /* Problem is to permute 4 keys into their proper positions.  What a
     pain in the butt! */

  /* Deal with the bank: */
  if (krBank != KR_BANK) {
    XCHG_KEYREG(krBank, KR_BANK);

    /* Now update the other arg locs to reflect this swap */
    if (krProto == KR_BANK)
      krProto = krBank;
    if (krMyDom == KR_BANK)
      krMyDom = krBank;
    if (krMyProcCre == KR_BANK)
      krMyProcCre = krBank;

    krBank = KR_BANK;
  }

  if (krMyProcCre != KR_CREATOR) {
    XCHG_KEYREG(krMyProcCre, KR_CREATOR);

    if (krProto == KR_CREATOR)
      krProto = krMyProcCre;
    if (krMyDom == KR_CREATOR)
      krMyDom = krMyProcCre;

    krMyProcCre = KR_CREATOR;
  }

  if (krMyDom != KR_SELF) {
    XCHG_KEYREG(krMyDom, KR_SELF);

    if (krProto == KR_SELF)
      krProto = krMyDom;

    krMyDom = KR_SELF;
  }

  if (krProto != PSKR_PROTO) {
    XCHG_KEYREG(krProto, PSKR_PROTO);
    krProto = PSKR_PROTO;
  }

  /* Fetch out the current address space to key reg 3: */
  (void) capros_Process_getAddrSpace(KR_SELF, PSKR_SPACE);
  
  uint32_t w1_out;

  /* The following invocation replaces our own address space
  and changes our PC, therefore the part of the stub after
  the invocation is never executed. 
  w2_in is received in a register, and w1_out is never used. */
  capros_Process_swapAddrSpaceAndPC32Proto(KR_SELF, PSKR_PROTO,
    0,	/* well known protospace address */
    (smallSpace ? 2 : 1),	// w2_in
    &w1_out,		// won't be used
    KR_VOID);
  /* NOTREACHED */
}
