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

/* SuperNode -- Constructs a tree of nodes to store keys passed by the user. 

A supernode key is an opaque key to the root node of the tree.
The root node has a keeper that leads to this process.
There is one process per supernode. 

Allocation and deallocation of space in the supernode is done by this process,
rather than by a library, because 
(1) the tree walk uses a lot of key registers (about 13), 
and (2) if multiple processes are sharing the supernode, 
they would have to have a mutex, which in general requires a process anyway. 

All nodes in the tree have an l2v value that is a multiple of 
capros_Node_l2nSlots (5). 
As you walk down the tree, l2v values are strictly decreasing,
as required by capros_Node_get/swapSlotExtended(). 

To better support sparse trees, the code can skip levels 
and can use nonzero guards. 

Only leaf nodes (l2v == 0) hold client keys, which can be any keys. 
If only one slot is allocated, it takes an entire leaf node to hold it. 
We don't keep a bitmap of allocated slots, so when walking the tree
we have to assume that all slots in a leaf node are allocated. 

The deallocation algorithm currently does not notice when it might be able to
skip a level by removing a node. 
Nor does it ever shorten the height of the tree. 

FIXME: Handle read-only supernodes.
*/

#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/SpaceBank.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Void.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/ProtoSpaceDS.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

#define KR_OSTREAM	KR_APP(0)
#define KR_TREE		KR_APP(2)	// the root node of the tree
/* NOTE! slots after KR_TREE are used sequentially in recursion. 
   When l2nSlots == 5 and extAddr_t is 64 bits, we need about 13 slots. */

#define dbg_init    0x1
#define dbg_alloc   0x2
#define dbg_dealloc 0x4

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0 | dbg_init | dbg_alloc | dbg_dealloc )

#define DEBUG(x) if (dbg_##x & dbg_flags)

typedef capros_Node_extAddr_t extAddr_t;
const unsigned int l2nSlots = capros_Node_l2nSlots;

/* We only use nodes with an l2v value that is a multiple of 
   capros_Node_l2nSlots. 
   The term "l2v5" means (l2v/capros_Node_l2nSlots). */
#define l2v5Shift(l2v5) ((l2v5) * l2nSlots)

/* All global state is in a structure on the stack, 
so the stack is the only private page we need. */

typedef struct {
  extAddr_t lowestAllocated;
  extAddr_t lastSlotInSupernode;	// a function of topL2v5
  uint32_t topL2v5;	// l2v5 of top-level node
  Message msg;
} GlobalState;

/* Calculate the smallest l2v5 such that
(x >> l2v5Shift(l2v5)) < capros_Node_nSlots. 

Some examples:

CalcL2v5(0x00) == 0
CalcL2v5(0x01) == 0
CalcL2v5(0x1f) == 0
CalcL2v5(0x20) == 1
*/
unsigned int
CalcL2v5(extAddr_t x)
{
  unsigned int r;
    
#if 0 // #if(sizeof(extAddr_t)*8 > l2v5Shift(8))
  if (!(x & ((extAddr_t)0 - ((extAddr_t)1 << l2v5Shift(8))))) {
    r = 7;
  }
  else {
    x >>= l2v5Shift(8);
    r = 15;
  }
#else
  r = 7;
#endif
 
  if (!(x & ((extAddr_t)0 - ((extAddr_t)1 << l2v5Shift(4))))) {
    r -= 4;
  }
  else {
    x >>= l2v5Shift(4);
  }

  assert(l2nSlots <= 5);
  // Therefore x is now < 2**l2v5Shift(4) <= 2**32
 
  if (!(x & ((uint32_t)0 - ((uint32_t)1 << l2v5Shift(2))))) {
    r -= 2;
  }
  else {
    x >>= l2v5Shift(2);
  }
 
  if (!(x & ((uint32_t)0 - ((uint32_t)1 << l2v5Shift(1))))) {
    r -= 1;
  }

  return r;
}

result_t
ensureHeight(GlobalState * mystate, extAddr_t last)
{
  unsigned int i;
  result_t result;
  unsigned int l2v5 = CalcL2v5(last);

  // Make sure we aren't trying to use the keeper slot:
  if ((last >> l2v5Shift(l2v5)) >= capros_Node_keeperSlot)
    l2v5++;	// need a bigger node to avoid the keeper slot

  const unsigned int shift = l2v5Shift(l2v5);

  DEBUG(init) kprintf(KR_OSTREAM,
                "Height: last=0x%x, now=%d, need=%d, lowalloc=0x%x\n",
                last, mystate->topL2v5, l2v5, mystate->lowestAllocated);

  if (mystate->topL2v5 < l2v5) {	// need to grow
    if (mystate->topL2v5 == 0
        && mystate->lowestAllocated >= capros_Node_nSlots) {
      /* We are growing from the original level 0 node, and there is
      nothing allocated in it (nothing allocated in the whole supernode).
      No need to keep anything in that node. */

      /* Since no slots are allocated, the client shouldn't be doing
      get/swapSlotExtended, so there's no need to block the node. */

      result = capros_Node_setL2v(KR_TREE, shift);
      assert(result == RC_OK);

      /* Clear the slots, just in case the client put keys there
      despite no slots being allocated. */
      for (i = 0; i < capros_Node_keeperSlot; i++) {
        result = capros_Node_swapSlot(KR_TREE, i, KR_VOID, KR_VOID);
        assert(result == RC_OK);
      }
    } else {
      /* Create a new node. */
      result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, KR_TEMP0);
      if (result != RC_OK)
        return result;

      // No guard.
      result = capros_Node_makeGuarded(KR_TEMP0, 0, KR_TEMP0);
      assert(result == RC_OK);

      /* Block callers while the state is inconsistent. */
      result = capros_Node_setBlocked(KR_TREE);
      assert(result == RC_OK);

      /* The client has a key to the top node, so we can't insert above there.
      Copy the root to the new node. */
      result = capros_Node_clone(KR_TEMP0, KR_TREE);
      assert(result == RC_OK);

      // We don't want the keeper:
      result = capros_Node_swapSlot(KR_TEMP0, capros_Node_keeperSlot,
                 KR_VOID, KR_VOID);
      assert(result == RC_OK);

      // Set slot 0 to point to the new node. No guard.
      result = capros_Node_swapSlot(KR_TREE, 0, KR_TEMP0, KR_VOID);
      assert(result == RC_OK);

      // Clear the rest of the slots.
      for (i = 1; i < capros_Node_keeperSlot; i++) {
        result = capros_Node_swapSlot(KR_TREE, i, KR_VOID, KR_VOID);
        assert(result == RC_OK);
      }

      result = capros_Node_setL2v(KR_TREE, shift);
      assert(result == RC_OK);

      result = capros_Node_clearBlocked(KR_TREE);
      assert(result == RC_OK);
    }
    mystate->topL2v5 = l2v5;

    // Update lastSlotInSupernode.
    extAddr_t endSlot = (extAddr_t)capros_Node_keeperSlot << shift;
    if ((endSlot >> shift) != capros_Node_keeperSlot) {
      /* The shift of keeperSlot overflowed. */
      mystate->lastSlotInSupernode = -(extAddr_t)1;
    } else {
      mystate->lastSlotInSupernode = endSlot - 1;
    }
  }
  return RC_OK;
}

/* On exit:
     If this node is known to be nonempty, 
       the return value is -1 and *lastEmptySlot is undefined. 
     Otherwise, the return value is the first slot of this node
       known to be empty, and all slots from there to *lastEmptySlot
       inclusive are known to be empty.
       If the return value is *lastEmptySlot +1, no slots are known to be empty.
 */
int
deallocateRange(GlobalState * mystate,
  cap_t thisNode, unsigned int thisL2v5,
  extAddr_t first, extAddr_t last,
  /* out */ unsigned int * lastEmptySlot)
{
  result_t result;
  unsigned int j;

  DEBUG(dealloc) kprintf(KR_OSTREAM,
                   "[%d]DeallocRange: thisL2v5=%d, first=0x%x, last=0x%x\n",
                   thisL2v5, thisL2v5, first, last);

  assert(first <= last);
  assert((last >> l2v5Shift(thisL2v5 + 1)) == 0);

  if (thisL2v5 == 0) {	// at the bottom level
    /* Level 0 nodes hold client keys. 
    Anything that we aren't deallocating is assumed to be in use. */
    if (first == 0 && last == capros_Node_nSlots - 1) {
      /* We deallocated everything in this node, so say so. */
      *lastEmptySlot = last;
      return first;
    }
    else return -1;
  }

  unsigned int thisShift = l2v5Shift(thisL2v5);

  // For each slot in this node covered by the range:
  unsigned int firstIndex = first >> thisShift;
  unsigned int lastIndex = last >> thisShift;
  bool anyNonempty = false;	// no nonempty slots so far
  for (j = firstIndex; j <= lastIndex; j++) {
    // Get the range for this slot.
    extAddr_t nextFirst = j == firstIndex ? first - (firstIndex << thisShift)
                                          : 0;
    extAddr_t nextLast = j == lastIndex ? last - (lastIndex << thisShift)
                                : ((extAddr_t)1 << thisShift) -1;
    DEBUG(dealloc) kprintf(KR_OSTREAM,
                     "[%d]j=0x%x, nextFirst=0x%x, nextLast=0x%x\n",
                     thisL2v5, j, nextFirst, nextLast);

    cap_t nextNode = thisNode + 1;	// allocate next key register
    assert(nextNode < KR_TEMP0);	/* we are recursing too deep, or
			we don't have enough key registers */

    result = capros_Node_getSlot(thisNode, j, nextNode);
    assert(result == RC_OK);

    // Given that thisL2v5 > 0, slots in this node are either node keys or void.
    unsigned char l2v;
    unsigned int nextL2v5;
    extAddr_t guard;
    result = capros_Node_getL2v(nextNode, &l2v);
    if (result != RC_OK) {	// this slot is void
      assert(result == RC_capros_key_Void);
    }
    else {		// this slot has a node key
      nextL2v5 = l2v / l2nSlots;
      assert(nextL2v5 * l2nSlots == l2v);   // it should be an even multiple

      // Calc highest addr that could be in nextNode:
      extAddr_t nextMax = ((extAddr_t)1 << l2v5Shift(nextL2v5 + 1)) - 1;

      result = capros_Node_getGuard(nextNode, &guard);
      assert(result == RC_OK);

      /* Take the guard into account. */
      extAddr_t firstRelative
        = nextFirst <= guard ? 0 : nextFirst - guard;	// ensure not negative
      if (nextLast < guard || firstRelative > nextMax ) {
        /* The entire range is outside (before or after) that of the next node,
        thus it's already deallocated.
        nextNode is presumed to be nonempty, and remains so. */
        anyNonempty = true;
      } else {
        extAddr_t lastRelative = nextLast - guard;
        lastRelative = min(lastRelative, nextMax); // don't go beyond nextNode

        DEBUG(dealloc) kprintf(KR_OSTREAM,
          "[%d]Found node, next=%d, guard=0x%x, firstRel=0x%x, lastRel=0x%x\n",
          thisL2v5, nextL2v5, guard, firstRelative, lastRelative);

        /* Recurse. */
        int nextFirstEmpty;
        unsigned int nextLastEmpty;
        nextFirstEmpty =
          deallocateRange(mystate, nextNode, nextL2v5,
                          firstRelative, lastRelative, &nextLastEmpty);

        DEBUG(dealloc) kprintf(KR_OSTREAM,
                       "[%d]Returned, firstEmpty=%d, lastEmpty=%d\n",
                       thisL2v5, nextFirstEmpty, nextLastEmpty);

        if (nextFirstEmpty < 0) {	// nextNode is nonempty
          anyNonempty = true;		// then so are we
        }
        else {	// nextNode isn't known to be nonempty. Find out.
          int i;
          bool nextNodeNonempty = false;
          /* Check slots before the first known empty slot. */
          for (i = nextFirstEmpty; --i >= 0; ) {
            if (capros_Node_getL2v(nextNode, &l2v)
                   != RC_capros_key_Void) {
              nextNodeNonempty = true;
              break;
            }
          }
          if (! nextNodeNonempty) {		// need to keep checking
            /* Check slots after the last known empty slot. */
            for (i = nextLastEmpty; ++i < capros_Node_nSlots; ) {
              if (capros_Node_getL2v(nextNode, &l2v)
                     != RC_capros_key_Void) {
                nextNodeNonempty = true;
                break;
              }
            }
          }
          // Done testing the remainder of nextNode for emptiness. 
          if (nextNodeNonempty)
            anyNonempty = true;
          else {
            /* nextNode is empty. Delete it. */
            DEBUG(dealloc) kprintf(KR_OSTREAM,
                             "[%d]Deleting nextNode.\n",
                             thisL2v5 );
            (void) capros_SpaceBank_free1(KR_BANK, nextNode);
          }
        }	// end of nextNode wasn't known to be nonempty
      }	// end of range outside of next
    }	// end of void vs. node
  }	// end of loop over slots

  if (anyNonempty)
    /* Some slot is nonempty, therefore this node is nonempty. */
    return -1;
  else {
    // All slots we visited are empty. Return that information.
    *lastEmptySlot = lastIndex;
    return firstIndex;
  }
}

result_t
allocateRange(GlobalState * mystate,
  cap_t thisNode, unsigned int thisL2v5,
  extAddr_t first, extAddr_t last)
{
  result_t result;
  unsigned int j;

  DEBUG(alloc) kprintf(KR_OSTREAM,
                 "AllocRange: thisL2v5=%d, first=0x%x, last=0x%x\n",
                 thisL2v5, first, last);

  assert(first <= last);
  assert((last >> l2v5Shift(thisL2v5 + 1)) == 0);

  if (thisL2v5 == 0)
    return RC_OK;	// at the bottom level

  unsigned int thisShift = l2v5Shift(thisL2v5);

  // For each slot in this node covered by the range:
  unsigned int firstIndex = first >> thisShift;
  unsigned int lastIndex = last >> thisShift;
  for (j = firstIndex; j <= lastIndex; j++) {
    // Get the range for this slot.
    extAddr_t slotFirst = j == firstIndex ? first - (firstIndex << thisShift)
                                          : 0;
    extAddr_t slotLast = j == lastIndex ? last - (lastIndex << thisShift)
                                : ((extAddr_t)1 << thisShift) - 1;
    DEBUG(alloc) kprintf(KR_OSTREAM, "j=0x%x, slotFirst=0x%x, slotLast=0x%x\n",
                   j, slotFirst, slotLast);

    cap_t nextNode = thisNode + 1;	// allocate next key register
    assert(nextNode < KR_TEMP0);	/* we are recursing too deep, or
			we don't have enough key registers */

    result = capros_Node_getSlot(thisNode, j, nextNode);
    assert(result == RC_OK);

    // Given that thisL2v5 > 0, slots in this node are either node keys or void.
    unsigned char nextL2v;
    unsigned int nextL2v5;
    unsigned int nextShiftPlus;
    extAddr_t guard;
    result = capros_Node_getL2v(nextNode, &nextL2v);
    if (result != RC_OK) {	// this slot is void
      assert(result == RC_capros_key_Void);

      result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, nextNode);
      if (result != RC_OK)
        return result;		// too bad

      // Calculate the smallest l2v5 that will hold this range.
      nextL2v5 = CalcL2v5(slotFirst ^ slotLast);

      DEBUG(alloc) kprintf(KR_OSTREAM,
                     "Found void, this=%d, next=%d\n",
                     thisL2v5, nextL2v5);

      while (1) {
        nextShiftPlus = l2v5Shift(nextL2v5 + 1);

        // Make a guard for the rest of the bits.
        guard = slotFirst >> nextShiftPlus << nextShiftPlus;
        result = capros_Node_makeGuarded(nextNode, guard, nextNode);
        if (result == RC_OK)
          break;	// success
        assert(result == RC_capros_Node_UnrepresentableGuard);
        // Try a larger l2v5, which will need fewer guard bits.
        nextL2v5++;
      }

      result = capros_Node_setL2v(nextNode, l2v5Shift(nextL2v5));
      assert(result == RC_OK);

      result = capros_Node_swapSlot(thisNode, j, nextNode, KR_VOID);
      assert(result == RC_OK);
    }
    else {		// this slot has a node key
      nextL2v5 = nextL2v / l2nSlots;
      assert(nextL2v5 * l2nSlots == nextL2v);	// it should be an even multiple
      nextShiftPlus = l2v5Shift(nextL2v5 + 1);

      result = capros_Node_getGuard(nextNode, &guard);
      assert(result == RC_OK);

      /* What is the smallest level that will hold both the existing node
      and our range?
      Calculate the combined range. */
      extAddr_t existingLast = guard
                               + ((extAddr_t)1 << nextShiftPlus) - 1;
      extAddr_t minFirst = min(guard, slotFirst);
      extAddr_t maxLast = max(existingLast, slotLast);
      unsigned int neededL2v5 = CalcL2v5(minFirst ^ maxLast);
      /* Should not have a problem expressing the guard,
      since this guard will have no more bits than the existing one. */

      DEBUG(alloc) kprintf(KR_OSTREAM,
                     "Found node, this=%d, needed=%d, next=%d\n",
                     thisL2v5, neededL2v5, nextL2v5);

      assert(neededL2v5 < thisL2v5);
      assert(neededL2v5 >= nextL2v5);
      if (neededL2v5 > nextL2v5) {
        // The existing tree skipped a level(s), and we must insert a node.

        result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode,
                                         KR_TEMP0);
        if (result != RC_OK)
          return result;		// too bad

        // Divide the existing guard into three parts:
        // highGuard, slot << neededShift, and (remaining) guard.
        unsigned int neededShift = l2v5Shift(neededL2v5);
        unsigned int neededShiftPlus = l2v5Shift(neededL2v5 + 1);
        extAddr_t highGuard = guard >> neededShiftPlus << neededShiftPlus;
        guard -= highGuard;
        unsigned int slot = guard >> neededShift;
        guard -= slot << neededShift;

        result = capros_Node_makeGuarded(nextNode, guard, nextNode);
        assert(result == RC_OK);

        result = capros_Node_swapSlot(KR_TEMP0, slot, nextNode, KR_VOID);
        assert(result == RC_OK);
        // Now done with nextNode.

        // Set the guard to the new node, and get the key in nextNode.
        guard = highGuard;
        result = capros_Node_makeGuarded(KR_TEMP0, guard, nextNode);
        assert(result == RC_OK);
        // Done with KR_TEMP0.

        nextL2v5 = neededL2v5;
        result = capros_Node_setL2v(nextNode, l2v5Shift(nextL2v5));
        assert(result == RC_OK);

        /* Set slot in thisNode last, because processes may be
        using the node. */
        result = capros_Node_swapSlot(thisNode, j, nextNode, KR_VOID);
        assert(result == RC_OK);
      }
    }

    // Recurse to allocate in next node.
    result = allocateRange(mystate, nextNode, nextL2v5,
               slotFirst - guard, slotLast - guard);
    if (result != RC_OK)
      return result;
  }	// end of loop over slots

  return RC_OK;
}

void
Sepuku(result_t retCode)
{
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_TEMP0);

  capros_SpaceBank_free1(KR_BANK, KR_CONSTIT);

  /* Invoke the protospace to destroy us and return. */
  protospace_destroy_small(KR_TEMP0, retCode);
  // Does not return here.
}

int
main()
{
  GlobalState gs;
  GlobalState * mystate = &gs;	// to address it consistently
  Message * msg = &mystate->msg;
  result_t result;
  int deallocResultFirst;
  unsigned int deallocResultLast;

  mystate->topL2v5 = 0;
  mystate->lastSlotInSupernode = capros_Node_keeperSlot - 1;
  mystate->lowestAllocated = capros_Node_nSlots;
    // Infinite, but this is big enough.

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, KR_TREE);
  if (result !=- RC_OK) {
    Sepuku(result);
  }

  capros_Node_setL2v(KR_TREE, 0);
  capros_Node_clearBlocked(KR_TREE);

  capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  capros_Node_setKeeper(KR_TREE, KR_TEMP0);

  DEBUG(init) kdprintf(KR_OSTREAM, "Supernode: initialized\n");

  // Return an opaque key to the root node.
  result = capros_Node_reduce(KR_TREE, capros_Node_opaque, KR_TEMP0);
  assert(result == RC_OK);
  
  msg->snd_invKey = KR_RETURN;
  msg->snd_key0 = KR_TEMP0;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  msg->snd_code = 0;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  msg->rcv_key0 = KR_ARG(0);
  msg->rcv_key1 = KR_VOID;
  msg->rcv_key2 = KR_VOID;
  msg->rcv_rsmkey = KR_RETURN;
  msg->rcv_limit = 0;

  for(;;) {
    RETURN(msg);

    // Set default return values:
    msg->snd_invKey = KR_RETURN;
    msg->snd_key0 = KR_VOID;
    msg->snd_key1 = KR_VOID;
    msg->snd_key2 = KR_VOID;
    msg->snd_rsmkey = KR_VOID;
    msg->snd_len = 0;
    msg->snd_w1 = 0;
    msg->snd_w2 = 0;
    msg->snd_w3 = 0;
    // msg->snd_code has no default.

    switch (msg->rcv_code) {
    extAddr_t first, last;

    case OC_capros_SuperNode_allocateRange:
      first = msg->rcv_w1;
      last = msg->rcv_w2;
      if (first > last) {
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }

      // Make sure the tree is tall enough. 
      result = ensureHeight(mystate, last);
      if (result != RC_OK) {
        msg->snd_code = result;
        break;
      }

      result = allocateRange(mystate, KR_TREE, mystate->topL2v5, first, last);
      if (result != RC_OK) {
        /* If we grew the tree above, we don't bother to shrink it here. */
        msg->snd_code = result;
        break;
      }

      mystate->lowestAllocated = min(mystate->lowestAllocated, first);;

      msg->snd_code = RC_OK;
      break;

    case OC_capros_SuperNode_deallocateRange:
    {
      first = msg->rcv_w1;
      last = msg->rcv_w2;
      if (first > last) {
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }

      // Check for going beyond the end of the supernode:
      if (first <= mystate->lastSlotInSupernode) {
        last = min(last, mystate->lastSlotInSupernode);

        (void) // we don't care whether the top node is empty, we're keeping it
          deallocateRange(mystate, KR_TREE, mystate->topL2v5, first, last,
                          &deallocResultLast);

        // Did we deallocate the lowest allocated slot?
        if (first <= mystate->lowestAllocated)
          mystate->lowestAllocated = max(mystate->lowestAllocated, last + 1);
      }
      else {
        /* first > mystate->lastSlotInSupernode:
         range is after the entire supernode.
         No error for deallocating already-deallocated slots. */
        DEBUG(dealloc) kdprintf(KR_OSTREAM,
                         "Deallocating from 0x%x, last is 0x%x.\n",
                         first, mystate->lastSlotInSupernode);
      }

      msg->snd_code = RC_OK;
      break;
    }

    case OC_capros_Node_getSlotExtended:
    case OC_capros_Node_swapSlotExtended:
      // We are receiving this call because the node was blocked.
      // Pass the call on to the node.
      msg->snd_invKey = KR_TREE;
      msg->snd_key0 = KR_ARG(0);
      msg->snd_key1 = KR_VOID;
      msg->snd_key2 = KR_VOID;
      msg->snd_rsmkey = KR_RETURN;
      msg->snd_len = 0;
      msg->snd_code = msg->rcv_code;
      msg->snd_w1 = msg->rcv_w1;
      msg->snd_w2 = msg->rcv_w2;
      msg->snd_w3 = msg->rcv_w3;
      break;

    case OC_capros_key_destroy:
    {
      // Deallocate all but the root.
      deallocResultFirst =
        deallocateRange(mystate, KR_TREE, mystate->topL2v5,
                        0, mystate->lastSlotInSupernode, &deallocResultLast);

      assert(deallocResultFirst == 0
             && deallocResultLast == (mystate->lastSlotInSupernode
                              >> l2v5Shift(mystate->topL2v5)) );

      // Free the root.
      (void) capros_SpaceBank_free1(KR_BANK, KR_TREE);

      Sepuku(RC_OK);
      /* NOTREACHED */
    }

    default:
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
  }
}
