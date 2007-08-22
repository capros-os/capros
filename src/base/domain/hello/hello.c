/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* Hello World

   A silly test program that we use often enough that I decided to
   just put it in the domain tree.
   */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include "constituents.h"

#define KR_OSTREAM  KR_APP(0)

#define KR_ARG0     KR_ARG(0)
#define KR_ARG1     KR_ARG(1)
#define KR_ARG2     KR_ARG(2)

int
ProcessRequest(Message *argmsg)
{
  kprintf(KR_OSTREAM, "hello, world\n");
  
  argmsg->snd_code = RC_OK;
  return 1;
}

void
init_hello()
{
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
}

int
main()
{
  Message msg;

  init_hello();

  capros_Process_makeStartKey(KR_SELF, 0, KR_ARG0);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_ARG0;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_ARG0;
  msg.rcv_key1 = KR_ARG1;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    msg.snd_key0 = KR_ARG0;	/* until proven otherwise */
    msg.snd_invKey = KR_RETURN;
  } while ( ProcessRequest(&msg) );

  return 0;
}
