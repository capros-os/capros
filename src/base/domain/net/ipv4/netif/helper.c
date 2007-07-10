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
#include <string.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/DevPrivs.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/NetKey.h>

#include "constituents.h"

#define KR_OSTREAM   KR_APP(0)
#define KR_START     KR_APP(1)
#define KR_DEVPRIVS  KR_APP(2)
#define KR_NETDRIVER KR_APP(3)

/*Globals*/
static int IRQ = 9;

int
main(void)
{
  Message msg;
  result_t result;
  
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);

  kprintf(KR_OSTREAM,"STARTING LANCE HELPER ... [SUCCESS]");

  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_key0 = KR_NETDRIVER;
  RETURN(&msg);
    
  msg.snd_invKey = KR_RETURN;
  msg.rcv_key0 = KR_VOID;

  /* IRQ to wait on */
  IRQ = msg.rcv_w1;
  
  kprintf(KR_OSTREAM, "helper:irq to wait on = %d",IRQ);
  /* Send back to the core driver*/
  SEND(&msg);
  
  msg.snd_invKey = KR_NETDRIVER;
  msg.snd_code = OC_irq_arrived;
  /* Loop infinitely waiting for IRQ */
  for(;;) {
    result = capros_DevPrivs_waitIRQ(KR_DEVPRIVS,IRQ);
    if (result != RC_OK) {
#ifdef DEBUG
      kprintf(KR_OSTREAM, "helper: ERROR on IRQ LINE %d",IRQ);
#endif
    }else {  
      CALL(&msg);/* We have an IRQ !! Signal lance*/
    }
  }
  return 0;
}
