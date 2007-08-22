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

#include <stddef.h>
#include <eros/target.h>
#include <eros/stdarg.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>

#include <stdlib.h>
#include <string.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Need the interface to this domain */
#include <domain/EventMgrKey.h>

/* Need the Window System interface for dispatching user events */
#include <idl/capros/winsys/master.h>

/* Need the PS/2 interface */
#include <idl/capros/Ps2.h>

#include "constituents.h"
#include "debug.h"

#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/
#define KR_PS2READER     KR_APP(1) /* The start key for the PS/2 driver */
#define KR_WINDOW_SYSTEM KR_APP(2) /* Start key for the window system */
#define KR_START         KR_APP(3)  /* generic start key for this domain */
#define KR_PS2MOUSE_HELPER KR_APP(4) /* PS2 helper domain */
#define KR_PS2KEY_HELPER   KR_APP(5) /* PS2 helper domain */

void
send_key(cap_t target, cap_t key, uint32_t opcode)
{
  Message m;

  memset(&m, 0, sizeof(m));
  m.snd_invKey = target;
  m.snd_code = opcode;
  m.rcv_rsmkey = KR_VOID;
  m.snd_key0 = key;
  CALL(&m);
}

static bool
ProcessRequest(Message *m)
{
  switch(m->rcv_code) {
  case OC_EventMgr_KeyData:
    {
      m->snd_code = eros_domain_winsys_master_keybd_event(KR_WINDOW_SYSTEM, 
							  m->rcv_w1);
    }
    break;

  case OC_EventMgr_MouseData:
    {
      m->snd_code = eros_domain_winsys_master_mouse_event(KR_WINDOW_SYSTEM, 
							  m->rcv_w1,
							  (int8_t)m->rcv_w2,
							  (int8_t)m->rcv_w3);
    }
    break;

  default:
    {
      kprintf(KR_OSTREAM, "*** Bad User Input Event ***\n");
    }
  }
  return true;
}

int
main(void)
{
  result_t result;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_PS2READER, KR_PS2READER);
  node_extended_copy(KR_CONSTIT, KC_PS2MOUSE, KR_PS2MOUSE_HELPER);
  node_extended_copy(KR_CONSTIT, KC_PS2KEYBD, KR_PS2KEY_HELPER);

  /* On construction, there should be a start key to the window system
     */
  COPY_KEYREG(KR_ARG(0), KR_WINDOW_SYSTEM);

  kprintf(KR_OSTREAM, "Event Manager says 'hi'!\n");

  /* Fabricate a generic start key to self */
  if (capros_Process_makeStartKey(KR_SELF, 0, KR_START) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate a start key "
	    "to myself...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* First construct the PS2 reader */
  if (constructor_request(KR_PS2READER, KR_BANK, KR_SCHED, KR_VOID,
			  KR_PS2READER) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: eventmgr unable to construct PS2 reader\n");
    return -1;
  }

  /* Initialize the PS2 reader.  If this fails, we obviously don't
     need to construct the PS2 helpers. */
  result = capros_Ps2_initPs2(KR_PS2READER);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: ps2 reader wouldn't initialize: 0x%08x\n",
	    result);
    return -1;
  }

  /* Construct the helpers and pass them key to PS2 reader */
  if (constructor_request(KR_PS2MOUSE_HELPER, KR_BANK, KR_SCHED, KR_PS2READER, 
			  KR_PS2MOUSE_HELPER) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: eventmgr unable to construct PS2 mouse "
	    "helper!\n");
    return -1;
  }

  if (constructor_request(KR_PS2KEY_HELPER, KR_BANK, KR_SCHED, KR_PS2READER,
			  KR_PS2KEY_HELPER) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: eventmgr unable to construct PS2 key "
	    "helper!\n");
    return -1;
  }

  /* Now send each helper the start key to this domain */
  send_key(KR_PS2MOUSE_HELPER, KR_START, 0);
  send_key(KR_PS2KEY_HELPER, KR_START, 0);

  /* Now enter processing loop */
  {
    Message m;

    memset(&m, 0, sizeof(Message));

    m.snd_invKey = KR_RETURN;

    /* Make sure we can return to the caller */
    m.rcv_rsmkey = KR_RETURN;

    /* Send back the generic start key */
    m.snd_key0 = KR_START;

    do {
      RETURN(&m);

      /* Zero out received key and start key slots */
      m.rcv_key0 = KR_VOID;
      m.snd_key0 = KR_START;

    } while (ProcessRequest(&m));
  }

  return 0;
}
