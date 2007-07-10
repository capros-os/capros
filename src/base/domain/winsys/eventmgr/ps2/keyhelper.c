/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* Keyboard Client to the ps2 driver(keyb). This process gets keyboard
 * scan codes from the ps2 driver and pushes them to the window
 * system's event manager */
 
#include <stddef.h>
#include <eros/target.h>
#include <eros/ProcessState.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Ps2.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/ProcessCreatorKey.h>
#include <domain/Runtime.h>
#include <domain/drivers/ps2.h>
#include <domain/EventMgrKey.h>

#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_PS2READER    KR_APP(1)
#define KR_MCLI_C       KR_APP(2)
#define KR_MCLI_S       KR_APP(3)
#define KR_START        KR_APP(4)
#define KR_PARENT       KR_APP(5)
#define KR_SCRATCH      KR_APP(6)

bool
ProcessRequest() 
{
  int32_t data;
  int32_t valid;
  
  (void)capros_Ps2_getKeycode(KR_PS2READER,&data,&valid);

  if (valid >= 0)
    (void)event_mgr_queue_key_data(KR_PARENT, data);
  
  return true;
}

int
main(void) 
{
  Message msg;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  
  COPY_KEYREG(KR_ARG(0), KR_PS2READER);
  
  /* We are done with all the initial setup and will now return
   * our start key. */
  process_make_start_key(KR_SELF,0,KR_START);

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

  msg.rcv_key0   = KR_PARENT;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  RETURN(&msg);
  
  /* Receive the parent key */
  msg.rcv_key0 = KR_VOID;
  msg.snd_key0 = KR_VOID;

  /* Send back to our parent to get it running. Don't RETURN as
   * we need to run too */
  SEND(&msg);
  
  /* Infinitely loop getting char from ps2reader and 
   * passing it to (queuing it up at) the parent's */
  do {
  }while(ProcessRequest());
  
  return 0;
}
