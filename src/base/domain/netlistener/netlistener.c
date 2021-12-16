/*
 * Copyright (C) 2009, 2010, Strawberry Development Group.
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
#include <idl/capros/ProcCre.h>
#include <idl/capros/NetListener.h>
#include <idl/capros/TCPPortNum.h>
#include <idl/capros/TCPListenSocket.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/Node.h>
#include <idl/capros/Discrim.h>
#include <ethread/ethread.h>

#include <domain/domdbg.h>
#include <domain/assert.h>

/* Constituent node contents */
#include "constituents.h"

/* Stack for the listen process */
unsigned long long listenStack[2048 / sizeof(unsigned long long)];

/* Key registers */
#define KR_TCPPortNum KR_APP(1) /* The TCP local port key - for listen */
#define KR_CONNECTION_HANDLER_C KR_APP(2) /* The Construstor for the
					     connection handler */
#define KR_SOCKET     KR_APP(3) /* The socket from the listen socket */
#define KR_LISTENPROC KR_APP(4) /* The listener process key if any */
#define KR_HANDLER    KR_APP(5) /* The connection handler object */
#define KR_TCPListenSocket KR_APP(6)
#define KR_OSTREAM    KR_APP(9)	/* only used for debugging */

/* DEBUG stuff */
#define dbg_init	0x01u   /* debug initialization logic */
#define dbg_errors      0x02u   /* debug errors */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init | dbg_errors )

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))

bool haveListener = false;

/* Internal routine prototypes */
int listen(void);
int processRequest(Message *argmsg);


int
main(void)
{
  Message msg;
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM); // for debug

  capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);

  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_TEMP0;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;

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
      if (haveListener) {
	argmsg->snd_code = RC_capros_NetListener_Already;
	break;
      }

      capros_Process_getKeyReg(KR_SELF, KR_ARG(0), KR_TCPPortNum);
      capros_Process_getKeyReg(KR_SELF, KR_ARG(1), KR_CONNECTION_HANDLER_C);
      result = ethread_new_thread1(KR_BANK,
                                   (uint8_t *)listenStack + sizeof(listenStack),
				   &listen, KR_LISTENPROC);
      if (RC_OK == result) {
        haveListener = true;	// true forevermore
        ethread_start(KR_LISTENPROC);
      }
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
      if (haveListener) {
        /* The listener thread is most likely waiting for a connection.
        There needs to be a way to stop it from waiting.
        Until then, just destroy the thread. */
        capros_ProcCre_destroyProcess(KR_CREATOR, KR_BANK, KR_LISTENPROC);
      }
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

int
listen(void)
{
  Message msg;

  while (1) {
    DEBUG(init) kprintf(KR_OSTREAM, "NetListener: waiting for connections\n");
    result_t rc = capros_TCPListenSocket_accept(KR_TCPListenSocket, KR_SOCKET);

    switch (rc) {
    case RC_capros_key_Void:
    case RC_capros_key_Restart:
      // There is no TCPListenSocket cap. Get one:
      DEBUG(init) kprintf(KR_OSTREAM, "NetListener: Getting TCPListenSocket\n");
      do {
        rc = capros_TCPPortNum_listen(KR_TCPPortNum, KR_TCPListenSocket);
      } while (rc == RC_capros_key_Restart);
      assert(rc == RC_OK);	// FIXME
      break;

    case RC_OK:
      {
        uint32_t ip;
        uint16_t port;
#if 0
        rc = capros_key_getType(KR_SOCKET, &ip);
        assert(rc == RC_OK);
        assert(ip == IKT_capros_TCPSocket);
#endif
        rc = capros_TCPSocket_getRemoteAddr(KR_SOCKET, &ip, &port);
        assert(rc == RC_OK);
        DEBUG(init) kprintf(KR_OSTREAM, "Got connection from %d.%d.%d.%d:%d\n",
                      ip >> 24,
                      (ip >> 16) & 0xff,
                      (ip >>  8) & 0xff,
                      ip & 0xff,
                      port);

	rc = capros_Constructor_request(KR_CONNECTION_HANDLER_C,
					KR_BANK, KR_SCHED, KR_SOCKET, KR_HANDLER);
        DEBUG(init) kprintf(KR_OSTREAM, "NetListener: Built handler\n");
	/* TODO Save handler for debugging etc. */
	/* Send the handler off to serve the socket */
	msg.snd_invKey = KR_HANDLER;
	msg.snd_key0 = KR_VOID;
	msg.snd_key1 = KR_VOID;
	msg.snd_key2 = KR_VOID;
	msg.snd_rsmkey = KR_VOID;
	msg.snd_len = 0;
	msg.snd_code = 0;
	msg.snd_w1 = 0;
	msg.snd_w2 = 0;
	msg.snd_w3 = 0;
	SEND(&msg);
      }
      break;

    default:
      DEBUG(errors) kdprintf(KR_OSTREAM, "NetListener: accept got rc=%#x\n",
                             rc);
      return 1;		// die
    }
  }
}
