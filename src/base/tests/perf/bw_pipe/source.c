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
#include <idl/eros/Sleep.h>
#include <idl/eros/SysTrace.h>
#include <domain/domdbg.h>
#include <domain/PipeKey.h>
#include <memory.h>

#define KR_VOID     0
#define KR_SLEEP    9
#define KR_OSTREAM  10
#define KR_SYSTRACE 11
#define KR_PIPECRE  12
#define KR_BANK     13
#define KR_SCHED    14
#define KR_SINK     15
#define KR_DWPIPE   16
#define KR_DRPIPE   17

/* The control pipe is created solely to allow more direct comparison
   to lmbench figures -- just making sure we are doing the same
   actions */
#define KR_CWPIPE   18
#define KR_CRPIPE   19

#define ITERATIONS 1000000

/* It is intended that this should be a large space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x41000;

#define BUF_SZ EROS_MESSAGE_LIMIT

char buf[BUF_SZ] __attribute__ ((aligned (EROS_PAGE_SIZE)));
#ifndef IO_SZ
#define IO_SZ PIPE_BUF_SZ
#endif

int
main()
{
  int i;
  uint32_t len;
  uint32_t result;
  uint32_t tot_mov = 0;
  uint64_t startcy = 0, endcy = 0;
  
  Message msg;

  result = pipe_create(KR_PIPECRE, KR_BANK, KR_SCHED,
		       KR_DWPIPE, KR_DRPIPE);

  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Data pipe creation failed\n");

  result = pipe_create(KR_PIPECRE, KR_BANK, KR_SCHED,
		       KR_CWPIPE, KR_CRPIPE);

  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Ctrl pipe creation failed\n");


  kprintf(KR_OSTREAM, "Pipe has been constructed\n");

  pipe_write(KR_DWPIPE, IO_SZ/2, buf, &len);
  if (len != IO_SZ/2)
    kprintf(KR_OSTREAM, "Initialization write did not accept full buffer %d vs %d \n", len, IO_SZ/2);
  
  pipe_read(KR_DRPIPE, IO_SZ/2, buf, &len);
  if (len != IO_SZ/2)
    kprintf(KR_OSTREAM, "Initialization read did not accept full buffer\n");
  
#if 0
  eros_Sleep_sleep(KR_SLEEP, 4000);
#endif

  bzero(&msg, sizeof(msg));
  msg.snd_invKey = KR_SINK;
  msg.snd_key0 = KR_DRPIPE;
  msg.snd_key1 = KR_CWPIPE;

  kprintf(KR_OSTREAM, "Sending RPIPE to SINK domain...\n");
  SEND(&msg);

  eros_Sleep_sleep(KR_SLEEP, 2000);
  
  for (i = 0; i < BUF_SZ; i++)
    buf[i] = i % 16;
  
    
  eros_SysTrace_getCycle(KR_SYSTRACE, &startcy);

#if 0
  eros_SysTrace_clearKernelStats(KR_SYSTRACE);
  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);
#endif

  pipe_read(KR_CRPIPE, sizeof(tot_mov), (uint8_t*) &tot_mov, &len);
  
  {
    uint32_t cur_mov = tot_mov;
    while (cur_mov) {
      pipe_write(KR_DWPIPE, IO_SZ, buf, &len);
      cur_mov -= len;
    }
  }
  
#if 0
  eros_SysTrace_stopCounter(KR_SYSTRACE);
#endif
  eros_SysTrace_getCycle(KR_SYSTRACE, &endcy);

  {
    uint64_t delta = endcy - startcy;

    kprintf(KR_OSTREAM, "%d bytes, %U cycles\n", tot_mov, delta); 
  }

  pipe_close(KR_DWPIPE);
  
  kprintf(KR_OSTREAM, "bw_pipe: Writer Done -- %d bytes\n", tot_mov);

  return 0;
}
