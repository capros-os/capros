/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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
#include <idl/capros/Sleep.h>
#include <domain/domdbg.h>
#include <idl/capros/SysTrace.h>

#define KR_VOID 0
#define KR_ECHO 8
#define KR_SLEEP 9
#define KR_OSTREAM 10
#define KR_SYSTRACE 11

#define ITERATIONS 1000000

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x20000;

int
main()
{
  int i;
  Message msg;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_len = 0;		/* no data returned */

  msg.snd_invKey = KR_ECHO;
  msg.snd_code = 1;

  capros_Sleep_sleep(KR_SLEEP, 4000);

  kprintf(KR_OSTREAM, "Beginning %d small calls to small echo domain\n", ITERATIONS);

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_instrs);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  eros_SysTrace_stopCounter(KR_SYSTRACE);

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_Imiss);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  eros_SysTrace_stopCounter(KR_SYSTRACE);

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_Dmiss);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  eros_SysTrace_stopCounter(KR_SYSTRACE);

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  eros_SysTrace_stopCounter(KR_SYSTRACE);

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_branches);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  eros_SysTrace_stopCounter(KR_SYSTRACE);

  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_brTaken);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  eros_SysTrace_stopCounter(KR_SYSTRACE);

  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
