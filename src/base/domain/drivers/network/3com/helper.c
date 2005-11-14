/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

/* This process is a helper to network driver. It waits for an IRQ to arrive
 * and then turns around and calls the core driver */
#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/cap-instr.h>

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/NetKey.h>

#include "constituents.h"

#define KR_OSTREAM   KR_APP(0)
#define KR_START     KR_APP(1)
#define KR_DEVPRIVS  KR_APP(2)
#define KR_3COM      KR_APP(3)

/*Globals*/
static int IRQ = 9;

int
main(void)
{
  Message msg;
  uint32_t result;

  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);

  COPY_KEYREG(KR_ARG(0),KR_3COM);
  kprintf(KR_OSTREAM,"STARTING LANCE HELPER ... [SUCCESS]");

  /* Make a start key to pass back to constructor */
  process_make_start_key(KR_SELF, 0, KR_START);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0   = KR_VOID;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit  = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  RETURN(&msg);
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  
  /* IRQ to wait on */
  IRQ = msg.rcv_w1;

  kprintf(KR_OSTREAM, "helper:irq to wait on = %d",IRQ);
  
  /* When we receive lance's Start Key we are out of 
   * the ProcessRequest loop. Now SEND to lance to 
   * get it running and we loop waiting for IRQ */
  
  /* Send back to lance*/
  SEND(&msg);
  
  msg.snd_invKey = KR_3COM;
  msg.snd_code = OC_irq_arrived;
  
  /* Loop infinitely waiting for IRQ */
  for(;;) {
    result = eros_DevPrivs_waitIRQ(KR_DEVPRIVS,IRQ);
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
