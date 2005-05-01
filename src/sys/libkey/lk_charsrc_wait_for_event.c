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

/* Wait for an event */
uint32_t
charsrc_wait_for_event(uint32_t krCharSrc,
		       uint32_t maxLen, char *buff,
		       uint32_t *charsRead,
		       uint32_t eventMask, uint32_t *eventsOccured)
{
  Message msg;
  uint32_t result;
  
  msg.snd_w1 = maxLen;
  msg.snd_w2 = eventMask;
  msg.snd_w3 = 0;
  
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0; 		/* no data sent */

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = maxLen;
  msg.rcv_data = buff;

  msg.snd_invKey = krCharSrc;
  msg.snd_code = OC_CharSrc_WaitForEvent;

  result = CALL(&msg);

  if (result == RC_OK) {
    if (charsRead) *charsRead = msg.rcv_w1;
    if (eventsOccured) *eventsOccured = msg.rcv_w2;
  }
  return result;
}

