/*
 * Copyright (C) 2007, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/*
 * Proc2 -- this process calls proc1 and stores the result in shared memory.
 */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

#define KR_OSTREAM 10
#define KR_PROC1_START 12

#define ADDR1 0x40000

#define BUF_SIZE EROS_PAGE_SIZE
static uint8_t rcvData[BUF_SIZE];

int
main()
{
  int i;
  Message msg;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code = 3;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_len = 0;
  //msg.snd_data = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = rcvData;
  msg.rcv_limit = BUF_SIZE;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  msg.snd_invKey = KR_PROC1_START;
  for (;;) {

    // Clear the receive area.
    uint32_t * p = (uint32_t *) &rcvData;
    for (i=0; i < BUF_SIZE/4; i++)
      p[i] = 0xbadbad00;

    CALL(&msg);

    // Order code is string limit for next call.
    msg.rcv_limit = msg.rcv_code;

    // Copy the message and string to shared memory where proc1 can examine it.
    memcpy((void *)ADDR1, &msg, sizeof(msg));
    memcpy((char *)ADDR1 + sizeof(msg),
           rcvData, BUF_SIZE - sizeof(msg));
  }

  return 0;
}
