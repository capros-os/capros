/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group
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
#include <eros/machine/atomic.h>
#include <eros/machine/cap-instr.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <domain/domdbg.h>
#include <idl/capros/SysTrace.h>

#define KR_VOID 0
#define KR_ECHO 8
#define KR_SLEEP 9
#define KR_OSTREAM 10
#define KR_SYSTRACE 11
#define KR_ECHO_PROCESS 12

/* It is intended that this should be a small space process. */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x21000;
const uint32_t __rt_unkept = 1;	/* do not mess with keeper */

#define EXERCISE_CACHE
#ifdef EXERCISE_CACHE
#define cacheStep 0x100	// step to the next cache segment
#define cacheAssoc 64
char cacheBuf[(cacheAssoc+1)*cacheStep];
#endif

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
  capros_Process_makeResumeKey(KR_ECHO_PROCESS, KR_ECHO_PROCESS);
  msg.snd_invKey = KR_ECHO_PROCESS;
  msg.snd_code = RC_OK;
  SEND(&msg);

  kprintf(KR_OSTREAM, "Sleep a while\n");
  capros_Sleep_sleep(KR_SLEEP, 1000);	// sleep 1000 ms

#define ITERATIONS 100000
  kprintf(KR_OSTREAM, "Beginning %d calls to echo process and return\n", ITERATIONS);

  msg.snd_invKey = KR_ECHO;
  msg.snd_code = 1;

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < ITERATIONS; i++)
    (void) CALL(&msg);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(ITERATIONS)) );


  kprintf(KR_OSTREAM, "Beginning %d calls to misc key: ", ITERATIONS);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  uint32_t type;
  for (i = 0; i < ITERATIONS; i++)
    capros_key_getType(KR_OSTREAM, &type);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(ITERATIONS)) );


#define PSITER 1000000

  kprintf(KR_OSTREAM, "Beginning %d calls to atomic add: ", PSITER);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  uint32_t atomicVar;
  for (i = 0; i < PSITER; i++)
    capros_atomic_add32_return(1, &atomicVar);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(PSITER)) );


  kprintf(KR_OSTREAM, "Beginning %d calls to null COPY_KEYREG: ", PSITER);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < PSITER; i++)
    COPY_KEYREG(KR_VOID, KR_VOID);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(PSITER)) );


  kprintf(KR_OSTREAM, "Beginning %d calls to normal COPY_KEYREG: ", PSITER);
  COPY_KEYREG(KR_ECHO_PROCESS, KR_ECHO);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < PSITER; i++)
    COPY_KEYREG(KR_ECHO_PROCESS, KR_ECHO);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(PSITER)) );


  kprintf(KR_OSTREAM, "Beginning %d calls to XCHG_KEYREG: ", PSITER);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < PSITER; i++)
    XCHG_KEYREG(KR_ECHO, KR_ECHO_PROCESS);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(PSITER)) );

#ifdef EXERCISE_CACHE
#define CITER 100000
  kprintf(KR_OSTREAM, "Beginning %d exercise cache read: ", CITER);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < CITER; i++) {
// This is designed to exercise the ARM920T cache.
    uint32_t j;
    volatile uint32_t * p;
    for (j = 0; j < cacheAssoc+1; j++) {
      p = (volatile uint32_t *) (&cacheBuf[j*cacheStep]);
      *p;
    }
  }

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(CITER)) );


  kprintf(KR_OSTREAM, "Beginning %d exercise cache write: ", CITER);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < CITER; i++) {
// This is designed to exercise the ARM920T cache.
    uint32_t j;
    volatile uint32_t * p;
    for (j = 0; j < cacheAssoc+1; j++) {
      p = (volatile uint32_t *) (&cacheBuf[j*cacheStep]);
      *p = 0;
    }
  }

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(CITER)) );
#endif


#define CheckInterations 10
  kprintf(KR_OSTREAM, "Beginning %d calls to check\n", CheckInterations);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = 0; i < CheckInterations; i++)
    capros_SysTrace_CheckConsistency(KR_SYSTRACE);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(CheckInterations)) );

#define INCR_ITERATIONS 15000000
  kprintf(KR_OSTREAM, "Beginning %d calls to a null procedure\n", INCR_ITERATIONS);

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);

  for (i = INCR_ITERATIONS; i > 0; i--) {
    extern void DoNothing(void);
    DoNothing();
  }

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);
  kprintf(KR_OSTREAM, "%10u ns per iter\n",
          (uint32_t) ((endTime - startTime)/(INCR_ITERATIONS)) );

  kdprintf(KR_OSTREAM, "Done\n");

  return i;
}
