/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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
#include <domain/SessionKey.h>

#include <idl/eros/key.h>

uint32_t
session_new_window(uint32_t session_key, 
		   uint32_t client_bank_key,
		   uint32_t parent_id,
		   uint32_t parent_location_x,
		   uint32_t parent_location_y,
		   uint32_t width,
		   uint32_t height,
		   uint32_t decorations,
		   /* out */ uint32_t *window_id,
		   uint32_t window_addrspace_key)
{
  Message m;
  uint32_t result = RC_eros_key_RequestError; /* until proven otherwise */
  uint32_t send_data[5] = {parent_location_x,
			   parent_location_y,
			   width,
			   height,
			   decorations};

  m.snd_invKey = session_key;
  m.snd_code = OC_Session_NewWindow;
  m.snd_key0 = client_bank_key;
  m.snd_key1 = KR_VOID;
  m.snd_key2 = KR_VOID;
  m.snd_rsmkey = KR_VOID;
  m.snd_data = send_data;
  m.snd_len = sizeof(send_data);
  m.snd_w1 = parent_id;
  m.snd_w2 = 0;
  m.snd_w3 = 0;

  m.rcv_rsmkey = KR_VOID;
  m.rcv_key0 = window_addrspace_key;
  m.rcv_key1 = KR_VOID;
  m.rcv_key2 = KR_VOID;
  m.rcv_data = 0;
  m.rcv_limit = 0;
  m.rcv_code = 0;
  m.rcv_w1 = 0;
  m.rcv_w2 = 0;
  m.rcv_w3 = 0;

  result = CALL(&m);
  if (result == RC_OK) {
    if (window_id != NULL)
      *window_id = m.rcv_w1;
  }

  return result;
}
