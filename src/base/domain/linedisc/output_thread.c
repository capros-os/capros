/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/* The routines for linedisc's input thread */
#include <stddef.h>
#include <string.h>
#include <alloca.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/cap-instr.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <idl/capros/Stream.h>
#include <idl/capros/linedisc.h>

#include "keydefs.h"

/* extern globals (defined in linedisc.c) */
extern eros_domain_linedisc_termios tstate;
extern bool flow_control;
extern uint32_t receive_buffer[];

static bool
ProcessOutputRequest(Message *m)
{
  COPY_KEYREG(KR_RETURN, KR_STASH_OUT);

  switch(m->rcv_code) {
  case OC_capros_Stream_write:
    {
      /* If flow-controlled then need to buffer these char's.  Else
	 send them directly downstream */
      if (!flow_control) 
        capros_Stream_write(KR_RAW_STREAM, m->rcv_w1);

      m->snd_code = RC_OK;
    }
    break;

  case OC_capros_Stream_nwrite:
    {
      /* If flow-controlled then need to buffer these char's.  Else
	 send them directly downstream */
      capros_Stream_iobuf *s = (capros_Stream_iobuf *)(m->rcv_data + 0);

      s->data = (char *)(m->rcv_data + 16);
  
      if (!flow_control) 
        capros_Stream_nwrite(KR_RAW_STREAM, *s);

      m->snd_code = RC_OK;
    }
    break;

  default:
    {
      m->snd_code = RC_capros_key_UnknownRequest;
    }
  }

  COPY_KEYREG(KR_STASH_OUT, KR_RETURN);
  return true;
}

static void
doClearRcvBuf(void *buf, size_t sz)
{
  uint32_t u;
  uint8_t *z = (uint8_t *)buf;

  for (u = 0; u < sz; u++)
    z[u] = 0;
}

int
output_main(void)
{
  Message msg;
  size_t sndSz = 1024;
  size_t rcvSz = 1024;
  void *sndBuf = alloca(sndSz);
  void *rcvBuf = alloca(rcvSz);

  kprintf(KR_OSTREAM, "Output thread of ld0 says hi...\n");

  memset(&msg, 0, sizeof(Message));
  msg.rcv_data = rcvBuf;
  msg.rcv_limit = rcvSz;
  msg.snd_data = sndBuf;
  msg.rcv_rsmkey = KR_RETURN;
  msg.snd_invKey = KR_RETURN;

  do {
    doClearRcvBuf(rcvBuf, rcvSz);
    RETURN(&msg);
  } while (ProcessOutputRequest(&msg));

  return 0;
}


