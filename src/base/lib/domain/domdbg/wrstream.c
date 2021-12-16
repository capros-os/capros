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

/* wrstream: primitive low-level write interface */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/ConsoleKey.h>
#include <domdbg/domdbg.h>

void
wrstream(uint32_t streamKey, const char *s, uint32_t len)
{
  Message msg;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;

  msg.snd_data = (uint8_t *) s;
  msg.snd_len = len;	/* omit trailing null! */

  msg.rcv_limit = 0;		/* no data returned */

  msg.snd_code = OC_Console_Put;
  msg.snd_invKey = streamKey;
  (void) CALL(&msg);
}
