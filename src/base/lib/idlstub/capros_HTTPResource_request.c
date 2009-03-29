/*
 * Copyright (C) 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/HTTPResource.h>

result_t
capros_HTTPResource_request(cap_t _self,
  capros_HTTPResource_HTTPVersion httpVersion,
  capros_HTTPResource_Method m,
  uint32_t lengthOfPath, uint32_t lengthOfPathAndQuery, uint8_t *pathAndQuery,
  capros_HTTPResource_RHType *rhtype, cap_t rh,
  uint32_t * sendLimit)
{
  Message msg = {
    .snd_invKey = _self,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = lengthOfPathAndQuery,
    .snd_data = pathAndQuery,
    .snd_code = 2,//// not to conflict with File.read and File.write
    .snd_w1 = httpVersion,
    .snd_w2 = m,
    .snd_w3 = lengthOfPath,
    .rcv_key0 = rh,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID,
    .rcv_limit = 0
  };

  CALL(&msg);
  if (msg.rcv_code == RC_OK) {
    if (rhtype)
      *rhtype = msg.rcv_w1;
    if (sendLimit)
      *sendLimit = msg.rcv_w2;
  }
  return msg.rcv_code;
}
