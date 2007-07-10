/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/SpaceBankKey.h>
#include <idl/capros/SpaceBank.h>

uint32_t
spcbank_buy_nodes(uint32_t krBank, uint32_t count,
                  uint32_t krTo0, uint32_t krTo1, uint32_t krTo2)
{
  switch (count) {
  case 1:
    return capros_SpaceBank_alloc1(krBank, capros_Range_otNode, krTo0);
  case 2:
    return capros_SpaceBank_alloc2(krBank,
      capros_Range_otNode | (capros_Range_otNode << 8),
      krTo0, krTo1);
  case 3:
    return capros_SpaceBank_alloc3(krBank,
      capros_Range_otNode | ((capros_Range_otNode | (capros_Range_otNode << 8)) << 8),
      krTo0, krTo1, krTo2);
  default:
    return RC_capros_key_RequestError;
  }
}

