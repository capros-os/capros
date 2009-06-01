/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Pipe -- limited size buffering object for unidirection streams.

   This version assumes single writer/single reader.  Multi
   reader/writer variants will come later. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/PipeKey.h>
#include <domain/ProtoSpace.h>
#include <stdlib.h>

#include "constituents.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

#define dbg_init	0x01u   /* requests */
#define dbg_req		0x02u   /* requests */
#define dbg_ack		0x04u   /* acknowledgements */
#define dbg_sleep	0x08u   /* sleep/wakeup */
#define dbg_eof		0x10u   /* sleep/wakeup */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))

#ifdef ALIGNED
const uint32_t __rt_stack_pages = 1;
#else
const uint32_t __rt_stack_pages = (PIPE_BUF_SZ/EROS_PAGE_SIZE)+1;
#endif

struct pipe_state {
  char     buf[PIPE_BUF_SZ];
  uint32_t start;
  uint32_t end;
  uint32_t sleepSlot;
  uint32_t sleep;
  uint32_t wrReqLen;
  uint32_t rdReqLen;
  uint32_t wClosed;
  uint32_t nWakeWriter;
  uint32_t nWakeReader;
};
typedef struct pipe_state pipe_state;

/* Size of pipe buffer is chosen to guarantee that only one of the
   following can be true. */
#define SL_NONE    0		/* no one is sleeping */
#define SL_WRITER  1		/* writer is sleeping */
#define SL_READER  2		/* reader is sleeping */

#define KR_CLIENT0     KR_APP(0)	/* Client can be either reader or */
#define KR_CLIENT1     KR_APP(1)	/* writer; it ping pongs. */
#define KR_OSTREAM     KR_APP(2)

#define KI_READER      1
#define KI_WRITER      2

void
teardown(uint32_t caller)
{
  COPY_KEYREG(caller, KR_RETURN);
  
  /* Wake up both clients as needed: */
  
  /* get the protospace */
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_CLIENT0);

  /* destroy as small space. */
  protospace_destroy(KR_VOID, KR_CLIENT0, KR_SELF, KR_CREATOR,
		     KR_BANK, 1);
  /* NOTREACHED */
}

pipe_state *
InitPipe(pipe_state *ps)
{
  ps->start = 0;
  ps->end = 0;
  ps->sleep = SL_NONE;
  ps->sleepSlot = 0;
  ps->wClosed = 0;
  ps->nWakeReader = 0;
  ps->nWakeWriter = 0;
  return ps;
}

uint32_t
return_to_writer(Message *msg, pipe_state *ps)
{
  /* Place reader to sleep and return to the writer. */

  uint32_t sleeper = ps->sleepSlot;
  
  ps->sleep = SL_READER;
  ps->sleepSlot = msg->snd_invKey;
  ps->rdReqLen = msg->rcv_w2;
  ps->nWakeWriter++;

  msg->snd_invKey = sleeper;
  msg->snd_len = 0;
  msg->snd_w1 = ps->wrReqLen;
  return RC_OK;
}

uint32_t
return_to_reader(Message *msg, pipe_state *ps)
{
  /* Place reader to sleep and return to the writer. */

  uint32_t sleeper = ps->sleepSlot;
  uint32_t result = RC_OK;
  uint32_t len = ps->end - ps->start;
  
  ps->sleep = SL_WRITER;
  ps->sleepSlot = msg->snd_invKey;
  ps->wrReqLen = msg->rcv_w2;

  if (len > ps->rdReqLen)
    len = ps->rdReqLen;

  if (len > PIPE_BUF_SZ)
    len = PIPE_BUF_SZ;

  msg->snd_invKey = sleeper;
  msg->snd_data = &ps->buf[ps->start];
  msg->snd_len = len;

  msg->snd_w1 = ps->rdReqLen;	/* ?? */

  ps->start += len;
  ps->nWakeReader++;

  if (ps->wClosed && (ps->start == ps->end))
    result = RC_EOF;

  if (ps->start == ps->end)
    ps->start = ps->end = 0;

  return result;
}

void
wake_writer(pipe_state *ps)
{
  Message wakeMsg;

  DEBUG(sleep)
    kprintf(KR_OSTREAM, "pipe wakes writer saying %d\n", ps->wrReqLen);

  /* Tell writer that we accepted no data so they will resend */
  wakeMsg.snd_invKey = ps->sleepSlot; /* if sleeping, always this KR */
  wakeMsg.snd_key0 = KR_VOID;
  wakeMsg.snd_key1 = KR_VOID;
  wakeMsg.snd_key2 = KR_VOID;
  wakeMsg.snd_rsmkey = KR_VOID;
  wakeMsg.snd_data = 0;
  wakeMsg.snd_len = 0;
  wakeMsg.snd_code = RC_OK;
  wakeMsg.snd_w1 = ps->wrReqLen;
  wakeMsg.snd_w2 = 0;
  wakeMsg.snd_w3 = 0;

  ps->nWakeWriter++;
  
  SEND(&wakeMsg);

  ps->sleep = SL_NONE;
  ps->sleepSlot = KR_VOID;
}
    
void
wake_reader(pipe_state *ps)
{
  Message wakeMsg;
  uint32_t len = ps->end - ps->start;

  if (len > ps->rdReqLen)
    len = ps->rdReqLen;

  if (len > PIPE_BUF_SZ)
    len = PIPE_BUF_SZ;

  wakeMsg.snd_invKey = ps->sleepSlot; /* if sleeping, always this KR */
  wakeMsg.snd_key0 = KR_VOID;
  wakeMsg.snd_key1 = KR_VOID;
  wakeMsg.snd_key2 = KR_VOID;
  wakeMsg.snd_rsmkey = KR_VOID;
  wakeMsg.snd_data = &ps->buf[ps->start];
  wakeMsg.snd_len = len;
  wakeMsg.snd_code = RC_OK;
  wakeMsg.snd_w1 = 0;
  wakeMsg.snd_w2 = 0;
  wakeMsg.snd_w3 = 0;

  ps->start += len;

  if (ps->wClosed && (ps->start == ps->end)) {
    wakeMsg.snd_code = RC_EOF;
    DEBUG(eof)
      kprintf(KR_OSTREAM, "Send EOF to reader when waking\n");
  }
  
  DEBUG(sleep)
    kprintf(KR_OSTREAM, "pipe wakes reader sending %d\n", len);

  ps->nWakeReader++;

  SEND(&wakeMsg);

  ps->sleep = SL_NONE;
  ps->sleepSlot = KR_VOID;

  if (ps->start == ps->end)
    ps->start = ps->end = 0;
}
    
int
ProcessRequest(Message *msg, pipe_state *ps)
{
  uint32_t result = RC_OK;
  uint32_t code = msg->rcv_code;
  fixreg_t got = min(msg->rcv_limit, msg->rcv_sent);
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  switch(code) {
  case OC_Pipe_Read:
    if (msg->rcv_keyInfo != KI_READER) {
      result = RC_capros_key_UnknownRequest;
      break;
    }

    DEBUG(req)
      kprintf(KR_OSTREAM, "pipe accepts read of length %d\n", msg->rcv_w2);

    if (ps->start != ps->end) {
      uint32_t xmit;
      
      if (ps->sleep == SL_WRITER && ps->start == 0)
	wake_writer(ps);
    
      xmit = ps->end - ps->start;
      if (xmit > msg->rcv_w2)
	xmit = msg->rcv_w2;
      if (xmit > PIPE_BUF_SZ)
	xmit = PIPE_BUF_SZ;
      
      msg->snd_len = xmit;
      msg->snd_data = ps->buf + ps->start;
      ps->start += xmit;

      if (ps->wClosed && (ps->start == ps->end)) {
	result = RC_EOF;
	DEBUG(eof)
	  kprintf(KR_OSTREAM, "Send EOF to reader\n");
      }
    }
    else if (ps->wClosed) {
      if (ps->sleep == SL_WRITER && ps->start == 0)
	wake_writer(ps);
    
      result = RC_EOF;
      DEBUG(eof)
	kprintf(KR_OSTREAM, "Send EOF to reader -- buffer empty\n");
      msg->snd_len = 0;
    }
    else {
      /* buffer is empty -- go to sleep. */
#if 0
      if (ps->sleep == SL_WRITER && ps->start == 0)
	wake_writer(ps);
    
      DEBUG(sleep)
	kprintf(KR_OSTREAM, "pipe blocks reader\n");

      ps->rdReqLen = msg->rcv_w2;

      ps->sleep = SL_READER;
      ps->sleepSlot = msg->snd_invKey;
      
      msg->snd_invKey = KR_VOID;
      msg->snd_len = 0;
#else
      if (ps->sleep == SL_WRITER && ps->start == 0)
	result = return_to_writer(msg, ps);
      else {
	DEBUG(sleep)
	  kprintf(KR_OSTREAM, "pipe blocks reader\n");

	ps->rdReqLen = msg->rcv_w2;

	ps->sleep = SL_READER;
	ps->sleepSlot = msg->snd_invKey;
      
	msg->snd_invKey = KR_VOID;
	msg->snd_len = 0;
      }
#endif
    }
    
    if (ps->start == ps->end)
      ps->start = ps->end = 0;
    break;
    
  case OC_Pipe_Write:
    DEBUG(req)
      kprintf(KR_OSTREAM, "pipe accepts write of length %d resid %d\n",
	      got, msg->rcv_w2);

    if (msg->rcv_keyInfo != KI_WRITER) {
      result = RC_capros_key_UnknownRequest;
      break;
    }

    ps->end += got;

    if (ps->end == PIPE_BUF_SZ) {
#if 1
      /* If reader is sleeping, wake them up: */
      if (ps->sleep == SL_READER &&
	  (ps->wClosed ||
	   ps->end == PIPE_BUF_SZ ||
	   (ps->end - ps->start) >= ps->rdReqLen)) {
	result = return_to_reader(msg, ps);
      }
      else {
	DEBUG(sleep)
	  kprintf(KR_OSTREAM, "pipe blocks writer\n");

	/* This I/O cause a full pipe buffer.  Make note that we have it
	   and put the writer to sleep. */
	ps->sleep = SL_WRITER;
	ps->sleepSlot = msg->snd_invKey;

	msg->snd_invKey = KR_VOID;
	msg->snd_len = 0;
	ps->wrReqLen = got;
      }
#else
      /* If reader is sleeping, wake them up: */
      if (ps->sleep == SL_READER &&
	  (ps->wClosed ||
	   ps->end == PIPE_BUF_SZ ||
	   (ps->end - ps->start) >= ps->rdReqLen)) {
	wake_reader(ps);
      }
    
      DEBUG(sleep)
	kprintf(KR_OSTREAM, "pipe blocks writer\n");

      /* This I/O cause a full pipe buffer.  Make note that we have it
	 and put the writer to sleep. */
      ps->sleep = SL_WRITER;
      ps->sleepSlot = msg->snd_invKey;

      msg->snd_invKey = KR_VOID;
      msg->snd_len = 0;
      ps->wrReqLen = got;
#endif
    }
    else {
      /* If reader is sleeping, wake them up: */
      if (ps->sleep == SL_READER &&
	  (ps->wClosed ||
	   ps->end == PIPE_BUF_SZ ||
	   (ps->end - ps->start) >= ps->rdReqLen)) {
	wake_reader(ps);
      }

      DEBUG(ack)
	kprintf(KR_OSTREAM, "pipe acks %d to  writer\n", got);
      msg->snd_w1 = got;
    }
    
    break;

  case OC_Pipe_Close:
    if (msg->rcv_keyInfo == KI_WRITER) {
      /*       DEBUG(req) */
	kprintf(KR_OSTREAM, "pipe: writer closes\n");
      ps->wClosed = 1;
      msg->snd_len = 0;

      /* If reader is sleeping, wake them up -- EVEN if there is
	 no more to read. */
      if (ps->sleep == SL_READER) {
	wake_reader(ps);
	ps->sleep = SL_NONE;
	ps->sleepSlot = KR_VOID;
      }

      /* MUST break here to prevent fall-through */
      break;
    }

    if (ps->sleep == SL_WRITER) {
      wake_writer(ps);
      ps->sleep = SL_NONE;
      ps->sleepSlot = KR_VOID;
    }

    DEBUG(req)
      kprintf(KR_OSTREAM, "pipe: reader closes\n");
    /* fall through is deliberate -- reader close means destroy! */
    
  case OC_capros_key_destroy:
    if (msg->rcv_keyInfo != KI_READER) {
      result = RC_capros_key_UnknownRequest;
      break;
    }

    kprintf(KR_OSTREAM, "nWakeWriter: %u nWakeReader %u\n",
	    ps->nWakeWriter, ps->nWakeReader);

    teardown(msg->snd_invKey);
    return 0; /* CAN'T HAPPEN */
    
  default:
    result = RC_capros_key_UnknownRequest;
    break;
  }

  msg->snd_code = result;
  return 1;
}

pipe_state ps __attribute__ ((aligned (EROS_PAGE_SIZE)));

int
main(void)
{
  uint32_t result;
#ifndef ALIGNED
  pipe_state ps;
  pipe_state *pps = &ps;
#endif
  Message msg;
  
  msg.snd_invKey = KR_VOID;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;
  msg.rcv_data = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  /* Initialization is not permitted to fail -- this would constitute
     an unrunnable system! */
  pps = InitPipe(&ps);
  
  /* Fabricate the reader and writer keys: */
  result = capros_Process_makeStartKey(KR_SELF, KI_WRITER, KR_CLIENT0);
  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Result from pipe cre strt key: 0x%x\n",
	     result);
  result = capros_Process_makeStartKey(KR_SELF, KI_READER, KR_CLIENT1);
  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Result from pipe cre strt key: 0x%x\n",
	     result);
  
  msg.snd_key0 = KR_CLIENT0;
  msg.snd_key1 = KR_CLIENT1;
  msg.snd_invKey = KR_RETURN;
  msg.snd_len = 0;

  msg.rcv_data = pps->buf;
  msg.rcv_limit = PIPE_BUF_SZ;
  msg.rcv_rsmkey = KR_CLIENT0;

  DEBUG(init)
    kprintf(KR_OSTREAM, "init pipe: accept rsm key to %d\n",
	    msg.rcv_rsmkey);
  
  RETURN(&msg);

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  
  for(;;) {
    /* Until somebody decides otherwise, we plan to return to the
       client who calls us: */
    msg.snd_invKey = msg.rcv_rsmkey;

    (void) ProcessRequest(&msg, &ps);

    /* If, however, we have a sleeping client, we set things up to
       accept the other side elsewhere: */
    msg.rcv_rsmkey = (pps->sleepSlot == KR_CLIENT0) ? KR_CLIENT1 : KR_CLIENT0;
    msg.rcv_data = &pps->buf[pps->end];
    msg.rcv_limit = PIPE_BUF_SZ - pps->end;

    RETURN(&msg);
  }
}
