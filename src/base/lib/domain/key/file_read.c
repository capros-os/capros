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
#include <domain/FileKey.h>

uint32_t
file_read(uint32_t krFile, uint64_t offset, uint32_t len, uint8_t *buf, uint32_t *outLen)
{
  uint32_t result;
  Message msg;

  msg.snd_invKey = krFile;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = OC_File_Read;
  msg.snd_w1 = 0;
  msg.snd_w2 = len;
  msg.snd_w3 = (uint32_t) offset;
     
  msg.rcv_key0 = KR_VOID;	/* no keys returned */
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = buf;
  msg.rcv_limit = len;
     
  result = CALL(&msg);
  *outLen = msg.rcv_limit;
  return result;
}
