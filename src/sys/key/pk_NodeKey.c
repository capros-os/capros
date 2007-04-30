/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <kerninc/Process.h>
#include <kerninc/SegWalk.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>
#include <eros/NodeKey.h>

#include <idl/eros/key.h>
#include <idl/eros/Number.h>

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
  case KKT_Segment:
  case KKT_Wrapper:
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

/* May Yield. */
void
NodeKey(Invocation* inv /*@ not null @*/)
{
  bool isSense = keyBits_IsWeak(inv->key);
  bool isFetch = keyBits_IsReadOnly(inv->key);


  Node *theNode = (Node *) key_GetObjectPtr(inv->key);


  if (inv->invokee && theNode == inv->invokee->procRoot
      && OC_Node_Swap == inv->entry.code)
    dprintf(true, "Modifying invokee domain root\n");

  if (inv->invokee && theNode == inv->invokee->keysNode
      && OC_Node_Swap == inv->entry.code)
    dprintf(true, "Modifying invokee keys node\n");

  switch (inv->entry.code) {
  case OC_eros_key_getType:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = AKT_Node;
      return;
    }

  case OC_Node_Copy:
    {
      uint32_t slot = inv->entry.w1;

      COMMIT_POINT();

      if (slot >= EROS_NODE_SIZE) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

      /* All of these complete ok. */
      inv->exit.code = RC_OK;
      

      if ( keyBits_IsRdHazard(node_GetKeyAtSlot(theNode, slot) ))
	node_ClearHazard(theNode, slot);

      /* Does not copy hazard bits, but preserves preparation: */

      inv_SetExitKey(inv, 0, &theNode->slot[slot]);


#ifdef NODEKEYDEBUG
      dprintf(true, "Copied key to exit slot %d\n", slot);
#endif
      
      if (isSense)
	Desensitize(inv->exit.pKey[0]);

      act_Prepare(act_Current());

      return;
    }      

  case OC_Node_Swap:
    {
      uint32_t slot;
      if (theNode == act_CurContext()->keysNode)
	dprintf(true, "Swap involving sender keys\n");


      if (isFetch) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_NoAccess;
	return;
      }
	
      slot = inv->entry.w1;

      if (slot >= EROS_NODE_SIZE) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }
	
      /* All of these complete ok. */
      inv->exit.code = RC_OK;

      node_MakeDirty(theNode);
      
      COMMIT_POINT();			/* advance the PC! */
      
      /* Following will not cause dirty node because we forced it
       * dirty above the commit point.
       */
      node_ClearHazard(theNode, slot);

      /* Tread carefully, because we need to rearrange the CPU
       * reserve linkages.
       */
      {
	Key k;			/* temporary in case send and receive */
				/* slots are the same. */
        
        keyBits_InitToVoid(&k);
	key_NH_Set(&k, &theNode->slot[slot]);
	
	key_NH_Set(node_GetKeyAtSlot(theNode, slot), inv->entry.key[0]);
	inv_SetExitKey(inv, 0, &k);

	/* Unchain, but do not unprepare -- the objects do not have
	 * on-disk keys. 
	 */
	key_NH_Unchain(&k);
       
      }

#ifdef NODEKEYDEBUG
      dprintf(true, "Swapped key to slot %d\n", slot);
#endif
     
      /* Thread::Current() changed to act_Current() */
      act_Prepare(act_Current());
     
      return;
    }      

  case OC_Node_Extended_Copy:
  case OC_Node_Extended_Swap:
    {
      SegWalk wi;
      bool result;
      uint32_t slot;
      Node *theNode = 0;

      wi.faultCode = FC_NoFault;
      wi.traverseCount = 0;
      wi.segObj = 0;
      wi.offset = inv->entry.w1;
      wi.frameBits = EROS_NODE_LGSIZE;
      wi.writeAccess = BOOL(inv->entry.code == OC_Node_Extended_Swap);
      wi.wantLeafNode = true;

      segwalk_init(&wi, inv->key);

      /* Begin the traversal... */

      result = WalkSeg(&wi, EROS_PAGE_BLSS, 0, 0);

      /* If this is a write operation, we need to mark the node dirty. */
      if (wi.writeAccess)
	node_MakeDirty(theNode);

      COMMIT_POINT();

      if ( result == false ) {
	fatal("Node tree traversal fails without keeper.!\n");
        // Invoke segment keeper (do we really want this?)
        proc_InvokeSegmentKeeper(act_CurContext(), &wi,
                      false /* no proc keeper */,
                      inv->entry.w1 /* orignal vaddr */ );

	inv->exit.code = RC_eros_key_NoAccess;
	inv->exit.w1 = wi.faultCode;
	inv->exit.w2 = inv->entry.w1;	// original "address"
	return;
      }

      assert(wi.segObj);
      assert(wi.segObj->obType <= ot_NtLAST_NODE_TYPE);

      slot = inv->entry.w1 & EROS_NODE_SLOT_MASK;

      theNode = (Node *) wi.segObj;

      COMMIT_POINT();

      if (inv->entry.code == OC_Node_Extended_Swap) {
        Key k;			/* temporary in case send and receive */
				/* slots are the same. */

        keyBits_InitToVoid(&k);
	assert(wi.canWrite);

	key_NH_Set(&k, &theNode->slot[slot]);

	key_NH_Set(node_GetKeyAtSlot(theNode, slot), inv->entry.key[0]);
	inv_SetExitKey(inv, 0, &k);        

	/* Unchain, but do not unprepare -- the objects do not have
	 * on-disk keys. 
	 */
	key_NH_Unchain(&k);

	/* There is no need to check this for sense key status, as the
	 * segment walker would have generated an exception if the
	 * tree was not writable, and any writable tree is also
	 * readable in full power form. */
      }
      else {

	inv_SetExitKey(inv, 0, &theNode->slot[slot]);


	if (wi.canFullFetch == false)
	  Desensitize(inv->exit.pKey[0]);
      }

      inv->exit.code = RC_OK;
      return;
    }

  case OC_Node_MakeNodeKey:
  case OC_Node_MakeSegmentKey:
  case OC_Node_MakeWrapperKey:
    {
      uint32_t w;
      uint8_t p;
      COMMIT_POINT();

      w = inv->entry.w1; /* the BLSS */
      p = inv->entry.w2; /* the permissions */

      if ( (w > MAX_BLSS) ) {
	inv->exit.code = RC_eros_key_RequestError;
	dprintf(true, "Value 0x%08x is out of range\n", w);
	return;
      }

      p |= inv->key->keyPerms;
	
      inv->exit.code = RC_OK;
      

      inv_SetExitKey(inv, 0, inv->key);

      if (inv->exit.pKey[0]) {
	inv->exit.pKey[0]->keyData = w;
	inv->exit.pKey[0]->keyPerms = p;

	switch(inv->entry.code) {
	case OC_Node_MakeNodeKey:
	  break;
	case OC_Node_MakeSegmentKey:
	  keyBits_SetType(inv->exit.pKey[0], KKT_Segment);
	  break;
	case OC_Node_MakeWrapperKey:
	  keyBits_SetType(inv->exit.pKey[0], KKT_Wrapper);
	  break;
	}
      }
	
      return;
    }
    
  case OC_Node_CompareKey:
    {

      key_Prepare(inv->entry.key[0]);


      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = 0;		/* until proven otherwise */

      switch (keyBits_GetType(inv->entry.key[0])) {
      case KKT_Node:
      case KKT_Wrapper:
      case KKT_Segment:
	/* It's possible - check further below */
	break;
      default:
	return;
      }


      if (key_GetKeyOid(inv->entry.key[0]) != key_GetKeyOid(inv->key))
	return;
      if (key_GetAllocCount(inv->entry.key[0]) !=
	  key_GetAllocCount(inv->key)) 
	return;


	
      inv->exit.w1 = 1;
      return;
    }

  case OC_Node_Clear:
    {
      if (isFetch) {
	inv->exit.code = RC_eros_key_NoAccess;
	return;
      }

      node_MakeDirty(theNode);

      /* If we zero it, we're going to nail all of it's dependencies
       * anyway:
       */
      
      COMMIT_POINT();


      node_ClearThisNode(theNode);

      


      act_Prepare(act_Current());

      
      return;
    }

  case OC_Node_LssAndPerms:
    {
      COMMIT_POINT();

      inv->exit.code = RC_OK;
      inv->exit.w1 = inv->key->keyData;
      inv->exit.w2 = inv->key->keyPerms;
      return;
    }

  case OC_Node_WriteNumber:
    {
      uint32_t slot;
      eros_Number_value nkv;

      if (isFetch) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_NoAccess;
	return;
      }

      slot = inv->entry.w1;

      if (slot >= EROS_NODE_SIZE || inv->entry.len != sizeof(nkv)) {
	COMMIT_POINT();
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

#ifndef OPTION_PURE_ENTRY_STRINGS
      proc_SetupEntryString(act_CurContext(), inv);
#endif

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

  case OC_Node_Clone:
    {
      /* copy content of node key in arg0 to current node */
      bool isWeak;
      Node *fromNode;
      unsigned slot;

      if (isFetch) {
	inv->exit.code = RC_eros_key_NoAccess;
	return;
      }


      /* Mark the object dirty. */
      node_MakeDirty(theNode);


      key_Prepare(inv->entry.key[0]);

      assert(keyBits_IsPrepared(inv->key));

      COMMIT_POINT();

      if (keyBits_GetType(inv->entry.key[0]) != KKT_Node) {
	inv->exit.code = RC_eros_key_RequestError;
	return;
      }

      isWeak = keyBits_IsWeak(inv->entry.key[0]);
		     

      fromNode = (Node *) key_GetObjectPtr(inv->entry.key[0]);


      for (slot = 0; slot < EROS_NODE_SIZE; slot++) {

	node_ClearHazard(theNode, slot);
	if (keyBits_IsRdHazard(&fromNode->slot[slot]))
	  node_ClearHazard(fromNode, slot);

      }

      /* The following will do the right thing if the two nodes are
       * identical, because the NH_Set code checks for this case.
       */
      for (slot = 0; slot < EROS_NODE_SIZE; slot++) {
	/* hazards cleared above */

	key_NH_Set( &theNode->slot[slot], node_GetKeyAtSlot(fromNode, slot) );

	if (isWeak)
	  Desensitize(&theNode->slot[slot]);
      }
						    
      return;
    }
#if 0
  /* Removed because keeping the reserves straight is a pain in the ass. */
  case OC_Node_WriteNumbers:
    {
      if (isFetch) {
	inv.exit.code = RC_eros_key_NoAccess;
	return;
      }

      inv.exit.code = RC_eros_key_RequestError;

      struct inMsg {
	Word start;
	Word end;
	Word numBuf[EROS_NODE_SIZEx][3];	/* number key data fields */
      } req;

      /* If we overwrite it, we're going to nail all of it's
       * dependencies anyway:
       */
      
      theNode->Unprepare(true);

      theNode->MakeObjectDirty();

#ifndef OPTION_PURE_ENTRY_STRINGS
      Thread::CurContext()->SetupEntryString(inv);
#endif

      COMMIT_POINT();

      bzero(&req, sizeof(req));
      inv.CopyIn(sizeof(req), &req);

      if (req.start >= EROS_NODE_SIZEx ||
	  req.end >= EROS_NODE_SIZEx ||
	  req.start > req.end)
	return;

      for (uint32_t k = req.start; k < req.end; k++) {
	(*theNode)[k].KS_SetNumberKey(req.numBuf[(k-req.start)][2],
                                      req.numBuf[(k-req.start)][1],
                                      req.numBuf[(k-req.start)][0] );
      }

      return;
    }
#endif

  default:
    COMMIT_POINT();

    inv->exit.code = RC_eros_key_UnknownRequest;
    return;
  }
}

