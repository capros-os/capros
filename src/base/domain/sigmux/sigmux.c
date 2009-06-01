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

/* Signal MUX -- distributor for single-bit signals */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>

#include <string.h>
#include <domain/domdbg.h>
#include <domain/SigMuxKey.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define DEBUG if(0)

#define NCLIENT        16
#define KR_OSTREAM     KR_APP(0)
#define KR_SCRATCH     KR_APP(1)

#define KR_CLIENT0     KR_APP(2)
#define KR_CLIENT(x)   (KR_CLIENT0+x)

#define KI_MASTER      128	/* key info of master start key */

typedef struct client_s {
  uint8_t  sleeping;		/* is client asleep? */
  uint32_t pending;		/* signals pending */
  uint32_t wakeup;		/* signals on which to awaken */
} client_s;

client_s client[NCLIENT];

void
teardown()
{
  /* get the protospace */
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_SCRATCH);

  /* destroy as small space. */
  protospace_destroy(KR_VOID, KR_SCRATCH, KR_SELF, KR_CREATOR,
		     KR_BANK, 1);
  /* NOTREACHED */
}

int
ProcessRequest(Message *msg)
{
  int i;
  uint32_t result = RC_OK;
  uint32_t code = msg->rcv_code;
  Message wakeMsg;
  
  msg->snd_w1 = 0;
  msg->snd_key0 = KR_VOID;

  bzero(&wakeMsg, sizeof(wakeMsg));
	
  switch(code) {
  case OC_SigMux_Post:
    {
      if (msg->rcv_keyInfo != KI_MASTER) {
	result = RC_capros_key_UnknownRequest;
	break;
      }
	
      DEBUG
	kdprintf(KR_OSTREAM, "sigmux: posting 0x%x\n", msg->rcv_w1);

      for (i = 0; i < NCLIENT; i++) {
	uint32_t shouldWake = 0;
      
	client[i].pending |= msg->rcv_w1;

	if (client[i].sleeping) {
	    DEBUG
	      kprintf(KR_OSTREAM, "sigmux: client[%d] sleeping for 0x%x\n",
		       i, client[i].wakeup); 
	  shouldWake = client[i].wakeup & client[i].pending;
	  client[i].pending &= ~shouldWake;

	  if (shouldWake) {
	    DEBUG
	      kprintf(KR_OSTREAM, "sigmux: waking client[%d] with 0x%x\n",
		      i, shouldWake); 
	    wakeMsg.snd_w1 = shouldWake;
	    wakeMsg.snd_invKey = KR_CLIENT(i);
	    SEND(&wakeMsg);
	  }
	}
      }
      break;
    }
    
  case OC_SigMux_Wait:
    {
      uint32_t shouldWake;

      if (msg->rcv_keyInfo >= NCLIENT) {
	result = RC_capros_key_UnknownRequest;
	break;
      }
	
      i = msg->rcv_keyInfo;
      client[i].wakeup = msg->rcv_w1;
      
      shouldWake = client[i].wakeup & client[i].pending;

      msg->snd_w1 = shouldWake;

      if (shouldWake) {
	client[i].sleeping = 0;
	client[i].pending &= ~shouldWake;
	DEBUG
	  kprintf(KR_OSTREAM, "sigmux: client %d wakes instantly.\n", i);
	/* We will simply return to the caller */
      }
      else {
	client[i].sleeping = 1;
	msg->snd_invKey = KR_VOID;
	COPY_KEYREG(KR_RETURN, KR_CLIENT(i));
	DEBUG
	  kprintf(KR_OSTREAM, "sigmux: client %d sleeps for 0x%x\n", i, 
		  client[i].wakeup);
      }

      break;
    }
  case OC_SigMux_MakeRecipient:
    {
      if (msg->rcv_keyInfo != KI_MASTER) {
	result = RC_capros_key_UnknownRequest;
	break;
      }

      if (msg->rcv_w1 < NCLIENT) {
	result = capros_Process_makeStartKey(KR_SELF, msg->rcv_w1,
				       KR_SCRATCH);
	msg->snd_key0 = KR_SCRATCH;
      }
      else {
	result = RC_capros_key_RequestError;
      }
      break;
    }
  case OC_capros_key_destroy:
    teardown();
    return 0; /* CAN'T HAPPEN */
    
  default:
    result = RC_capros_key_UnknownRequest;
    break;
  }

  msg->snd_code = result;
  return 1;
}

void
InitSigMux()
{
  int i;

  for (i = 0; i < NCLIENT; i++) {
    client[i].pending = 0;
    client[i].wakeup = 0;
    client[i].sleeping = 0;
  }
}

int
main(void)
{
  uint32_t result;
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
  InitSigMux();
  
  /* Fabricate the master start key and arrange to return that: */
  result = capros_Process_makeStartKey(KR_SELF, KI_MASTER, KR_SCRATCH);
  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Result from sigmux cre strt key: 0x%x\n",
	     result);
  
  msg.snd_key0 = KR_SCRATCH;
  msg.snd_invKey = KR_RETURN;

  msg.rcv_limit = 0;		/* reset receive length */

  for(;;) {

    RETURN(&msg);

    msg.snd_invKey = KR_RETURN;
    
    (void) ProcessRequest(&msg);
  }
}
