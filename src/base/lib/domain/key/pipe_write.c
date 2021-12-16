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
#include <domain/PipeKey.h>

uint32_t
pipe_write(uint32_t krPipe, uint32_t len, const uint8_t *buf, uint32_t *outLen)
{
  uint32_t result;
  uint32_t resid = len;
  const uint8_t *outbuf = buf;

  static Message msg = {
    IT_Call,
    KR_VOID,			/* krPipe -- overwritten */
    0,				/* snd_len -- overwritten */
    0,				/* snd_data -- overwritten */
    KR_VOID,			/* snd_key0 */
    KR_VOID,			/* snd_key1 */
    KR_VOID,			/* snd_key2 */
    KR_VOID,			/* snd_rsmkey */

    0,				/* rcv_limit -- overwritten */
    0,				/* rcv_data -- overwritten */
    KR_VOID,			/* rcv_key0 */
    KR_VOID,			/* rcv_key1 */
    KR_VOID,			/* rcv_key2 */
    KR_VOID,			/* rcv_rsmkey */

    OC_Pipe_Write,		/* snd_code */
    0,				/* snd_w1 */
    0,				/* snd_w2 */
    0,				/* snd_w3 */
    
    0,				/* rcv_code */
    0,				/* rcv_w1 */
    0,				/* rcv_w2 */
    0,				/* rcv_w3 */
    0,				/* rcv_keyInfo */
  };

  msg.snd_invKey = krPipe;
     
  do {
    uint32_t rqLen = resid;
    if (rqLen > PIPE_BUF_SZ)
      rqLen = PIPE_BUF_SZ;

    msg.snd_len = rqLen;
    msg.snd_data = outbuf;
    msg.snd_w2 = resid;
    
    result = CALL(&msg);

    resid -= msg.rcv_w1;
    outbuf += msg.rcv_w1;
  } while (resid && result == RC_OK);
  
  *outLen = len - resid;
  return result;
}

