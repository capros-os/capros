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
#include <domain/KeySetKey.h>

uint32_t
keyset_add_key(uint32_t krKeySet, uint32_t krKey, uint32_t data, /*OUT*/uint32_t *oldData)
{
  Message msg;
  uint32_t result;
  
  msg.snd_w1 = data;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.snd_key0 = krKey;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;		/* no data sent */

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = 0;		/* no data returned */

  /* No string arg == I'll take anything */
  msg.snd_invKey = krKeySet;
  msg.snd_code = OC_KeySet_AddKey;

  result = CALL(&msg);

  if (result == RC_OK) {
    if (oldData) *oldData = msg.rcv_w1;
  }
  return result;
}
