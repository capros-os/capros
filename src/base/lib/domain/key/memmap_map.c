/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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
#include <domain/MemmapKey.h>

#include <idl/eros/key.h>

uint32_t
memmap_map(uint32_t memmap_key, uint32_t physrange_key, 
	   uint64_t base_addr, uint64_t size,
	   uint16_t *lss)
{
  Message m;
  uint32_t result = RC_eros_key_RequestError; /* until proven otherwise */
  uint32_t send_data[4];

  m.snd_invKey = memmap_key;
  m.snd_code = OC_Memmap_Map;
  m.snd_key0 = KR_VOID;		/* don't overwrite the keeper key */
  m.snd_key1 = physrange_key;
  m.snd_key2 = KR_VOID;		/* don't overwrite the wrapper node key */
  m.snd_rsmkey = KR_VOID;
  m.snd_data = send_data;
  m.snd_len = sizeof(send_data);
  m.snd_w1 = 0;
  m.snd_w2 = 0;
  m.snd_w3 = 0;

  m.rcv_rsmkey = KR_VOID;
  m.rcv_key0 = KR_VOID;
  m.rcv_key1 = KR_VOID;
  m.rcv_key2 = KR_VOID;
  m.rcv_data = 0;
  m.rcv_limit = 0;
  m.rcv_code = 0;
  m.rcv_w1 = 0;
  m.rcv_w2 = 0;
  m.rcv_w3 = 0;

  send_data[0] = (uint32_t)(base_addr & 0xffffffff);
  send_data[1] = (uint32_t)(base_addr >> 32);
  send_data[2] = (uint32_t)(size & 0xffffffff);
  send_data[3] = (uint32_t)(size >> 32);

  result = CALL(&m);
  if (result == RC_OK) {
    if (lss) *lss = (uint16_t)m.rcv_w1;
  }

  return result;
}
