/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

/* SuperNode -- domain that looks like a very big node.  Constructs a
   tree of nodes to store keys passed by the user.

   Key Registers:

   KR15: key to root of node tree
   KR14: Temporary key slot used to walk tree
   KR13: key to space bank

   KR1:  incoming key to store/outgoing key to return

   Return Codes:
   0: OK
   1: Bounds Error
   2: Bad Request

   Incoming data always comes in as an in-register value.
   */
#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>
#include <eros/ProcessKey.h>

#include <idl/capros/key.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>

#include <domain/SuperNodeKey.h>
#include <domain/domdbg.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM	KR_APP(0)
#define KR_SCRATCH	KR_APP(1)
#define KR_TREE		KR_APP(2)
#define KR_WALK		KR_APP(3)

#define MAX_LEVEL	5

#define dbg_init    0x1
#define dbg_op      0x2
#define dbg_copy    0x4
#define dbg_swap    0x8
#define dbg_zap     0x10

/* Following should be an OR of some of the above */
#define dbg_flags   0  /* ( dbg_op|dbg_copy|dbg_swap|dbg_zap ) */

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* In principle, treeHeight ought to just be log16(lastKey).
   Unfortunately, tree expansion can fail in the middle due to lack of
   space, in which case the tree may have been grown but the key may
   never have gotten inserted.  We therefore need to track them
   separately. */

/* There is a TEMPORARY restriction on the tree height of MAX_LEVEL (3),
   limiting supernode (somewhat arbitrarily) to 16^3 keys.  This is
   due to the stupidity of the current tree delete strategy, which
   should be replaced by a chain inverting algorithm similar to that
   used in classical GC systems for CONS cells. */

typedef struct {		/* wrap globals in a structure for
				   small domains */
  uint32_t lastKey;		/* initialize to 0 */
  uint32_t treeHeight;		/* initialize to 0 */
} state;


uint32_t snode_xtract(uint32_t ndx, state *mystate, uint32_t depth);
uint32_t snode_copy(Message *msg, state *mystate);
uint32_t snode_swap(Message *msg, state *mystate);
void snode_zap_all_keys(state *mystate);
void snode_destroy(state *mystate);

void ProcessRequest(Message *msg, state *mystate);

void
Initialize(state *mystate)
{

  mystate->lastKey = 0;
  mystate->treeHeight = 0;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  COPY_KEYREG(KR_VOID, KR_TREE);

  DEBUG(init) kdprintf(KR_OSTREAM, "Supernode: initialized\n");
}

int
main()
{
  state mystate;
  Message msg;

  Initialize(&mystate);  

  process_make_start_key(KR_SELF, 0, KR_ARG(0));
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_ARG(0);
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  for(;;) {
    RETURN(&msg);
    msg.snd_key0 = KR_VOID;		 /* until otherwise proven */
    ProcessRequest(&msg, &mystate);
  }
}


uint32_t
log16(uint32_t w)
{
  uint32_t log = 0;
  while (w > 0) {
    w >>= 4;
    log++;
  }

  return log;
}

void
ProcessRequest(Message *msg, state *mystate)
{
  uint32_t result = RC_OK;

  msg->snd_len = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  
  switch (msg->rcv_code) {
  case OC_SuperNode_Copy:
    {
      DEBUG(op) kdprintf(KR_OSTREAM, "snode_copy(ndx=%d)\n", msg->rcv_w1);
      result = snode_copy(msg, mystate);
      if (result == RC_OK)
	msg->snd_key0 = KR_WALK;
      break;
    }

  case OC_SuperNode_Swap:
    {
      DEBUG(op) kdprintf(KR_OSTREAM, "snode_swap(ndx=%d)\n", msg->rcv_w1);
      result = snode_swap(msg, mystate);
      break;
    }

  case OC_SuperNode_Zero:
    {
      DEBUG(op) kdprintf(KR_OSTREAM, "snode_zero(ndx=%d)\n", msg->rcv_w1);
      snode_zap_all_keys(mystate);
      break;
    }

  /*
   * FIX: we need an order code for destroy, but
   * since it doesn't totally work yet I guess
   * we'll wait
   */

  default:
    result = RC_capros_key_UnknownRequest;
    break;
  }

  msg->snd_code = result;
}

/* Follow the path implied by ndx until you reach a tree depth of
   /depth/, and return that kay.

   Returns 0 if it terminates the walk early, which reduces
   unnecessary calls to spcbank in zap_all_keys. */
uint32_t
snode_xtract(uint32_t ndx, state *mystate, uint32_t depth)
{
  uint32_t height = mystate->treeHeight;

  /* Until proven otherwise, snd_key0 should remain KR_VOID --
   * all slots have void until we know otherwise.
   */

  /* Copy tree root key into temporary key: */
  COPY_KEYREG(KR_TREE, KR_WALK);
    
  while (height > depth) {
    uint32_t nodeNdx;
    
    nodeNdx = (ndx >> ((height-1) * 4));
	
    nodeNdx &= 0xfu;

    if (capros_Node_getSlot(KR_WALK, nodeNdx, KR_WALK) == RC_capros_key_Void) {
      /* It was a void key - just return RC_OK, since msg->snd_key0
	 still holds KR_VOID. */
      return 0;
    }
    height--;
  }

  return 1;
}

/* supernode_copy(): Return key from position described by 'ndx'
 */
uint32_t
snode_copy(Message *msg, state *mystate)
{
  if (msg->rcv_w1 > mystate->lastKey)
    return RC_capros_key_RequestError;

  (void) snode_xtract(msg->rcv_w1, mystate, 0);

  msg->snd_key0 = KR_WALK;
  return RC_OK;
}

/* supernode_swap(): Exchange argument key with key at position
   described by 'ndx'.  The tricky difference between this and
   supernode_copy() is that this has to be prepared to grow the tree
   on demand. */
uint32_t
snode_swap(Message *msg, state *mystate)
{
  uint32_t height;
  uint32_t ndxHeight = log16(msg->rcv_w1);
  uint32_t result;
    
  if (ndxHeight > MAX_LEVEL) {
    kdprintf(KR_OSTREAM, "MAX SNODE HEIGHT EXCEEDED; ndx 0x%08x\n",
	     msg->rcv_w1); 
    return RC_capros_key_RequestError;
  }
  
  /* Step 1: Check if we need to grow the tree upwards: */
  while (ndxHeight > mystate->treeHeight) {
    DEBUG(swap) kprintf(KR_OSTREAM, "NDX height is %d Height is %d\n", ndxHeight,
		  mystate->treeHeight); 

    /* Using KR_WALK as a scratch register */

    if ((result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode,
                                          KR_WALK)) != RC_OK)
      return result;

    DEBUG(swap) kdprintf(KR_OSTREAM, "Got new node\n", ndxHeight,
	     mystate->treeHeight); 

    /* set slot 0 of new node to old tree: */
    capros_Node_swapSlot(KR_WALK, 0, KR_TREE, KR_VOID);
      
    /* establish new tree root */
    COPY_KEYREG(KR_WALK, KR_TREE);

    mystate->treeHeight++;
  }

  height = mystate->treeHeight;
      
  /* Special case: if current tree Height is 0 and setting slot 0,
     blast the tree root slot: */
  if (mystate->treeHeight == 0 && msg->rcv_w1 == 0) {
    /* establish new tree root */
    COPY_KEYREG(KR_ARG(0), KR_WALK);
    COPY_KEYREG(KR_ARG(0), KR_TREE);

    msg->snd_key0 = KR_WALK;
  
    return RC_OK;
  }

  /* Step 2: Attempt to traverse the tree downwards looking for the
     right slot.  It's possible that the relevant subtree is not
     populated, in which case we should expand it as we go. The key
     held in KR_TREE can safely be assumed to be a node key if the
     treeHeight is > 0. */
    
  /* establish new tree root */
  COPY_KEYREG(KR_TREE, KR_WALK);

  while (height > 1) {
    uint32_t nodeNdx;
    
    nodeNdx = (msg->rcv_w1 >> ((height-1) * 4));
	
    nodeNdx &= 0xfu;

    capros_Node_getSlot(KR_WALK, nodeNdx, KR_SCRATCH);

    /* KR_SCRATCH may be a void key, in which case it will respond
       with RC_capros_key_Void to the following, and we must populate that
       subtree: */
      
    if (capros_Node_getSlot(KR_SCRATCH, 0, KR_VOID) == RC_capros_key_Void) {
#if 0
      uint32_t subnodeNdx = (mystate->inNdx >> ((height - 2) * 4));
#endif
	
      if ((result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode,
                                            KR_SCRATCH)) != RC_OK)
	return result;

      capros_Node_swapSlot(KR_WALK, nodeNdx, KR_SCRATCH, KR_VOID);
    }

    COPY_KEYREG(KR_SCRATCH, KR_WALK);

    height--;
  }

  capros_Node_swapSlot(KR_WALK, (msg->rcv_w1 & 0xf), KR_ARG(0), KR_WALK);
  
  msg->snd_key0 = KR_WALK;
  
  if (mystate->lastKey < msg->rcv_w1)
    mystate->lastKey = msg->rcv_w1;

  return RC_OK;
}

/* Tree destruction is a little tricky.  We know the height of the
   tree, but we have no simple way to build a stack of node pointers
   as we traverse it.

   After much batting around of rotation and layering, I finally
   decided that it just wasn't worthwhile to do anything fancy.  The
   following algorithm simply walks the tree once per entry,
   efficiently skipping unpopulated subtrees.

   */
void
snode_zap_all_keys(state *mystate)
{
  uint32_t depth;

  for (depth = 1; depth < mystate->treeHeight; depth++) {
    uint32_t lo = 0;
    uint32_t hi = mystate->lastKey;
    uint32_t increment = 1 << (depth * 16);

    for (lo = 0; lo < hi; lo += increment) {
      /* We could check if leaf itself is null, but supernodes tend
	 not to be sparse, and passing null key to return to bank
	 hasn't much negative impact. The test of the snode_xtract
	 return value will let us weed large sparsities. */
      if ( snode_xtract(lo, mystate, 1) )
	capros_SpaceBank_free1(KR_BANK, KR_WALK);
    }
  }

  COPY_KEYREG(KR_VOID, KR_TREE);
}
   
/* In spite of unorthodox fabrication, the constructor self-destructs
   in the usual way. */
void
Sepuku()
{
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_WALK);

  capros_SpaceBank_free1(KR_BANK, KR_CONSTIT);

  /* Invoke the protospace with arguments indicating that we should be
     demolished as a small space domain */
  protospace_destroy(KR_VOID, KR_CREATOR, KR_SELF,
		     KR_WALK, KR_BANK, 1);
}

/* supernode_destroy(): Destroy the supernode.

   FIX: This does not yet cause the space bank itself to be destroyed,
   because I do not yet understand how to make that work.
 */

void
snode_destroy(state *mystate)
{
  snode_zap_all_keys(mystate);

  Sepuku();
}
