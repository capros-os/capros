/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Process.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#include <idl/capros/key.h>
#include <idl/capros/Node.h>
#include <idl/capros/Number.h>

/* #define NODEKEYDEBUG */

/* Node key implements node keys, RO node keys, and sense keys.
 * N.B. that the node we point to may NOT be prepared as a node!
 */

/* DANGER -- there is a potential gotcha in the node key
 * implementation.  Suppose that the node key happens to name the
 * caller's domain root. The operations (swap, zero, WriteNumbers)
 * either alter the register values of the domain or cause the domain
 * to become malformed.  The swap operation may, in addition, cause
 * the key registers node to be changed, which has the effect of
 * altering the return key slots.
 * 
 * KeyKOS made this all transparent. It would be nice to make all of
 * this transparent, but if you are holding a node key in the first
 * place then you have sufficient authority to obtain a domain key to
 * yourself, by which any alteration that would leave your domain
 * running can be accomplished.  If you don't want your domain
 * running, be a sport and return to the null key.
 * 
 * Given this, I just made dorking the returnee generate a suitable
 * return code.
 * 
 */

void
Desensitize(Key *k)
{
  if (k == 0)
    return;
  
  switch (keyBits_GetType(k)) {
  case KKT_Node:
  case KKT_GPT:
    keyBits_SetReadOnly(k);
    keyBits_SetNoCall(k);
    keyBits_SetWeak(k);
    break;
  case KKT_Page:
    keyBits_SetReadOnly(k);
    break;
  case KKT_Number:
    break;
  case KKT_Discrim:
    break;
  default:
    /* Safe because only called on invocation block keys: */
    assert( keyBits_IsHazard(k) == false );

    key_NH_SetToVoid(k);

    break;
  }
}

static void
SwapSlot(Invocation * inv, Node * theNode, Key * slot)
{
  node_MakeDirty(theNode);

  COMMIT_POINT();
  
  Key k;	/* temporary in case send and receive slots are the same. */
        
  keyBits_InitToVoid(&k);
  key_NH_Set(&k, slot);
	
  key_NH_Set(slot, inv->entry.key[0]);
  inv_SetExitKey(inv, 0, &k);
  /* There is no need to check this for sense key status, as
   * we would have generated an exception if the
   * node were not writable, and any writable node is also
   * readable in full power form. */

  /* Unchain, but do not unprepare -- the objects do not have
   * on-disk keys. */
  key_NH_Unchain(&k);
}

static void
WalkExtended(Invocation * inv, Node * curNode, bool write)
{
  unsigned int depthRemaining = 32;	// max levels deep
  uint32_t addr = inv->entry.w1;
  unsigned int restrictions = inv->key->keyPerms;

  for(;;) {
    if (write && (restrictions & capros_Node_readOnly)) {
      inv->exit.code = RC_capros_key_NoAccess;
      goto fault_exit;
    }

    uint8_t l2vField = node_GetL2vField(curNode);
    unsigned int curL2v = l2vField & NODE_L2V_MASK;

    if (--depthRemaining == 0) {
      inv->exit.code = RC_capros_Node_TooDeep;
      goto fault_exit;
    }

    unsigned int maxSlot;

    if (l2vField & NODE_KEEPER) {        // it has a keeper
      maxSlot = capros_Node_keeperSlot -1;
    }
    else maxSlot = capros_Node_nSlots -1;

    capros_Node_extAddr_t ndx = addr >> curL2v;
    if (ndx > maxSlot) {
      inv->exit.code = RC_capros_Node_NoAddr;
      goto fault_exit;
    }

    Key * curKey = node_GetKeyAtSlot(curNode, ndx);
	
    /* Only the process creator can get to hazarded slots, and it shouldn't: */
    assert(! keyBits_IsHazard(curKey));

    if (curL2v == 0) {	// at the bottom level
      inv->exit.code = RC_OK;
      if (! write) {	// get
        COMMIT_POINT();

        /* Does not copy hazard bits, but preserves preparation: */
        inv_SetExitKey(inv, 0, curKey);
        // Desensitize?
      } else {		// swap
        SwapSlot(inv, curNode, curKey);
      }
      return;
    }

    addr &= (1ull << curL2v) - 1ull;	// remaining bits of address

    key_Prepare(curKey);

    switch (keyBits_GetType(curKey)) {
    case KKT_Node:
      break;

    default:	// most likely void
      inv->exit.code = RC_capros_Node_NoAddr;
    }

    curNode = (Node *)curKey->u.ok.pObj;
    restrictions |= curKey->keyPerms;
  }
  assert(false);        // can't get here

fault_exit:
  COMMIT_POINT();
  return;
}

void
NodeClone(Node * toNode, Key * fromKey)
{
  bool isWeak = keyBits_IsWeak(fromKey);

  Node * fromNode = objH_ToNode(key_GetObjectPtr(fromKey));

  unsigned slot;
  for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
    node_ClearHazard(toNode, slot);
    if (keyBits_IsRdHazard(node_GetKeyAtSlot(fromNode, slot)))
      node_ClearHazard(fromNode, slot);
  }

  /* The following will do the right thing if the two GPTs are
   * identical, because the NH_Set code checks for this case.
   */
  for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
    key_NH_Set(node_GetKeyAtSlot(toNode, slot),
               node_GetKeyAtSlot(fromNode, slot) );

    if (isWeak)
      Desensitize(node_GetKeyAtSlot(toNode, slot));
  }

  toNode->nodeData = fromNode->nodeData;
}

/* May Yield. */
void
NodeKey(Invocation* inv /*@ not null @*/)
{
  bool isSense = keyBits_IsWeak(inv->key);	// is this feature used?
  bool isFetch = keyBits_IsReadOnly(inv->key);
  bool opaque = inv->key->keyPerms & capros_Node_opaque;

  Node * theNode = (Node *) key_GetObjectPtr(inv->key);

  switch (inv->entry.code) {
  case OC_capros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Node;
      return;
    }

  case OC_capros_Node_getSlot:
    {
      uint32_t slot = inv->entry.w1;

      COMMIT_POINT();

      if (slot >= EROS_NODE_SIZE) {
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      /* All of these complete ok. */
      inv->exit.code = RC_OK;

      Key * sourceSlot = node_GetKeyAtSlot(theNode, slot);

      /* The slot can only be read hazarded if it is a register or
      key register of a process. Only the process creator has node
      keys to such nodes, and it should not be doing this. */
      assert(! keyBits_IsRdHazard(sourceSlot));

      /* Does not copy hazard bits, but preserves preparation: */
      inv_SetExitKey(inv, 0, sourceSlot);

#ifdef NODEKEYDEBUG
      dprintf(true, "Copied key to exit slot %d\n", slot);
#endif
      
      if (isSense)
	Desensitize(inv->exit.pKey[0]);

      return;
    }      

  case OC_capros_Node_swapSlot:
    {
      /* Only the process creator can have node keys to process components,
      and it should not be doing weird stuff: */
      assert(! inv->invokee || theNode != inv->invokee->procRoot);
      assert(! inv->invokee || theNode != inv->invokee->keysNode);
      assert(theNode != act_CurContext()->procRoot);
      assert(theNode != act_CurContext()->keysNode);

      if (isFetch) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_NoAccess;
	return;
      }
	
      uint32_t slot = inv->entry.w1;

      if (slot >= EROS_NODE_SIZE) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      Key * destSlot = node_GetKeyAtSlot(theNode, slot);

      /* Only the process creator can do this, and it shouldn't: */
      assert(! keyBits_IsHazard(destSlot));
	
      inv->exit.code = RC_OK;
      SwapSlot(inv, theNode, destSlot);

#ifdef NODEKEYDEBUG
      dprintf(true, "Swapped key to slot %d\n", slot);
#endif
     
      return;
    }      

  case OC_capros_Node_getSlotExtended:
    WalkExtended(inv, theNode, false);
    return;

  case OC_capros_Node_swapSlotExtended:
    WalkExtended(inv, theNode, true);
    return;

  case OC_capros_Node_reduce:
    {
      COMMIT_POINT();

      uint32_t p = inv->entry.w1;
      if (p & ~ capros_Node_readOnly) {
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      inv->exit.code = RC_OK;
      
      if (inv->exit.pKey[0]) {
        inv_SetExitKey(inv, 0, inv->key);

	inv->exit.pKey[0]->keyPerms |= p;
      }
	
      return;
    }
    
#if 0	// this isn't used
  case OC_capros_Node_clear:
    {
      if (isFetch) {
	inv->exit.code = RC_capros_key_NoAccess;
	return;
      }

      node_MakeDirty(theNode);

      /* If we zero it, we're going to nail all of it's dependencies
       * anyway:
       */
      
      COMMIT_POINT();

      node_DoClearThisNode(theNode);

      act_Prepare(act_Current());
      
      return;
    }
#endif

  case OC_capros_Node_writeNumber:
    {
      uint32_t slot;
      capros_Number_value nkv;

      if (isFetch) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_NoAccess;
	return;
      }

      slot = inv->entry.w1;

      if (slot >= EROS_NODE_SIZE || inv->entry.len != sizeof(nkv)) {
	COMMIT_POINT();
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      /* If we overwrite it, we're going to nail all of it's
       * dependencies anyway:
       */
      
      inv->exit.code = RC_OK;

      node_MakeDirty(theNode);

      COMMIT_POINT();

      inv_CopyIn(inv, inv->entry.len, &nkv);

      node_ClearHazard(theNode, slot);


      assert( keyBits_IsUnprepared(node_GetKeyAtSlot(theNode, slot) ));
      key_SetToNumber(node_GetKeyAtSlot(theNode, slot),
		      nkv.value[2],
		      nkv.value[1],
		      nkv.value[0]);

      act_Prepare(act_Current());

      return;
    }

  case OC_capros_Node_getL2v:
    if (opaque) goto opaqueError;

    COMMIT_POINT();

    inv->exit.code = RC_OK;
    inv->exit.w1 = node_GetL2vField(theNode) & NODE_L2V_MASK;
    return;

  case OC_capros_Node_setL2v:
    if (opaque) goto opaqueError;

    COMMIT_POINT();

    unsigned int newL2v = inv->entry.w1;
    if (! (newL2v < (sizeof(capros_Node_extAddr_t)*8) ))
      goto request_error;

    inv->exit.code = RC_OK;
    uint8_t l2vField = node_GetL2vField(theNode);
    uint8_t oldL2v = l2vField & NODE_L2V_MASK;
    // inv->exit.w1 = oldL2v;
    node_SetL2vField(theNode, l2vField - oldL2v + newL2v);
    return;

  case OC_capros_Node_setKeeper:
    if (opaque) goto opaqueError;

    node_SetSlot(theNode, capros_Node_keeperSlot, inv);

    node_SetL2vField(theNode, node_GetL2vField(theNode) | NODE_KEEPER);
    return;

  case OC_capros_Node_clearKeeper:
    if (opaque) goto opaqueError;

    COMMIT_POINT();

    node_SetL2vField(theNode, node_GetL2vField(theNode) & ~ NODE_KEEPER);
    inv->exit.code = RC_OK;
    return;

  case OC_capros_Node_clone:
    {
      /* copy content of node key in arg0 to current node */
      if (isFetch) {
	inv->exit.code = RC_capros_key_NoAccess;
	return;
      }

      /* Mark the object dirty. */
      node_MakeDirty(theNode);

      key_Prepare(inv->entry.key[0]);

      assert(keyBits_IsPrepared(inv->key));

      COMMIT_POINT();

      if (keyBits_GetType(inv->entry.key[0]) != KKT_Node) {
	inv->exit.code = RC_capros_key_RequestError;
	return;
      }

      NodeClone(theNode, inv->entry.key[0]);
						    
      inv->exit.code = RC_OK;
      return;
    }

  default:
    COMMIT_POINT();

    inv->exit.code = RC_capros_key_UnknownRequest;
    return;
  }

opaqueError:
  COMMIT_POINT();
request_error:
  inv->exit.code = RC_capros_key_RequestError;
  return;
}

