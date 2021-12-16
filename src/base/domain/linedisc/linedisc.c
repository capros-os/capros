/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution,
 * and is derived from the EROS Operating System distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* A domain that implements line discipline for character terminals */
#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/cap-instr.h>

#include <string.h>

#include <domain/ConstructorKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <idl/capros/Process.h>
#include <idl/capros/Stream.h>
#include <idl/capros/linedisc.h>

#include <ethread/ethread.h>

#include "constituents.h"

//#define TRACE

#include "keydefs.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

/* Stack size for the threads */
#define STACK_SIZE  4000

/* main functions for the two threads */
extern int input_main(void);
extern int output_main(void);

/* globals */
uint32_t receive_buffer[1024] = {0};
bool flow_control = false;
eros_domain_linedisc_termios tstate = {
  0,
  0,
  0,
  0,
  0,
  {0},
  0,
  0,
};

#if 1
static void
return_start_key(cap_t kr_start)
{
  Message msg;

  memset(&msg, 0, sizeof(msg));
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = kr_start;

  SEND(&msg);
}
#endif

static void
redirect(Message *m, cap_t kr_new_target)
{
  m->snd_key0 = kr_new_target;
  m->snd_w1 = RETRY_SET_LIK;
  m->invType = IT_Retry;
}

static void
termios_clear_chars(eros_domain_linedisc_termios *t)
{
  uint32_t u;

  if (t == NULL)
    return;

  for (u = 0; u < eros_domain_linedisc_NCCS; u++)
    t->c_cc[u] = 0;
}

static void
makecooked()
{
  termios_clear_chars(&tstate);

#define M(nm) eros_domain_linedisc_##nm
  tstate.c_iflag = (M(BRKINT)|M(ICRNL)|M(IXON));
  tstate.c_oflag = (M(OPOST)|M(ONLCR));
  tstate.c_lflag = (M(ECHO)|M(ECHONL)|M(ECHOE)|M(ECHOCTL)|
		    M(ECHOKE)|M(ICANON)|M(ISIG));
  tstate.c_cflag = (M(CREAD)|M(B9600));

  tstate.c_cc[M(VEOL)]     = '\n';
  tstate.c_cc[M(VEOL2)]     = '\r';
  tstate.c_cc[M(VERASE)]   = 0x08;
  tstate.c_cc[M(VKILL)]    = 0x15;
  tstate.c_cc[M(VWERASE)]  = 0x17;
  tstate.c_cc[M(VREPRINT)] = 0x12;

#undef M
}

static void
makeraw()
{
  termios_clear_chars(&tstate);

#define M(nm) eros_domain_linedisc_##nm
  tstate.c_iflag &= ~(M(IGNBRK)|M(BRKINT)|M(PARMRK)|M(ISTRIP)|
		      M(INLCR)|M(IGNCR)|M(ICRNL)|M(IXON));
  tstate.c_oflag &= ~M(OPOST);
  tstate.c_lflag &= ~(M(ECHO)|M(ECHONL)|M(ICANON)|M(ISIG));
  tstate.c_cflag &= ~(M(CSIZE)|M(PARENB));
  tstate.c_cflag |= M(CS8);
#undef M
}

static bool
DispatchRequest(Message *m)
{
  switch(m->rcv_code) {

    /* The Line Discipline requests: */
    case OC_eros_domain_linedisc_makeraw:
      {
	makeraw();
	m->snd_code = RC_OK;
      }
      break;

    case OC_eros_domain_linedisc_makecooked:
      {
	makecooked();
	m->snd_code = RC_OK;
      }
      break;

    case OC_eros_domain_linedisc_setattr:
      {
	uint32_t got, expect;
	eros_domain_linedisc_termios *t = NULL;

	expect = sizeof(eros_domain_linedisc_termios);
	got = min(m->rcv_sent, m->rcv_limit);
	if (got != expect) {
	  kprintf(KR_OSTREAM, "**ERROR: linedisc msg truncated: expect="
		  "%u and got=%u\n", expect, got);
	  m->snd_code = RC_capros_key_RequestError;
	  return true;
	}

	t = (eros_domain_linedisc_termios *)receive_buffer;
	tstate = *t;
	m->snd_code = RC_OK;

      }
      break;

    case OC_eros_domain_linedisc_getattr:
      {
	m->snd_data = &tstate;
	m->snd_len = sizeof(tstate);
	m->snd_code = RC_OK;
      }
      break;

      /* Handle the Stream requests */
    case OC_capros_Stream_write:
    case OC_capros_Stream_nwrite:
      {
	redirect(m, KR_OUT_THREAD);
	m->snd_code = RC_OK;
      }
      break;

    case OC_capros_Stream_read:
    case OC_capros_Stream_nread:
      {
	redirect(m, KR_IN_THREAD);
	m->snd_code = RC_OK;
      }
      break;

    default:
      {
	m->snd_code = RC_capros_key_UnknownRequest;
      }
    }
  return true;
}

static void
dispatch_loop(void)
{
  Message msg;

  memset(&msg, 0, sizeof(Message));
  msg.invType = IT_PReturn;
  //  msg.snd_key0 = KR_START;

  do {
    msg.rcv_rsmkey = KR_RETURN;
    msg.snd_invKey = KR_RETURN;
    msg.rcv_data = receive_buffer;
    msg.rcv_limit = sizeof(receive_buffer);
    INVOKECAP(&msg);
    msg.invType = IT_PReturn;
    msg.snd_key0 = KR_VOID;
    msg.snd_w1 = 0;
  } while (DispatchRequest(&msg));
}

int
main(void)
{
  result_t result;

  /* A key to a downstream terminal is passed via this domain's
     constructor.  Grab it here and stash it for later use. */
  COPY_KEYREG(KR_ARG(0), KR_RAW_STREAM);

  /* Retrieve the constituent keys */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  kprintf(KR_OSTREAM, "Linedisc says hi...\n");

  /* Make start key */
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);

  return_start_key(KR_START);

  /* Create two threads: one for input and one for output.  This
     "main" thread will be the dispatcher. */
  result = ethread_new_thread(KR_BANK, STACK_SIZE,
			      (uint32_t)&input_main, KR_IN_THREAD);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: linedisc couldn't create input thread:"
	    " 0x%08x\n", result);
    return -1;
  }

  result = ethread_new_thread(KR_BANK, STACK_SIZE,
			      (uint32_t)&output_main, KR_OUT_THREAD);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: linedisc couldn't create output thread:"
	    " 0x%08x\n", result);
    return -1;
  }

  kprintf(KR_OSTREAM, "Linedisc is available...\n");

  /* Now this domain becomes the "dispatcher" */
  dispatch_loop();

  return 0;
}

