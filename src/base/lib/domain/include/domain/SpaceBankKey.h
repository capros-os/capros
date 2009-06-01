#ifndef __SPACEBANKKEY_H__
#define __SPACEBANKKEY_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* This file has been superceded by the IDL-generated SpaceBank.h.
   This file remains temporarily for compatibility. */

#include <idl/capros/SpaceBank.h>

/* error defines */
#define RC_SB_LimitReached     RC_capros_SpaceBank_LimitReached
	/* spacebank limit reached */

#ifndef __ASSEMBLER__

uint32_t spcbank_buy_nodes(uint32_t krBank, uint32_t count,
			   uint32_t krTo0,  uint32_t krTo1,
			   uint32_t krTo2);
uint32_t spcbank_buy_data_pages(uint32_t krBank, uint32_t count,
				uint32_t krTo0, uint32_t krTo1,
				uint32_t krTo2);

INLINE uint32_t spcbank_return_node(uint32_t krBank, uint32_t krNode)
{
  return capros_SpaceBank_free1(krBank, krNode);
}

INLINE uint32_t spcbank_return_data_page(uint32_t krBank, uint32_t krPage)
{
  return capros_SpaceBank_free1(krBank, krPage);
}

INLINE uint32_t spcbank_create_subbank(uint32_t krBank, uint32_t krNewBank)
{
  return capros_SpaceBank_createSubBank(krBank, krNewBank);
}

INLINE uint32_t spcbank_destroy_bank(uint32_t krBank, uint32_t andSpace)
{
  if (andSpace)
    return capros_SpaceBank_destroyBankAndSpace(krBank);
  else
    return capros_key_destroy(krBank);
}

INLINE uint32_t spcbank_verify_bank(uint32_t krBank, uint32_t krPurportedBank,
			     uint32_t * isGood)
{
  return capros_SpaceBank_verify(krBank, krPurportedBank, isGood);
}

#endif

#endif /* __SPACEBANKKEY_H__ */

