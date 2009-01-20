/*
 * Copyright (C) 2009, Strawberry Development Group.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpaceDS.h>

#include <idl/capros/Process.h>
#include <idl/capros/NetListener.h>
#include <idl/capros/TCPListenSocket.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/Node.h>
#include <idl/capros/Discrim.h>
#include <ethread/ethread.h>

#include <domain/domdbg.h>

/* Constituent node contents */
#include "constituents.h"

/* Constants for setting up the listen process */
#define STACK_SIZE 4096

/* Key registers */
#define KR_TCP_PORTNO KR_APP(1) /* The TCP local port key - for listen */
#define KR_CONNECTION_HANDLER_C KR_APP(2) /* The Construstor for the
					     connection handler */
#define KR_SOCKET     KR_APP(3) /* The socket from the listen socket */
#define KR_LISTENER   KR_APP(4) /* The listener key or VOID if no listener */
#define KR_HANDLER    KR_APP(5) /* The connection handler object */
#define KR_DISCRIM    KR_APP(6) /* The Discrim key to test for listener proc */
#define KR_OSTREAM    KR_APP(9)	/* only used for debugging */

/* DEBUG stuff */
#define dbg_init	0x01u   /* debug initialization logic */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init )

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))
#define DEBUG2(x,y) if (((dbg_##x|dbg_##y) & dbg_flags) == (dbg_##x|dbg_##y))


/* Internal routine prototypes */
uint32_t listen (Message *argmsg);
int processRequest(Message *argmsg);


int
main(void)
{
  Message msg;
  char buff[256]; // Initial parameters
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM); // for debug
  capros_Node_getSlot(KR_CONSTIT, KC_DISCRIM, KR_DISCRIM); 
  capros_Process_getKeyReg(KR_SELF, KR_VOID, KR_LISTENER); // Ensure VOID

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

  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = sizeof(buff);
  msg.rcv_data = buff;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  for(;;) {
    RETURN(&msg);

    msg.snd_invKey = KR_RETURN;

    (void) processRequest(&msg);
  }
}

int
processRequest(Message *argmsg)
{
  uint32_t result = RC_OK;
  uint32_t code = argmsg->rcv_code;
  Message msg;

  argmsg->snd_len = 0;
  argmsg->snd_w1 = 0;
  argmsg->snd_w2 = 0;
  argmsg->snd_w3 = 0;
  argmsg->snd_key0 = KR_VOID;
  argmsg->snd_key1 = KR_VOID;
  argmsg->snd_key2 = KR_VOID;
  argmsg->snd_code = RC_OK;
  
  switch (code) {
  case OC_capros_NetListener_listen:
    {
      result_t ret;
      capros_Discrim_classify(KR_DISCRIM, KR_LISTENER, &ret);
      if (capros_Discrim_clVoid != ret) {
	argmsg->snd_code = RC_capros_NetListener_Already;
	break;
      }

      capros_Process_getKeyReg(KR_SELF, KR_ARG(0), KR_TCP_PORTNO);
      capros_Process_getKeyReg(KR_SELF, KR_ARG(1), KR_CONNECTION_HANDLER_C);
      result = ethread_new_thread(KR_BANK, STACK_SIZE,
				  (uint32_t)&listen, KR_LISTENER);
      if (RC_OK == result) {
	/* Send the listener the port and handler keys */
	msg.snd_invKey = KR_LISTENER;
	msg.snd_key0 = KR_TCP_PORTNO;
	msg.snd_key1 = KR_CONNECTION_HANDLER_C;
	msg.snd_key2 = KR_VOID;
	msg.snd_rsmkey = KR_VOID;
	msg.snd_data = 0;
	msg.snd_len = 0;
	msg.snd_code = 0;
	msg.snd_w1 = 0;
	msg.snd_w2 = 0;
	msg.snd_w3 = 0;
	SEND(&msg);
      }
      //      result = listen(argmsg);
      argmsg->snd_code = result;
      break;
    }
  case OC_capros_key_getType: /* Key type */
    {
      argmsg->snd_code = RC_OK;
      argmsg->snd_w1 = IKT_capros_NetListener;
      break;
    }
  case OC_capros_key_destroy:
    {
      capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPACE, KR_TEMP0);

      /* Invoke the protospace to destroy us and return. */
      protospace_destroy_small(KR_TEMP0, RC_OK);
      // Does not return here.
    }
  default:
    {
      argmsg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
  }
  
  return 1;
}

uint32_t
listen(Message *argmsg)
{
  Message msg;

  /* Set up to receive the port and handler constructor keys */
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

  msg.rcv_key0 = KR_TCP_PORTNO;
  msg.rcv_key1 = KR_CONNECTION_HANDLER_C;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;
  msg.rcv_data = NULL;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  RETURN(&msg);

  DEBUG(init) kdprintf(KR_OSTREAM, "NetListener: waiting for connections\n");
  while (1) {
    result_t rc = capros_TCPListenSocket_accept(KR_TCP_PORTNO, KR_SOCKET);
    
    switch (rc) {
    case RC_OK:
      {
	rc = capros_Constructor_request(KR_CONNECTION_HANDLER_C,
					KR_BANK, KR_SCHED, KR_SOCKET, KR_HANDLER);
	/* TODO Save handler for debugging etc. */
	/* Send the handler off to serve the socket */
	msg.snd_invKey = KR_HANDLER;
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
	SEND(&msg);

	argmsg->snd_code = rc;
      }
    default: return 1;
    }
    return 1;
  }
}
