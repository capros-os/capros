/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution,
 * and is derived from the EROS Operating System distribution.
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

/* Mouse Client to the ps2 driver(ps2reader). This process gets mouse
 * data & calls the process specified in its builder Key , passing
 * (queuing?) MouseEvents */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/Ps2.h>

#include <stdlib.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM    KR_APP(0)
#define KR_PS2READER  KR_APP(1)
#define KR_START      KR_APP(2)
#define KR_PARENT     KR_APP(3)
#define KR_SCRATCH    KR_APP(4)

int 
main() {
  int32_t data;
  int32_t valid;

  Message msg;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  
  COPY_KEYREG(KR_ARG(0), KR_PS2READER);
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);
  
  /* Return back a start key to our builder */
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
  msg.rcv_limit  = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  RETURN(&msg);
  
  msg.snd_key0 = KR_VOID;
  msg.rcv_key0 = KR_VOID;
  
  /* We now have all the keys we need: PS2READER_KEY & PARENT_KEY
   * Send back to parent and keep running...
   */
  SEND(&msg);
    
  /* Call ps2reader to look for any key packets in the ps2 h/w buffer.
      Disregard them since the test domain doesn't need them. However,
      we must remove them from the PS2 h/w buffer, so this helper is
      required. */
  do {
    (void)capros_Ps2_getKeycode(KR_PS2READER,&data,&valid);
  } while (true);

  return 0;
}
