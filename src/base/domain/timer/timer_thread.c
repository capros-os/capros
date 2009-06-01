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

#include <stddef.h>
#include <string.h>
#include <eros/target.h>
#include <eros/cap-instr.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/timer/timer_client.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM  KR_APP(0)
#define KR_SLEEP    KR_APP(1)
#define KR_TMP      KR_APP(3)
#define KR_START    KR_APP(5)
#define KR_CLIENT   KR_APP(6)

int
main(void)
{
  Message m;
  uint32_t interval = 0;

  capros_Node_getSlot(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT,KC_SLEEP,KR_SLEEP);
  
  COPY_KEYREG(KR_ARG(0), KR_CLIENT);

  kprintf(KR_OSTREAM,"Timeout Thread says hi...\n");

  /* Fabricate a start key */
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);

  memset(&m, 0, sizeof(Message));
  m.rcv_rsmkey = KR_RETURN;
  m.snd_invKey = KR_RETURN;
  m.snd_key0 = KR_START;

  /* Return start key and wait for interval */
  RETURN(&m);

  kprintf(KR_OSTREAM, "Timer Thread: received interval = %u\n", m.rcv_w1);
  interval = m.rcv_w1;

  m.snd_code = RC_OK;
  m.snd_key0 = KR_VOID;
  SEND(&m);

  for (;;) {
    capros_Sleep_sleep(KR_SLEEP, interval);
    capros_timer_timer_client_wakeup(KR_CLIENT);
  }
 
  return 0;
}
