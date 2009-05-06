/*
 * Copyright (C) 2009, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/HTTPRequestHandler.h>

result_t
capros_HTTPRequestHandler_headers(cap_t _self,
  uint32_t lengthOfHeaderData, uint8_t *headerData, uint32_t *sendLimit)
{
  Message msg = {
    .snd_invKey = _self,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = lengthOfHeaderData,
    .snd_data = headerData,
    .snd_code = 0,////
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID,
    .rcv_limit = 0
  };

  CALL(&msg);
  if (msg.rcv_code == RC_OK) {
    if (sendLimit)
      *sendLimit = msg.rcv_w1;
  }
  return msg.rcv_code;
}
