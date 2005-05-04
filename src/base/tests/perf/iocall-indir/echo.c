/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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


/*
 * Echo -- domain that simply echos what you send it.
 *
 * Key Registers:
 *
 * KR12: arg0
 * KR13: arg1
 * KR14: arg2
 * KR15: arg3
 */

#include <eros/target.h>
#include <eros/Invoke.h>

/* It is intended that this should be a large space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x80001000;

#define KR_VOID 0
#define KR_RK0     12
#define KR_RK1     13
#define KR_RK2     14
#define KR_RETURN  15

static uint8_t rcvData[EROS_MESSAGE_LIMIT + EROS_PAGE_SIZE];

void
main()
{
  Message msg;
  uint32_t rcvPtr = (uint32_t) rcvData;
  rcvPtr += (EROS_PAGE_SIZE - 1);
  rcvPtr &= ~EROS_PAGE_MASK;
  
  msg.snd_invKey = KR_VOID;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = (uint8_t *) rcvPtr;
  msg.rcv_len = EROS_MESSAGE_LIMIT;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  for (;;) {
    msg.rcv_len = EROS_MESSAGE_LIMIT;
    RETURN(&msg);
    msg.snd_invKey = KR_RETURN;
  }
}
