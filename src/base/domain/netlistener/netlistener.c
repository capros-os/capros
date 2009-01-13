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

#include <domain/domdbg.h>

/* Constituent node contents */
#include "constituents.h"


/* Key registers */
#define KR_TCP_PORTNO KR_APP(1) /* The TCP local port key - for listen */
#define KR_CONNECTION_HANDLER_C KR_APP(2) /* The Construstor for the
					     connection handler */
#define KR_SOCKET     KR_APP(3) /* The socket from the listen socket */
#define KR_OSTREAM    KR_APP(9)	/* only used for debugging */

/* DEBUG stuff */
#define dbg_init	0x01u   /* debug initialization logic */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init )

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))
#define DEBUG2(x,y) if (((dbg_##x|dbg_##y) & dbg_flags) == (dbg_##x|dbg_##y))


/* Internal routine prototypes */
int listen (Message *argmsg);
int processRequest(Message *argmsg);


int
main(void)
{
  Message msg;
  char buff[256]; // Initial parameters
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM); // for debug

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
      capros_Process_getKeyReg(KR_SELF, KR_ARG(0), KR_TCP_PORTNO);
      capros_Process_getKeyReg(KR_SELF, KR_ARG(1), KR_CONNECTION_HANDLER_C);
      result = listen(argmsg);
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

int
listen(Message *argmsg)
{
  DEBUG(init) kdprintf(KR_OSTREAM, "NetListener: waiting for connections\n");
  result_t rc = capros_TCPListenSocket_accept(KR_TCP_PORTNO, KR_SOCKET);

  switch (rc) {
  case RC_OK:
    {
      rc = capros_Constructor_request(KR_CONNECTION_HANDLER_C,
             KR_BANK, KR_SCHED, KR_SOCKET, KR_VOID);
      argmsg->snd_code = rc;
    }
  default: return 1;
  }
  return 1;
}
