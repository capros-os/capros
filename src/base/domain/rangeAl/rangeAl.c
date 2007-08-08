/*
 * Copyright (C) 2007, Strawberry Development Group.
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

#include <eros/target.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>

#include <stdlib.h>
#include <domain/ConstructorKey.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>

#include <idl/capros/Node.h>
#include <idl/capros/rangeAl.h>
#include "constituents.h"
#include "RangeAllocatorStack.c"

#define KR_OSTREAM     KR_APP(0)
#define KR_START       KR_APP(1)


#define CR_CREATE OC_capros_rangeAl_SendCreateRangeAlloc
#define CR_REQUEST OC_capros_rangeAl_SendRequestRange
#define CR_RELEASE OC_capros_rangeAl_SendReleaseRange
#define CR_DEFINE OC_capros_rangeAl_SendDefineRange
#define CR_GET OC_capros_rangeAl_SendGetRangeAlloc

RA* ra;
/* The server's API dictates what data types/values are passed for
   each requested command.  For this example, I'm just using an
   arbitrary array of 5 uint32_t's. */
uint32_t receive[5];

/* Client command processing logic goes in this routine.  Real servers
   will most likely end up with a big switch-case stmt here,
   dispatching client requests based on the incoming opcode (which,
   for the server, is in msg->rcv_code). */
static bool
doSomething(Message *m)
{
 result_t result; 
uint32_t got, expected;
//kprintf(KR_OSTREAM, "*** Server was just invoked with oc = 0x%08x\n",
//m->rcv_code);

  switch(m->rcv_code){
	case CR_DEFINE:{

/* vandy: */
 result = DefineRange(m->rcv_w1, m->rcv_w2, ra);
	if(result)
	m->snd_code= RC_OK;
	else
	m->snd_code= RC_capros_rangeAl_DefineFail;
	}break;

	case CR_REQUEST: {
/* vandy */

	expected =  sizeof(uint32_t);
	got = m->rcv_sent;
	if(m->rcv_limit < got)
	  got= m->rcv_limit;
	if (got != expected){
  	m->snd_code= RC_capros_rangeAl_RangeTaken; 
//	kprintf(KR_OSTREAM, "OOPS\n");

	   }
/*
kprintf(KR_OSTREAM, "r1:  %d:\n",  m->rcv_w1);
kprintf(KR_OSTREAM, "r2:  %d:\n",  m->rcv_w2);
kprintf(KR_OSTREAM, "r3:  %d:\n",  m->rcv_w3);
kprintf(KR_OSTREAM, "receive 0:  %d:\n",  receive[0]);
kprintf(KR_OSTREAM, "receive 1:  %d:\n",  receive[1]);
kprintf(KR_OSTREAM, "receive 2:  %d:\n",  receive[2]);
kprintf(KR_OSTREAM, "receive 3:  %d:\n",  receive[3]);
*/
	result= RequestRange(m->rcv_w1,
				m->rcv_w2,
				m->rcv_w3,
				receive[0],
				ra);
	m->snd_w1= result;
	m->snd_code= RC_OK;
	if(GetLastError(ra)!=NO_ERROR){
//		kprintf(KR_OSTREAM, "error:  %d:\n",  GetLastError(ra));
//		kprintf(KR_OSTREAM, "value:  %d:\n",  m->snd_w1);
  		m->snd_code= RC_capros_rangeAl_RangeTaken; 
	}
	}break;

	case CR_RELEASE: {
	result= ReleaseRange(m->rcv_w1, ra);
	if(result)
	m->snd_code= RC_OK;
	else m->snd_code= RC_capros_rangeAl_ReleaseFail;
	}break;

	case CR_CREATE:{

	
	  ra= CreateRangeAlloc();
	  if(ra)
	  	m->snd_code= RC_OK;
	  else m->snd_code= RC_capros_rangeAl_CreateFail;
	} break;

	case CR_GET:{
		m->snd_code= RC_OK;
		m->snd_len= sizeof(RA);
		m->snd_data= ra;
	}break;

	default:{

/* vandy */
m->snd_code = RC_capros_key_UnknownRequest;

  m->snd_invKey = KR_RETURN;
  m->snd_key0 = KR_VOID;
  m->snd_key1 = KR_VOID;
  m->snd_key2 = KR_VOID;
  m->snd_rsmkey = KR_RETURN;
  m->snd_data = 0;
  m->snd_len = 0;
  m->snd_code = 0;
  m->snd_w1 = 0;
  m->snd_w2 = 0;
  m->snd_w3 = 0;

  m->rcv_key0 = KR_VOID;
  m->rcv_key1 = KR_VOID;
  m->rcv_key2 = KR_VOID;
  m->rcv_rsmkey = KR_RETURN;
  m->rcv_data= receive;
  m->rcv_limit= sizeof(receive);
  m->rcv_code = 0;
  m->rcv_w1 = 0;
  m->rcv_w2 = 0;
  m->rcv_w3 = 0;
  

	} break;
	}

  return true;
}

int
main(void) 
{
  Message msg;
   ra= CreateRangeAlloc();
  /* In order to use "kprintf", you must have the Console key in the
     key register set: */
  capros_Node_getSlot(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);

 	 kprintf(KR_OSTREAM,"Server Running");
  /* This process must pass back a start key to its constructor, in
     order for that constructor to hand out the start key to
     clients. */
  process_make_start_key(KR_SELF, 0, KR_START);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_START;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
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
  msg.rcv_data= receive;
  msg.rcv_limit= sizeof(receive);
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  /* Stay in a processing loop until specifically told to shut down.
     Here's one way of doing that. */
  do {
    msg.rcv_data= receive;
    msg.rcv_limit= sizeof(receive);
    RETURN (&msg);
    msg.snd_key0 = KR_VOID;
  } while (doSomething(&msg));

  return 0;
}
