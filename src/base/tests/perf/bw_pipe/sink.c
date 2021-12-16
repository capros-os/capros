/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2009, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <domain/domdbg.h>
#include <domain/PipeKey.h>
#include <idl/capros/key.h>

#define KR_VOID     0
#define KR_SINK     8
#define KR_SLEEP    9
#define KR_OSTREAM  10
#define KR_SYSTRACE 11

#define KR_DRPIPE    16
#define KR_CWPIPE    17

#define ITERATIONS 1000000

/* It is intended that this should be a large space domain */
const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x41000;

#define NBUFS 4096		/* move 256M */
#define BUF_SZ capros_key_messageLimit

char buf[BUF_SZ] __attribute__ ((aligned (EROS_PAGE_SIZE)));
#ifndef IO_SZ
#define IO_SZ PIPE_BUF_SZ
#endif

int
main()
{
  uint32_t len;
  uint32_t result;
  uint32_t tot_mov = 0;
  Message msg;

  msg.snd_invKey = KR_VOID;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.rcv_key0 = KR_DRPIPE;
  msg.rcv_key1 = KR_CWPIPE;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_len = 0;
  msg.snd_len = 0;

  kprintf(KR_OSTREAM, "sink: waits for pipe read key\n");

  /* first things first... wait for the pipe read key. */
  RETURN(&msg);
  
  tot_mov = NBUFS * BUF_SZ;

  kprintf(KR_OSTREAM, "sink: got pipe read key\n");

  pipe_write(KR_CWPIPE, sizeof(tot_mov), (const uint8_t *) &tot_mov, &len);
  
  do {
    result = pipe_read(KR_DRPIPE, IO_SZ, buf, &len);
  } while (result != RC_EOF);

  kprintf(KR_OSTREAM, "Reader Done -- %d bytes\n", tot_mov);

  kprintf(KR_OSTREAM, "Call to destroy...\n");

  key_destroy(KR_DRPIPE);
  
  kprintf(KR_OSTREAM, "Reader Done -- %d bytes\n", tot_mov);

  return 0;
}
