/*
 * Copyright (C) 2007, Strawberry Development Group
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

#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Number.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM KR_APP(0)
#define KR_TREE0   KR_APP(1)
#define KR_TREE1   KR_APP(2)
#define KR_BIGTREE KR_APP(3)
#define KR_ROTREE  KR_APP(4)
#define KR_KEEPER_PROCESS KR_APP(5)
#define KR_KEEPER_START KR_APP(6)

int
main(void)
{
  int i;
  capros_Number_value numval;

  Message msg;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  result_t retval;

#define ckOK \
  if (retval != RC_OK) \
    kprintf(KR_OSTREAM, "Return code 0x%0x at line %d.\n", retval, __LINE__);

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_PROC2, KR_KEEPER_PROCESS);

  capros_Process_makeStartKey(KR_KEEPER_PROCESS, 5, KR_KEEPER_START);

  /* Get keeper process started. */
  capros_Process_makeResumeKey(KR_KEEPER_PROCESS, KR_KEEPER_PROCESS);
  msg.snd_invKey = KR_KEEPER_PROCESS;
  msg.snd_code = RC_OK;
  SEND(&msg);
  
  kprintf(KR_OSTREAM, "Beginning test...\n");

  retval = capros_SpaceBank_alloc3(KR_BANK, capros_Range_otNode
             | (capros_Range_otNode << 8)
             | (capros_Range_otNode << 16),
             KR_TREE0, KR_TREE1, KR_BIGTREE);
  ckOK

  // Build bigtree.
  retval = capros_Node_setL2v(KR_BIGTREE, capros_Node_l2nSlots);
  ckOK
  uint8_t getl2v;
  retval = capros_Node_getL2v(KR_BIGTREE, &getl2v);
  ckOK
  if (getl2v != capros_Node_l2nSlots)
    kprintf(KR_OSTREAM, "Returned l2v %d at line %d.\n", getl2v, __LINE__);

  retval = capros_Node_swapSlot(KR_BIGTREE, 0, KR_TREE0, KR_VOID);
  ckOK
  retval = capros_Node_swapSlot(KR_BIGTREE, 1, KR_TREE1, KR_VOID);
  ckOK

  // Write slots 0-31 via node.
  for (i = 0; i < 32; i++) {
    numval.value[0] = i;
    retval = capros_Node_writeNumber(KR_TREE0, i, numval);
    ckOK
  }

  // Write slots 32-63 via extended node.
  for (; i < 64; i++) {
    numval.value[0] = i;
    // Use bigtree slot 31 to create a number key.
    retval = capros_Node_writeNumber(KR_BIGTREE, 31, numval);
    ckOK
    retval = capros_Node_getSlot(KR_BIGTREE, 31, KR_TEMP0);	// number key
    ckOK

    retval = capros_Node_swapSlotExtended(KR_BIGTREE, i, KR_TEMP0, KR_VOID);
    ckOK
  }

  // Read all via extended node.
  for (i = 0; i < 64; i++) {
    retval = capros_Node_getSlotExtended(KR_BIGTREE, i, KR_TEMP0);
    ckOK
    retval = capros_Number_get(KR_TEMP0, &numval.value[0],
               &numval.value[1], &numval.value[2]);
    ckOK
    if (numval.value[0] != i)
      kprintf(KR_OSTREAM, "Expecting %d got %d.\n", i, numval.value[0]);
  }

  retval = capros_Node_reduce(KR_BIGTREE, capros_Node_readOnly, KR_ROTREE);
  ckOK

  retval = capros_Node_swapSlotExtended(KR_ROTREE, 31, KR_VOID, KR_VOID);
  if (retval != RC_capros_key_NoAccess)
    kprintf(KR_OSTREAM, "Return code 0x%0x at line %d.\n", retval, __LINE__);

  // Test keeper.

  retval = capros_Node_setKeeper(KR_BIGTREE, KR_KEEPER_START);
  ckOK

  retval = capros_key_destroy(KR_BIGTREE);
  // This should go to the keeper.
  if (retval != 29) \
    kprintf(KR_OSTREAM, "Return code 0x%0x at line %d.\n", retval, __LINE__);

  retval = capros_Node_clearKeeper(KR_BIGTREE);
  ckOK

  retval = capros_key_destroy(KR_BIGTREE);
  // This should NOT go to the keeper.
  if (retval != RC_capros_key_UnknownRequest) \
    kprintf(KR_OSTREAM, "Return code 0x%0x at line %d.\n", retval, __LINE__);

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}
