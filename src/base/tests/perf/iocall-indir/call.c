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


#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/SleepKey.h>
#include <domain/domdbg.h>
#include <eros/SysTraceKey.h>

#define KR_VOID 0
#define KR_ECHO 8
#define KR_SLEEP 9
#define KR_OSTREAM 10
#define KR_SYSTRACE 11

#define ITERATIONS 400000

uint8_t msgbuf[EROS_MESSAGE_LIMIT + EROS_PAGE_SIZE];

/* It is intended that this should be a large space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x80001000;

void
dotrace(uint32_t nChar, int shouldAlign)
{
  Message msg;
  int i;

  uint32_t msgPtr = (uint32_t) msgbuf;
  if (shouldAlign) {
    msgPtr += (EROS_PAGE_SIZE - 1);
    msgPtr &= ~EROS_PAGE_MASK;
  }
  
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = nChar;
  msg.snd_data = (uint8_t *) msgPtr;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_len = 0;		/* no data returned */

  msg.snd_invKey = KR_ECHO;
  msg.snd_code = 1;

  kprintf(KR_OSTREAM, "%d iterations of %d bytes (indirect)\n", ITERATIONS, nChar);
  systrace_clear_kstats(KR_SYSTRACE);
  systrace_start(KR_SYSTRACE, SysTrace_Mode_Cycles);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  systrace_stop(KR_SYSTRACE);
}

void main()
{
  int i;

  sl_sleep(KR_SLEEP, 4000);

  kprintf(KR_OSTREAM, "SENDER ALIGNED\n");

  i = 16;
  while (i <= EROS_MESSAGE_LIMIT) {
    dotrace(i, 1);
    i *= 2;
  }

  kprintf(KR_OSTREAM, "Done\n");
}
