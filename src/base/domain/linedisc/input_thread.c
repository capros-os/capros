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
#include <eros/i486/cap-instr.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <idl/eros/Stream.h>
#include <idl/eros/domain/linedisc.h>

#include "keydefs.h"
#include "buffer.h"

extern eros_domain_linedisc_termios tstate;
static LDBuffer linebuf;

static bool
ProcessInputRequest(Message *m)
{
  COPY_KEYREG(KR_RETURN, KR_STASH_IN);

  switch(m->rcv_code) {
  case OC_eros_Stream_read:
    {
    }
    break;

  case OC_eros_Stream_nread:
    {
      bool eol = false;
      uint32_t u;

      /* If line is available, return it to client.  Else, park client. */

      /* Then, keep requesting input from the "downstream stream"
         until end-of-line is entered.  At that point, wake client and
         return buffer contents. */

/* FIX: How to return ctrl chars to client?  E.G. Is CTRL-C and CTRL-V reported separately or part of the current line?  Do they act as EOL as far as returning input to client?  */

      while (!eol) {
	uint8_t c = 0;

	/* Read a character */
	eros_Stream_read(KR_RAW_STREAM, &c);

	/* Test for EOL */
	eol = (c == tstate.c_cc[eros_domain_linedisc_VEOL] ||
	       c == tstate.c_cc[eros_domain_linedisc_VEOL2]); 

	/* If not EOL write character to line buffer */
	if (!eol)
	  buffer_write(KR_RAW_STREAM, tstate, &linebuf, c);
      }

      {
	unsigned sndIndir = 16;
	eros_Stream_iobuf *s = (eros_Stream_iobuf *) (m->snd_data + 0);

	/* FIX: I think this is wrong! (vandy) */
	s->data = alloca((linebuf.len+1) * sizeof(*s->data));

	/* Set up the sndbuf and return it to  client */
	for (u = 0; u < linebuf.len; u++)
	  s->data[u] = linebuf.chars[u];

	/* NULL terminated */
	s->data[linebuf.len] = 0;
	s->len = linebuf.len+1;

	__builtin_memcpy((void *)(m->snd_data) + sndIndir, s->data, s->len *
			 sizeof(*s->data));

	m->snd_len = 16 + (s->len * sizeof(*s->data));
	m->snd_code = RC_OK;    
      }

      /* Don't forget to clear the line buffer! */
      buffer_clear(&linebuf);
    }
    break;

  default:
    {
      m->snd_code = RC_eros_key_UnknownRequest;
    }
  }

  COPY_KEYREG(KR_STASH_IN, KR_RETURN);
  return true;
}

int
input_main(void)
{
  Message msg;
  size_t sndSz = 1024;
  size_t rcvSz = 1024;
  void *sndBuf = alloca(sndSz);
  void *rcvBuf = alloca(rcvSz);

  kprintf(KR_OSTREAM, "Input thread of ld0 says hi...\n");

  /* Init the line buffer */
  buffer_clear(&linebuf);

  memset(&msg, 0, sizeof(Message));
  msg.rcv_limit = rcvSz;
  msg.rcv_data = rcvBuf;
  msg.snd_data = sndBuf;
  msg.rcv_rsmkey = KR_RETURN;
  msg.snd_invKey = KR_RETURN;

  do {
    RETURN(&msg);
  } while (ProcessInputRequest(&msg));

  return 0;
}


