/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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

/* Domain for continuously waiting for the expiration of a specified
   interval and sending a message to its client.
 */
#include <stddef.h>
#include <string.h>
#include <eros/target.h>
#include <eros/cap-instr.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/timer/manager.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/ConstructorKey.h>

#include "constituents.h"

#define KR_OSTREAM        KR_APP(0)
#define KR_START          KR_APP(1)
#define KR_CLIENT         KR_APP(2)
#define KR_TIMER_THREAD_C KR_APP(3)
#define KR_TIMER_THREAD   KR_APP(4)
#define KR_NEW_BANK       KR_APP(5)

static uint32_t interval = 500;
static bool running = false;

static void
send_interval_to_thread(cap_t kr_thread, uint32_t interval)
{
  Message m;

  memset(&m, 0, sizeof(Message));
  m.snd_invKey = kr_thread;
  m.snd_w1 = interval;
  CALL(&m);
}

static bool
ProcessRequest(Message *m)
{
  switch(m->rcv_code) {
  case OC_capros_timer_manager_setInterval:
    {
      kprintf(KR_OSTREAM, "Timer Mgr asked to set interval to %u.\n", 
	      m->rcv_w1);
      interval = m->rcv_w1;
      m->snd_code = RC_OK;
    }
    break;

  case OC_capros_timer_manager_startTimer:
    {
      if (!running) {
	kprintf(KR_OSTREAM, "Timer Mgr asked to start timer.\n");

	/* Buy a sub bank for this thread */
	capros_SpaceBank_createSubBank(KR_BANK, KR_NEW_BANK);

	/* Now construct the timer thread */
	if (constructor_request(KR_TIMER_THREAD_C, KR_NEW_BANK, KR_SCHED, 
				KR_CLIENT, KR_TIMER_THREAD) != RC_OK) {
	  kprintf(KR_OSTREAM, "**ERROR: Timer Mgr couldn't construct thread\n");
	  return -1;
	}

	send_interval_to_thread(KR_TIMER_THREAD, interval);

	running = true;
      }
      m->snd_code = RC_OK;
    }
    break;

  case OC_capros_timer_manager_stopTimer:
    {
      /* FIX: Shoot the bank from which the timer thread was created
	 */
      kprintf(KR_OSTREAM, "Timer Mgr destroying sub-bank for TIMER_THREAD!\n");
#if 0
      capros_key_destroy(KR_NEW_BANK, 1);
#endif
      running = false;
      m->snd_code = RC_OK;
    }
    break;

  default:
    {
      m->snd_code = RC_capros_key_UnknownRequest;
    }
    break;

  }
  return true;
}

int
main(void)
{
  Message msg;
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_TIMER_THREAD, KR_TIMER_THREAD_C);

  
  COPY_KEYREG(KR_ARG(0), KR_CLIENT);
  
  kprintf(KR_OSTREAM,"Timeout Agent says hi...\n");

  /* Fabricate a start key to pass back to constructor */
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);
  
  /* Main processing loop */
  memset(&msg, 0, sizeof(Message));
  msg.rcv_rsmkey = KR_RETURN;
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_START;

  kprintf(KR_OSTREAM, "Timer Mgr entering main processing loop...\n");

  do {
    RETURN(&msg);
    msg.rcv_rsmkey = KR_RETURN;
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0 = KR_VOID;
  } while (ProcessRequest(&msg));
  
  return 0;
}
