/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/cap-instr.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpace.h>

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
  (void) process_copy(KR_SELF, ProcAddrSpace, PSKR_SPACE);
  
  {
    Message msg;
    msg.snd_key0 = PSKR_PROTO;
    msg.snd_key1 = KR_VOID;
    msg.snd_key2 = KR_VOID;
    msg.snd_rsmkey = KR_VOID;
    msg.snd_w1 = 0;		/* well known protospace address */
    msg.snd_w2 = (smallSpace ? 2 : 1);
    msg.snd_w3 = 0;
    msg.snd_data = 0;
    msg.snd_len = 0;

    msg.rcv_key0 = KR_VOID;
    msg.rcv_key1 = KR_VOID;
    msg.rcv_key2 = KR_VOID;
    msg.rcv_rsmkey = KR_VOID;
    msg.rcv_limit = 0;		/* no data returned */

    /* No string arg == I'll take anything */
    msg.snd_invKey = KR_SELF;
    msg.snd_code = OC_Process_SwapMemory32;

    CALL(&msg);
  }
  /* NOTREACHED */
}
