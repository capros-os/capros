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

/* A simple shell domain that asks for a line of input, parses it, and
   executes a small set of commands. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <eros/cap-instr.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/capros/Stream.h>
#include <idl/capros/eterm.h>
#include <idl/capros/linedisc.h>

#include "constituents.h"

#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/
#define KR_ETERM         KR_APP(1)
#define KR_STRM          KR_APP(2)
#define KR_LINEDISC      KR_APP(3)
#define KR_NODE          KR_APP(4)
#define KR_TMP           KR_APP(5)

static void
return_to_vger()
{
  Message m;

  memset(&m, 0, sizeof(Message));
  m.rcv_rsmkey = KR_RETURN;
  m.snd_invKey = KR_RETURN;

  SEND(&m);
}

static void
stream_writes(cap_t strm, const char *s)
{
  capros_Stream_iobuf buf;

  buf.len = strlen(s);
  buf.max = strlen(s);
  buf.data = alloca(strlen(s));

  __builtin_memcpy((void *)(buf.data), s, buf.len);

  capros_Stream_nwrite(strm, buf);
}

static void
echo_prompt(cap_t strm)
{
  stream_writes(strm, "eros> \0");
}

result_t
my_capros_Stream_nread(cap_t _self, capros_Stream_iobuf *s)
{
  Message msg;

  unsigned char *rcvData;
  unsigned rcvLen = 0;
  unsigned rcvIndir = 0;

  /* receive string size computation */
  rcvLen += 12;	/* sizeof(s), align=4 */
  rcvLen += (8-1); rcvLen -= (rcvLen % 8);	/* align to 8 */
  rcvIndir = rcvLen;
  rcvLen += 1 * 1024; /* s vector MAX content, align=1 */
  rcvData = alloca(rcvLen);

  msg.snd_invKey = KR_LINEDISC;
  msg.snd_code = OC_capros_Stream_nread;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_len = 0;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;

  msg.rcv_limit = 0;
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;

  msg.rcv_data = rcvData;
  msg.rcv_limit = rcvLen;

  CALL(&msg);

  if (msg.rcv_code != RC_OK) return msg.rcv_code;
  rcvLen = 0;

  /* Deserialize s */
  {
    capros_Stream_iobuf *_CAPIDL_arg;

    _CAPIDL_arg = (capros_Stream_iobuf *) (rcvData + rcvLen);
    *s = *_CAPIDL_arg;
    rcvLen += sizeof(*s);
    _CAPIDL_arg->data = (char *) (rcvData + rcvIndir);

    __builtin_memcpy(s->data, _CAPIDL_arg->data, (_CAPIDL_arg->len < 4096 ? _CAPIDL_arg->len : 4096));

    rcvIndir += sizeof(s->data) * (_CAPIDL_arg->len < 4096 ? _CAPIDL_arg->len : 4096);
  }
  return msg.rcv_code;
}

int
main(void)
{
  /* Get needed keys from our constituents node */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_LINEDISC, KR_LINEDISC);

  /* Get ETerm key */
  COPY_KEYREG(KR_ARG(0), KR_ETERM);

  /* Answer our creator, just so it won't block forever. */
  return_to_vger();

  kprintf(KR_OSTREAM, "Test domain says hi ...\n");

  eros_domain_eterm_stream_open(KR_ETERM, KR_STRM);
  eros_domain_eterm_set_cursor(KR_ETERM, 219);

  /* Now that we have a stream, construct line discipline module */
  kprintf(KR_OSTREAM, "About to construct line discipline domain...");
  if (constructor_request(KR_LINEDISC, KR_BANK, KR_SCHED, KR_STRM, 
			  KR_LINEDISC) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct line discipline...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  eros_domain_linedisc_makecooked(KR_LINEDISC);

  stream_writes(KR_LINEDISC, "Welcome to EROS v.0.0.1pre-alpha!\n\r");

  for (;;) {
    uint8_t line[1024] = {' '};
    capros_Stream_iobuf s = {
      1024,
      1024,
      line,
    };

    echo_prompt(KR_LINEDISC);
    my_capros_Stream_nread(KR_LINEDISC, &s);

    kprintf(KR_OSTREAM, "Test: returned from nread: len=%u s=[%s]\n", s.len,
	    s.data);
    stream_writes(KR_LINEDISC, "\n\r");
  }

  //  eros_domain_eterm_sepuku(KR_ETERM);
  kprintf(KR_OSTREAM, "Test is done.\n");

  return 0;
}

