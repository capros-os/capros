/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>

#include <idl/eros/Number.h>

#include <domain/SpaceBankKey.h>
#include <domain/Runtime.h>

#include "wrapper.h"

uint32_t 
wrapper_create(uint32_t bank, uint32_t wrapper_key, 
	       uint32_t node_key, uint32_t key_to_wrap, 
	       uint32_t flags, uint32_t value1, uint32_t value2)
{
  uint32_t result;
  eros_Number_value nkv;

  result = spcbank_buy_nodes(bank, 1, wrapper_key, KR_VOID, KR_VOID);
  if (result != RC_OK)
    return result;

  result = node_make_node_key(wrapper_key, 1, 0, wrapper_key);
  if (result != RC_OK)
    return result;

  result = process_copy_keyreg(KR_SELF, wrapper_key, node_key);
  if (result != RC_OK)
    return result;

  nkv.value[0] = flags;
  nkv.value[1] = value1;
  nkv.value[2] = value2;

  result = node_write_number(wrapper_key, WrapperFormat, &nkv);
  if (result != RC_OK)
    return result;

  result = node_swap(wrapper_key, WrapperKeeper, key_to_wrap, KR_VOID);
  if (result != RC_OK)
    return result;

  result = node_make_wrapper_key(wrapper_key, 0, 0, wrapper_key);
  return result;
}

uint32_t 
wrapper_modify(uint32_t node_key, uint32_t flags, uint32_t value1, 
	       uint32_t value2)
{
  eros_Number_value nkv;

  nkv.value[0] = flags;
  nkv.value[1] = value1;
  nkv.value[2] = value2;

  return node_write_number(node_key, WrapperFormat, &nkv);
}
