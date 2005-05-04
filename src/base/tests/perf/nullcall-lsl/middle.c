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

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x20000;

#define KR_VOID 0

#define KR_ECHO    8
#define KR_JUNK    10

#define KR_RK0     12
#define KR_RK1     13
#define KR_RK2     14
#define KR_RETURN  15

static uint8_t rcvData[EROS_PAGE_SIZE];

int
main()
{
  Message msgin;
  
  msgin.snd_invKey = KR_VOID;
  msgin.snd_key0 = KR_VOID;
  msgin.snd_key1 = KR_VOID;
  msgin.snd_key2 = KR_VOID;
  msgin.snd_rsmkey = KR_VOID;
  msgin.snd_data = 0;
  msgin.snd_len = 0;
  msgin.snd_code = 0;
  msgin.snd_w1 = 0;
  msgin.snd_w2 = 0;
  msgin.snd_w3 = 0;

  msgin.rcv_key0 = KR_VOID;
  msgin.rcv_key1 = KR_VOID;
  msgin.rcv_key2 = KR_VOID;
  msgin.rcv_rsmkey = KR_RETURN;
  msgin.rcv_data = rcvData;
  msgin.rcv_len = 0;
  msgin.rcv_code = 0;
  msgin.rcv_w1 = 0;
  msgin.rcv_w2 = 0;
  msgin.rcv_w3 = 0;

  for (;;) {
    Message msgout;
    
    RETURN(&msgin);
    msgin.snd_invKey = KR_RETURN;

    msgout.snd_invKey = KR_ECHO;

    msgout.snd_key0 = KR_VOID;
    msgout.snd_key1 = KR_VOID;
    msgout.snd_key2 = KR_VOID;
    msgout.snd_rsmkey = KR_VOID;
    msgout.snd_code = 0;
    msgout.snd_w1 = 1;
    msgout.snd_w2 = 2;
    msgout.snd_w3 = 3;

    msgout.rcv_key0 = KR_JUNK;
    msgout.rcv_key1 = KR_VOID;
    msgout.rcv_key2 = KR_VOID;
    msgout.rcv_rsmkey = KR_VOID;
    msgout.rcv_data = 0;
    msgout.rcv_len = 0;

    CALL(&msgout);
  }

  return 0;
}
