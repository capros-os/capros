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

/* This process is a helper to ps2reader. It waits for IRQ12 to arrive
 * and then turns around and calls ps2reader signalling the arrival of
 * IRQ12, so that ps2reader can read the ps2 h/w buffer for mouse data */

#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/cap-instr.h>

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>
#include <idl/eros/Ps2.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM    KR_APP(0)
#define KR_START      KR_APP(1)
#define KR_DEVPRIVS   KR_APP(2)
#define KR_PS2READER     KR_APP(3)

int
main(void)
{
  Message msg;
  uint32_t result;

  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);
  
  COPY_KEYREG(KR_ARG(0),KR_PS2READER);
  
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
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit  = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  /* Send back to ps2reader*/
  SEND(&msg);
  
  /* Loop infinitely waiting for IRQ12 */
  for(;;) {
    result = eros_DevPrivs_waitIRQ(KR_DEVPRIVS, 12);
    if (result != RC_OK) {
      kprintf(KR_OSTREAM, "helper12: ERROR on 12");
    }else {  
      /* We have an IRQ on 12 !! Signal ps2reader */
      (void)eros_Ps2_irqArrived(KR_PS2READER,eros_Ps2_IRQ12);
    }
  }
  return 0;
}
