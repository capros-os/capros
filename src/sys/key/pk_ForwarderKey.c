/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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
#include <kerninc/Activity.h>
#include <disk/Forwarder.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/Forwarder.h>

void
ForwarderKey(Invocation* inv)
{
  uint32_t slot;
  Node * theNode = (Node *) key_GetObjectPtr(inv->key);

  if (inv->key->keyData & capros_Forwarder_opaque) {
    if (theNode->nodeData & ForwarderBlocked) {
      act_SleepOn(ObjectStallQueueFromObHdr(&theNode->node_ObjHdr));
      act_Yield();
    }

    if (inv->key->keyData & capros_Forwarder_sendCap) {
      /* Not hazarded because invocation key */
      key_NH_Set(&inv->scratchKey, inv->key);
      keyBits_SetType(&inv->scratchKey, KKT_Forwarder);
      inv->scratchKey.keyData = 0;	// not capros_Forwarder_opaque
      inv->entry.key[2] = &inv->scratchKey;
      inv->flags |= INV_SCRATCHKEY;
    }

    if (inv->key->keyData & capros_Forwarder_sendWord) {
      Key * dataKey = &theNode->slot[ForwarderDataSlot];
      assert(keyBits_IsType(dataKey, KKT_Number));
      inv->entry.w3 = dataKey->u.nk.value[0];
    }

    Key * targetSlot = node_GetKeyAtSlot(theNode, ForwarderTargetSlot);
    inv_InvokeGateOrVoid(inv, targetSlot);
    return;
  }

  // The key is not opaque.
  
  inv_GetReturnee(inv);

  switch (inv->entry.code) {
  case OC_capros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Forwarder;
      break;
    }

  case OC_capros_Forwarder_getTarget:
    {
      slot = ForwarderTargetSlot;
      goto getSlot;
    }

  case OC_capros_Forwarder_getSlot:
    {
      slot = inv->entry.w1;
      if (slot > capros_Forwarder_maxSlot) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	break;
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
      break;
    }

  case OC_capros_Forwarder_swapTarget:
    {
      slot = ForwarderTargetSlot;
      goto swapSlot;
    }

  case OC_capros_Forwarder_swapSlot:
    {
      slot = inv->entry.w1;
      if (slot > capros_Forwarder_maxSlot) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	break;
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
      break;
    }

  case OC_capros_Forwarder_getDataWord:
    {
      KeyBits * dataKey = node_GetKeyAtSlot(theNode, ForwarderDataSlot);
      assert(keyBits_IsType(dataKey, KKT_Number));
      
      COMMIT_POINT();

      inv->exit.w1 = dataKey->u.nk.value[0];
      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_Forwarder_swapDataWord:
    {
      KeyBits * dataKey = node_GetKeyAtSlot(theNode, ForwarderDataSlot);
      assert(keyBits_IsType(dataKey, KKT_Number));

      node_MakeDirty(theNode);
      
      COMMIT_POINT();

      inv->exit.w1 = dataKey->u.nk.value[0];
      dataKey->u.nk.value[0] = inv->entry.w1;
      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_Forwarder_clearBlocked:
    {
      node_MakeDirty(theNode);

      COMMIT_POINT();

      theNode->nodeData &= ~ForwarderBlocked;

      // Wake up any processes that were blocked.
      sq_WakeAll(ObjectStallQueueFromObHdr(node_ToObj(theNode)), false);

      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_Forwarder_setBlocked:
    {
      node_MakeDirty(theNode);

      COMMIT_POINT();

      theNode->nodeData |= ForwarderBlocked;
      inv->exit.code = RC_OK;
      break;
    }

  case OC_capros_Forwarder_getOpaqueForwarder:
    {
      COMMIT_POINT();

      uint32_t w = inv->entry.w1; /* the flags */
      if (w & ~(capros_Forwarder_sendCap | capros_Forwarder_sendWord)) {
	inv->exit.code = RC_capros_key_RequestError;
	break;
      }

      if (inv->exit.pKey[0]) {
        inv_SetExitKey(inv, 0, inv->key);
	inv->exit.pKey[0]->keyData = w | capros_Forwarder_opaque;
      }

      inv->exit.code = RC_OK;
      break;
    }

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  ReturnMessage(inv);
}
