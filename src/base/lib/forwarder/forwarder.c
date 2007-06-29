/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime distribution.
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
#include <idl/eros/SpaceBank.h>
#include <domain/Runtime.h>
#include "forwarder.h"

uint32_t 
forwarder_create(uint32_t bank, uint32_t opaque_key, 
	         uint32_t nonopaque_key, uint32_t target_key, 
	         uint32_t flags, uint32_t value)
{
  uint32_t result;

  result = eros_SpaceBank_alloc1(bank, eros_Range_otForwarder, 
             nonopaque_key);
  if (result != RC_OK) return result;

  result = eros_Forwarder_swapTarget(nonopaque_key, target_key, KR_VOID);
  if (result != RC_OK) return result;

  if (flags & eros_Forwarder_sendWord) {
    result = eros_Forwarder_swapDataWord(nonopaque_key, value, &value);
    if (result != RC_OK) return result;
  }

  result = eros_Forwarder_getOpaqueForwarder(nonopaque_key,
             flags, opaque_key);
  return result;
}
