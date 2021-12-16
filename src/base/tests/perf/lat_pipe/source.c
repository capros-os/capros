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
#include <idl/capros/SysTrace.h>
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
#define KR_WPIPE    16
#define KR_RPIPE    17

#define ITERATIONS 1000000

/* It is intended that this should be a large space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x41000;

#define BUF_SZ 1

char buf[BUF_SZ];

int
main()
{
  int i;
  uint32_t len;
  uint32_t result;

  result = pipe_create(KR_PIPECRE, KR_BANK, KR_SCHED,
		       KR_WPIPE, KR_RPIPE);

  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Data pipe creation failed\n");

  kprintf(KR_OSTREAM, "Pipe has been constructed\n");

  /* WARM THINGS UP: */
  pipe_write(KR_WPIPE, BUF_SZ, buf, &len);
  if (len != BUF_SZ)
    kprintf(KR_OSTREAM, "Initialization write did not accept full buffer\n");
  
  pipe_read(KR_RPIPE, BUF_SZ, buf, &len);
  if (len != BUF_SZ)
    kprintf(KR_OSTREAM, "Initialization read did not accept full buffer\n");
  
  capros_Sleep_sleep(KR_SLEEP, 2000);
  
  for (i = 0; i < BUF_SZ; i++)
    buf[i] = i % 16;
    
  eros_SysTrace_clearKernelStats(KR_SYSTRACE);
  eros_SysTrace_startCounter(KR_SYSTRACE, eros_SysTrace_mode_cycles);

  for (i = 0; i < ITERATIONS; i++) {
    pipe_write(KR_WPIPE, BUF_SZ, buf, &len);
    pipe_read(KR_RPIPE, BUF_SZ, buf, &len);
  }
  
  eros_SysTrace_stopCounter(KR_SYSTRACE);

  
  kprintf(KR_OSTREAM, "lat_pipe -- %d iterations\n", ITERATIONS);

  pipe_close(KR_WPIPE);
  pipe_close(KR_RPIPE);

  return 0;
}

