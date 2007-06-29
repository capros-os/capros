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
#include <domain/NFileKey.h>

uint32_t
nfile_read(uint32_t krFile, uint32_t at, uint32_t len,
	   void *buf, uint32_t *outLen)
{
  uint32_t result = RC_OK;
  uint32_t resid = len;
  uint8_t *inbuf = buf;

  Message msg;

  msg.snd_invKey = krFile;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = OC_NFile_Read;

  msg.rcv_key0 = KR_VOID;	/* no keys returned */
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;

  while (resid && result == RC_OK) {
    uint32_t rqLen = resid;

    if (rqLen > EROS_MESSAGE_LIMIT)
      rqLen = EROS_MESSAGE_LIMIT;
    
    /* msg.snd_w3 changes with each call. */  
    msg.snd_w1 = rqLen;
    msg.snd_w2 = at;

    msg.rcv_data = inbuf;
    msg.rcv_limit = rqLen;

    result = CALL(&msg);

    resid -= msg.rcv_limit;
    inbuf += msg.rcv_limit;
    at += msg.rcv_limit;
  }

  *outLen = len - resid;
  return result;
}
