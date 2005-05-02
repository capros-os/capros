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
#include <domain/NFileKey.h>

uint32_t
nfile_write(uint32_t krFile, uint32_t at, uint32_t len,
	    const void *buf, uint32_t *outLen)
{
  uint32_t result = RC_OK;
  uint32_t resid = len;
  const uint8_t *outbuf = buf;
  Message msg;

  msg.snd_invKey = krFile;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code = OC_NFile_Write;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
     
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
     
  while (resid && result == RC_OK) {
    uint32_t rqLen = resid;
    if (rqLen > EROS_MESSAGE_LIMIT)
      rqLen = EROS_MESSAGE_LIMIT;

    msg.snd_len = rqLen;
    msg.snd_data = outbuf;
    msg.snd_w2 = resid;
    msg.snd_w3 = at;
    
    result = CALL(&msg);

    resid -= msg.rcv_w1;
    outbuf += msg.rcv_w1;
    at += msg.rcv_w1;
  }
  
  *outLen = len - resid;
  return result;
}

