/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

/* IPL Tool

   Application that starts all other threads.
   */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>

#include <idl/eros/key.h>
#include <idl/eros/Discrim.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include "constituents.h"

#define KR_THREADLIST KR_APP(0)
#define KR_OSTREAM    KR_APP(1)
#define KR_PRIMEBANK  KR_APP(2)
#define KR_NEWSCHED   KR_APP(3)
#define KR_FAULT      KR_APP(4)

/* This program is one shot with no backing environment -- stack page
 * is provided in the map file. Override __rt_small_process so that we
 * do not get the space initialization behavior that normally goes
 * with small processes. */
uint32_t __rt_small_process = 0;
uint32_t __rt_unkept = 1;

int
main()
{
  Message msg;
  uint32_t kt;

  node_copy(KR_CONSTIT, KC_THREADLIST, KR_THREADLIST);
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_PRIMEBANK, KR_PRIMEBANK);
  node_copy(KR_CONSTIT, KC_NEWSCHED, KR_NEWSCHED);

  kprintf(KR_OSTREAM, "IPL Tool says hello!\n");

  msg.snd_key0 = KR_PRIMEBANK; /* ignored by make fault key */
  msg.snd_key1 = KR_NEWSCHED; /* ignored by make fault key */
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_code = RC_OK;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_len = 0;

 while (eros_key_getType(KR_THREADLIST, &kt) == RC_OK && kt == AKT_Node) {
    unsigned i;
    for (i = 0; i < (EROS_NODE_SIZE-1); i++) {
      uint32_t keyType;
      uint32_t result;

      node_copy(KR_THREADLIST, i, KR_FAULT);

      result = eros_key_getType(KR_FAULT, &keyType);
      if (result == RC_eros_key_Void)
	keyType = AKT_Void;

      if (keyType == AKT_Process) {
	/* If the key is a process key, fabricate a fault key to it in
	   order to set it in motion. */
	process_make_fault_key(KR_FAULT, KR_FAULT);

	msg.snd_invKey = KR_FAULT;
	msg.snd_code = RC_OK; 
      }
      else {
	/* Assume it's a constructor key */
	msg.snd_code = OC_Constructor_Request;
	msg.snd_invKey = KR_FAULT;
      }

      SEND(&msg);
    }

    node_copy(KR_THREADLIST, EROS_NODE_SIZE-1, KR_THREADLIST);
  }

  return 0;
}
