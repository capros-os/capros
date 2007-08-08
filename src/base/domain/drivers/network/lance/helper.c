/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

/* This process is a helper to lance. It waits for IRQ9 to arrive
 * and then turns around and calls lance */

#include <stddef.h>
#include <eros/target.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>

#include <idl/capros/key.h>
#include <idl/capros/DevPrivs.h>
#include <idl/capros/Node.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/NetKey.h>

#include "lance.h"
#include "constituents.h"

#define KR_OSTREAM  KR_APP(0)
#define KR_START    KR_APP(1)
#define KR_DEVPRIVS KR_APP(2)
#define KR_LANCECALL KR_APP(3)

//#define DEBUG 1

/*Globals*/
static int IRQ = 9;

/* Process Request*/
int 
ProcessRequest(Message *msg) 
{
  msg->snd_len   = 0;
  msg->snd_key0  = KR_VOID;
  msg->snd_key1  = KR_VOID;
  msg->snd_key2  = KR_VOID;
  msg->snd_rsmkey= KR_RETURN;
  msg->snd_code  = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_VOID;

  switch (msg->rcv_code) {
  case OC_netdriver_key: 
    {
    IRQ = msg->rcv_w1;
    return 0;
    }
  default:
    break;
  }
  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}


int
main(void)
{
  Message msg;
  uint32_t result;

  capros_Node_getSlot(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);

#ifdef DEBUG  
  kprintf(KR_OSTREAM,"STARTING LANCE HELPER ... [SUCCESS]");
#endif  

  /* Make a start key to pass back to constructor */
  process_make_start_key(KR_SELF, 0, KR_START);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0   = KR_VOID;
  msg.rcv_key1   = KR_LANCECALL;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit  = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0 = KR_VOID;
    msg.snd_rsmkey = KR_RETURN;
  }while(ProcessRequest(&msg));
  
  /* IRQ to wait on */
  IRQ = msg.rcv_w1;

#ifdef DEBUG
  kprintf(KR_OSTREAM, "helper:irq received = %d ... [SUCCESS]",IRQ);
#endif
  
  /* When we receive lance's Start Key we are out of 
   * the ProcessRequest loop. Now SEND to lance to 
   * get it running and we loop waiting for IRQ */
  
  /* Send back to lance*/
  SEND(&msg);
  
  msg.snd_invKey = KR_LANCECALL;
  msg.snd_key0   = KR_VOID;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = OC_irq_arrived;
  msg.snd_w1 = IRQ;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
 
  msg.rcv_key0   = KR_VOID;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_code = 0;
  msg.rcv_w1   = 0;
  msg.rcv_w2   = 0;
  msg.rcv_w3   = 0;
  
  /* Loop infinitely waiting for IRQ */
  for(;;) {
    result = capros_DevPrivs_waitIRQ(KR_DEVPRIVS,IRQ);
    if (result != RC_OK) {
#ifdef DEBUG
      kprintf(KR_OSTREAM, "Lance helper: ERROR on IRQ LINE %d",IRQ);
#endif
    }else {  
      CALL(&msg);/* We have an IRQ !! Signal lance*/
    }
  }
  return 0;
}
