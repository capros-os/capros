/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <idl/eros/Sleep.h>
#include <domain/domdbg.h>
#include <idl/eros/arch/arm/SysTrace.h>

#define KR_VOID 0
#define KR_ECHO 8
#define KR_SLEEP 9
#define KR_OSTREAM 10
#define KR_SYSTRACE 11
#define KR_ECHO_PROCESS 12

#define ITERATIONS 1000000
#define CheckInterations 100

/* It is intended that this should be a small space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

int
main()
{
  uint32_t result;
  uint64_t startTime, endTime;
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
  msg.rcv_limit = 0;		/* no data returned */

  /* Get echo process started. */
  process_make_fault_key(KR_ECHO_PROCESS, KR_ECHO_PROCESS);
  msg.snd_invKey = KR_ECHO_PROCESS;
  msg.snd_code = RC_OK;
  SEND(&msg);

  kprintf(KR_OSTREAM, "Sleep a while\n");
  eros_Sleep_sleep(KR_SLEEP, 1000);	// sleep 1000 ms

  msg.snd_invKey = KR_ECHO;
  msg.snd_code = 1;

  kprintf(KR_OSTREAM, "Beginning %d calls to echo domain\n", ITERATIONS);

  result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(ITERATIONS)) );

  kprintf(KR_OSTREAM, "Beginning %d calls to check\n", CheckInterations);

  result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < CheckInterations; i++)
    eros_arch_arm_SysTrace_CheckConsistency(KR_SYSTRACE);

  result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(CheckInterations)) );

#define INCR_ITERATIONS 150000000
  kprintf(KR_OSTREAM, "Beginning %d increments\n", INCR_ITERATIONS);

  result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = INCR_ITERATIONS; i > 0; i--) ;

  result = eros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(INCR_ITERATIONS)) );

  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
