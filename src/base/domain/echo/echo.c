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
 */

#include <stddef.h>
#include <eros/target.h>
#include <domain/Runtime.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <stdlib.h>

#define min(a,b) ((a) <= (b) ? (a) : (b))

#define KR_RK0		KR_ARG(0)
#define KR_RK1		KR_ARG(1)
#define KR_RK2		KR_ARG(2)

int
ProcessRequest(Message *msg)
{
  msg->snd_len = min(msg->rcv_limit, msg->rcv_sent);
  msg->snd_data = msg->rcv_data;

  msg->snd_key0 = msg->rcv_key0;
  msg->snd_key1 = msg->rcv_key1;
  msg->snd_key2 = msg->rcv_key2;
  msg->snd_rsmkey = msg->rcv_rsmkey;

  msg->snd_code = msg->rcv_code;
  msg->snd_w1 = msg->rcv_w1;
  msg->snd_w2 = msg->rcv_w2;
  msg->snd_w3 = msg->rcv_w3;

  return 1;
}

int
main()
{
  Message msg;
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

  msg.rcv_key0 = KR_RK0;
  msg.rcv_key1 = KR_RK1;
  msg.rcv_key2 = KR_RK2;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = (void *) 0x22000;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  msg.rcv_limit = EROS_PAGE_SIZE;

  do {
    RETURN(&msg);
    msg.snd_invKey = msg.rcv_rsmkey;
  } while ( ProcessRequest(&msg) );

  return 0;
}
