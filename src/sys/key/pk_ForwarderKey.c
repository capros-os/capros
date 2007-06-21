/*
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

#include <kerninc/kernel.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <kerninc/Forwarder.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>

#include <idl/eros/key.h>
#include <idl/eros/Forwarder.h>

void
ForwarderKey(Invocation* inv)
{
  uint32_t slot;
  // We should only get here if the key is not opaque:
  assert(! (inv->key->keyData & eros_Forwarder_opaque));

  Node * theNode = (Node *) key_GetObjectPtr(inv->key);

  switch (inv->entry.code) {
  case OC_eros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Forwarder;
      return;
    }

  case OC_eros_Forwarder_getTarget:
    {
      slot = ForwarderTargetSlot;
      goto getSlot;
    }

  case OC_eros_Forwarder_getSlot:
    {
      slot = inv->entry.w1;
      if (slot > eros_Forwarder_maxSlot) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }
	
getSlot:
      COMMIT_POINT();

      KeyBits * theKey = node_GetKeyAtSlot(theNode, slot);

      // Slots in a Forwarder node are never hazarded.
      assert(! keyBits_IsHazard(theKey));
      // A Forwarder node cannot be a key registers node.
      assert(theKey != inv->entry.key[0]);

      inv_SetExitKey(inv, 0, theKey);

      inv->exit.code = RC_OK;
      return;
    }

  case OC_eros_Forwarder_swapTarget:
    {
      slot = ForwarderTargetSlot;
      goto swapSlot;
    }

  case OC_eros_Forwarder_swapSlot:
    {
      slot = inv->entry.w1;
      if (slot > eros_Forwarder_maxSlot) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }
	
swapSlot:
      node_MakeDirty(theNode);

      COMMIT_POINT();

      KeyBits * theKey = node_GetKeyAtSlot(theNode, slot);

      // Slots in a Forwarder node are never hazarded.
      assert(! keyBits_IsHazard(theKey));
      // A Forwarder node cannot be a key registers node.
      assert(theKey != inv->entry.key[0]);

      Key k;		/* temporary, in case send and receive
			key registers are the same. */
      keyBits_InitToVoid(&k);

      key_NH_Set(&k, theKey); 

      key_NH_Set(theKey, inv->entry.key[0]);
      inv_SetExitKey(inv, 0, &k);

      key_NH_Unchain(&k);

      inv->exit.code = RC_OK;
      return;
    }

  case OC_eros_Forwarder_getDataWord:
    {
      KeyBits * dataKey = node_GetKeyAtSlot(theNode, ForwarderDataSlot);
      assert(keyBits_IsType(dataKey, KKT_Number));
      
      COMMIT_POINT();

      inv->exit.w1 = dataKey->u.nk.value[0];
      inv->exit.code = RC_OK;
      return;
    }

  case OC_eros_Forwarder_swapDataWord:
    {
      KeyBits * dataKey = node_GetKeyAtSlot(theNode, ForwarderDataSlot);
      assert(keyBits_IsType(dataKey, KKT_Number));
      
      COMMIT_POINT();

      node_MakeDirty(theNode);

      inv->exit.w1 = dataKey->u.nk.value[0];
      dataKey->u.nk.value[0] = inv->entry.w1;
      inv->exit.code = RC_OK;
      return;
    }

  case OC_eros_Forwarder_clearBlocked:
    {
      COMMIT_POINT();

      node_MakeDirty(theNode);

      theNode->nodeData &= ~ForwarderBlocked;

      // Wake up any processes that were blocked.
      sq_WakeAll(ObjectStallQueueFromObHdr(node_ToObj(theNode)), false);

      inv->exit.code = RC_OK;
      return;
    }

  case OC_eros_Forwarder_setBlocked:
    {
      COMMIT_POINT();

      node_MakeDirty(theNode);

      theNode->nodeData |= ForwarderBlocked;
      inv->exit.code = RC_OK;
      return;
    }

  case OC_eros_Forwarder_getOpaqueForwarder:
    {
      COMMIT_POINT();

      uint32_t w = inv->entry.w1; /* the flags */
      if (w & ~(eros_Forwarder_sendNode | eros_Forwarder_sendWord)) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

      if (inv->exit.pKey[0]) {
        inv_SetExitKey(inv, 0, inv->key);
	inv->exit.pKey[0]->keyData = w | eros_Forwarder_opaque;
      }

      inv->exit.code = RC_OK;
      return;
    }

  default:
    COMMIT_POINT();

    inv->exit.code = RC_eros_key_UnknownRequest;
    return;
  }
}
