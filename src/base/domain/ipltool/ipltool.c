/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

/* IPL Tool

   Application that starts all other threads.
   */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <disk/NPODescr.h>

#include <idl/capros/key.h>
#include <idl/capros/Discrim.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/MigratorTool.h>

#include <idl/capros/Constructor.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>
#include "constituents.h"

/* Bypass all the usual initialization. */
unsigned long __rt_runtime_hook = 0;

#define KR_OSTREAM    KR_APP(0)
#define KR_VOLSIZE    KR_APP(1)
#define KR_PRIMEBANK  KR_APP(2)
#define KR_NEWSCHED   KR_APP(3)
#define KR_THREADLIST KR_APP(4)
#define KR_FAULT      KR_APP(5)

#define dbg_init	0x1 

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

void
ProcessThreadlist(void)
{
  Message msg;
  uint32_t kt;

  msg.snd_key0 = KR_PRIMEBANK; /* ignored by make fault key */
  msg.snd_key1 = KR_NEWSCHED; /* ignored by make fault key */
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code = RC_OK;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_len = 0;

  while (capros_key_getType(KR_THREADLIST, &kt) == RC_OK && kt == AKT_Node) {
    unsigned i;
    for (i = 0; i < (EROS_NODE_SIZE-1); i++) {
      uint32_t keyType;
      uint32_t result;

      capros_Node_getSlot(KR_THREADLIST, i, KR_FAULT);

      result = capros_key_getType(KR_FAULT, &keyType);
      if (result == RC_capros_key_Void)
        continue;

      if (keyType == AKT_Process) {
        DEBUG(init) kdprintf(KR_OSTREAM,
                      "Starting process %d (in key %d)\n", i, KR_FAULT);
	/* If the key is a process key, fabricate a fault key to it in
	   order to set it in motion. */
	capros_Process_makeResumeKey(KR_FAULT, KR_FAULT);

	msg.snd_invKey = KR_FAULT;
	msg.snd_code = RC_OK; 
      }
      else {
	/* Assume it's a constructor key */
        DEBUG(init) kdprintf(KR_OSTREAM,
                      "Starting constructor in key %d\n", KR_FAULT);
	msg.snd_code = OC_capros_Constructor_request;
	msg.snd_invKey = KR_FAULT;
      }

      SEND(&msg);
    }

    capros_Node_getSlot(KR_THREADLIST, EROS_NODE_SIZE-1, KR_THREADLIST);
  }
}

int
main()
{
  kprintf(KR_OSTREAM, "IPL Tool says hello!\n");

  ProcessThreadlist();

  return 0;
}
