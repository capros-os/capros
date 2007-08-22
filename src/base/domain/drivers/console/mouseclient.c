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

/* Mouse Client to the ps2 driver(ps2reader). This process gets mouse
 * data & wastes it, as the stream interface doesn't need mouse data,
 * Nevertheless we need to flush out mouse data from the ps2 buffer. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/Ps2.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM    KR_APP(0)
#define KR_PS2READER  KR_APP(1)
#define KR_START      KR_APP(2)

#define KR_SCRATCH    KR_APP(5)

/* GLobals */
int32_t w1,w2,w3;

int 
main() {
  Message msg;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  
  COPY_KEYREG(KR_ARG(0),KR_PS2READER);
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

  msg.rcv_key0   = KR_SCRATCH;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit  = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  /* Start looking for mouse packets data  */
  SEND(&msg);
    
  /* Call ps2reader to look for any mouse packets in the ps2 h/w buffer */
  do {
    (void)capros_Ps2_getMousedata(KR_PS2READER,&w1,&w2,&w3);
  } while (1);

  return 0;
}
