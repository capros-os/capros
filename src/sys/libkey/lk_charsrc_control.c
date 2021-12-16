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
#include <eros/CharSrcProto.h>

/* Wait for a tty event */
uint32_t
charsrc_control(uint32_t krCharSrc, uint32_t controlCode,
		uint32_t sndReg1, uint32_t sndReg2,
		const void *sndData, uint32_t sndLen,
		uint32_t *rcvReg1, uint32_t *rcvReg2,
		void *rcvData, uint32_t maxRcvLen,
		uint32_t *rcvLen)
{
  Message msg;
  uint32_t retval;
  
  msg.snd_w1 = controlCode;
  msg.snd_w2 = sndReg1;
  msg.snd_w3 = sndReg2;
  
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = sndLen;
  msg.snd_data = sndData;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = maxRcvLen;
  msg.rcv_data = rcvData;
  
  msg.snd_invKey = krCharSrc;
  msg.snd_code = OC_CharSrc_Control;

  retval = CALL(&msg);
 
  if (retval == RC_OK) {
    if (rcvReg1) *rcvReg1 = msg.rcv_w2;
    if (rcvReg2) *rcvReg2 = msg.rcv_w3;
    if (rcvLen)  *rcvLen = msg.rcv_limit;
  }
  return retval;
}
