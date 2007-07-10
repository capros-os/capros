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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <eros/ProcessKey.h>
#include <eros/StdKeyType.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Discrim.h>
#include <idl/capros/Forwarder.h>
#include <domain/domdbg.h>
#include <idl/capros/arch/arm/SysTrace.h>

#define KR_DISCRIM 8
#define KR_SLEEP 9
#define KR_OSTREAM 10
#define KR_SYSTRACE 11
#define KR_ECHO_PROCESS 12
#define KR_FORWARDER 13
#define KR_OP_FORWARDER 14	// opaque
#define KR_ECHO_START0 15
#define KR_ECHO_START1 16
#define KR_SCRATCH 17
#define KR_SCRATCH2 18

#define ITERATIONS 100000

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

Message msg;

int
main()
{
  uint32_t result;
  uint64_t startTime, endTime;
  int i;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_limit = 0;		/* no data returned */

  /* Get echo process started. */
  process_make_fault_key(KR_ECHO_PROCESS, KR_SCRATCH);
  msg.snd_invKey = KR_SCRATCH;
  msg.snd_code = RC_OK;
  SEND(&msg);

  kprintf(KR_OSTREAM, "Sleep a while\n");

  capros_Sleep_sleep(KR_SLEEP, 1000);	// sleep 1000 ms

  kprintf(KR_OSTREAM, "Beginning %d calls to forwarder\n", ITERATIONS);

  msg.snd_invKey = KR_OP_FORWARDER;
  msg.snd_code = 7;

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(ITERATIONS)) );

  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
