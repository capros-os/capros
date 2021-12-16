/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

/* VCSK -- Virtual copy space keeper/Zero space keeper (they are
   actually the same!)

   When a fault occurs on a frozen VCS, the fault is simply punted to
   the domain keeper (by returning RC_capros_key_RequestError, which
   causes a process fault).

   When a fault occurs on a non-frozen VCS, the action taken depends
   on the fault type:

      Read Fault   --  space is expanded appropriately using GPT
                       capabilities into the primordial zero space.

      Write fault  --  all immutable GPTs on the path from root to
                       offset are replaced with mutable GPTs until
		       an immutable page is found.  A new page is
		       acquired from a space bank and the content of
		       the old page is copied into the new page.

The original segment to be copied must be an "original key".
An "original key" must be either
1. a read-only page key, or
2. A sensory (weak, nocall, and RO restrictions) non-opaque key to a GPT
   whose slots are all either
  (a) original keys with BLSS one smaller than this key (no levels skipped),
   or (b) void.

A key to a VCS is an opaque key to a GPT.
(The key may have other restrictions such as RO.)
The GPT has a keeper that is a start key to a process (one process per VCS).
The keyInfo of the start key is 0 if the segment is not frozen,
nonzero if it is frozen (not implemented yet). 

Slot 0 of the top-level GPT has a key to a subtree. No other slots are used.

A subtree key is either a page key or a non-opaque GPT key. 
Void keys may also be present?
A page key is either original,
  or RW having been copied from the original segment or the zero segment. 
A GPT key is either original,
  or non-sensory (no restrictions) having been copied
    from the original segment or the zero segment.

What about extending with zero pages?

The only l2v values used are those produced by the procedure BlssToL2v.
L2v goes down by EROS_NODE_LGSIZE at each level - no levels are skipped.

   You might think that this ought to be built using background
   windows -- I certainly did.  Unfortunately, the background window
   approach has an undesirable consequence -- the translation of pages
   that have NOT been copied requires traversing the entire background
   segment, a traversal that is a function of the tree depth.

   Norm points out that it is necessary to have a distinct VCSK process for
   each active kept segment, to avoid the situation in which a single
   VCSK might become inoperative due to blocking on a dismounted page.


   VCSK doesn't quite start up as you might expect.  Generally, it is
   the product of a factory, and returns a segment key as its
   "yield"  The special case is that the zero segment keeper (which is
   the very first, primordial keeper) does not do this, but
   initializes directly into "frozen" mode.  It recognizes this case
   by the fact that a pre-known constituent slot holds something other
   than void.
   (The above appears to be wrong.)
   */

#include <stddef.h>
#include <stdbool.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Page.h>
#include <idl/capros/GPT.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/VCS.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/ProtoSpaceDS.h>

#include "constituents.h"

#define L1_OPTIMIZATION

#define dbg_init    0x1
#define dbg_invalid 0x2
#define dbg_access  0x4
#define dbg_op      0x8
#define dbg_destroy 0x10
#define dbg_returns 0x20
#define dbg_alloc   0x40

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_OSTREAM  KR_APP(0)
#define KR_SEGMENT  KR_APP(1)	// a non-opaque key to the segment
#define KR_ZINDEX   KR_APP(2)	/* index of primordial zero space */ 
#ifdef L1_OPTIMIZATION
#define KR_L1_NODE  KR_APP(3)		/* last L1 node we COW'd a page into */
#endif
#define EndFixedKRs KR_APP(4)

/* Key registers EndFixedKRs and higher are overlaid.
 * Within DestroySegment(), KR_Stack through (KR_Stack+maxBlss-1) are used. */
#define KR_Stack    EndFixedKRs
/* Elsewhere, KR_SCRATCH, KR_SCRATCH2,
 * and KR_PG_CACHE through (KR_PG_CACHE+2) are used. */
#define KR_SCRATCH  EndFixedKRs
#define KR_SCRATCH2 (EndFixedKRs+1)
#define KR_PG_CACHE (EndFixedKRs+2)


/* POLICY NOTE:

   The original design was intended to grow by one page when
   zero-extending the segment.  Newer operating systems frequently
   grow by more than this.  The following tunable may be set to
   anything in the range 1..EROS_NODE_SIZE to control how many pages
   the segment grows by. Note that this could (and should) be made
   into a user-controllable variable -- I just haven't bothered
   yet. */

#define ZERO_EXTEND_BY 2

/* Our start key is never invoked directly, because we never give it out.

   If this was a pass-through invocation via a red space key, these
   hold what the user passed except that KR_SEGMENT has been replaced
   by a non-opaque key to the segment GPT.

   If this was a kernel-generated fault invocation,
   KR_SEGMENT holds a non-opaque key to the segment GPT,
   and KR_RETURN holds a fault key. */

/* IMPORTANT optimization:

   VCSK serves to implement both demand-copy and demand-zero
   segments.  Of the cycles spent invoking capabilities, it proves
   that about 45% of them are spent in capros_Page_clone, and another 45% in
   range key calls (done by the space bank).  The only call to
   capros_Page_clone is here.  It is unavoidable when we are actually doing a
   virtual copy, but very much avoidable if we are doing demand-zero
   extension on an empty or short segment -- the page we get from the
   space bank is already zeroed.

   We therefore remember in the VCSK state the offset of the *end* of
   the last copied or original page.  Anything past this is known to be zero.
   We take advantage of this to know when the capros_Page_clone() operation
   can be skipped.  The variable is copied_limit.

   When a VCS is frozen, we stash this number in a number key in the
   red segment node, and reload it from there when the factory creates
   a new child node.

   At the moment I have not implemented this, because the VCSK code is
   also trying to be useful in providing demand extension of existing
   segments. There are a couple of initialization cases to worry about
   -- all straightforward -- but I haven't dealt with those yet. None
   of these are issues in the performance path.  */

typedef struct {
  bool was_access;
  uint64_t last_offset;		/* last offset we modified */

	/* No pages at or above the offset in copied_limit are either
	copied or original. */
  uint64_t copied_limit;
  int frozen;
  uint32_t npage;
} state;

void Sepuku(result_t);

// maxBlss is enough to represent a 64-bit space.
#define maxBlss ((64 - EROS_PAGE_ADDR_BITS \
                  + (EROS_NODE_LGSIZE-1) /* to round up*/) / EROS_NODE_LGSIZE)

static unsigned int
L2vToBlss(unsigned int l2v)
{
  return (l2v - EROS_PAGE_ADDR_BITS) / EROS_NODE_LGSIZE + 1;
}

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
}

uint32_t
BiasedLSS(uint64_t offset)
{
  /* Shouldn't this be using fcs()? */
  uint32_t bits = 0;
  uint32_t w0 = (uint32_t) offset;
  
  static unsigned hexbits[16] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };

  /* Run a decision tree: */
  if (offset >= 0x100000000ull) {
    bits += 32;
    w0 = (offset >> 32);
  }
	
  if (w0 >= 0x10000ull) {
    bits += 16;
    w0 >>= 16;
  }
  if (w0 >= 0x100u) {
    bits += 8;
    w0 >>= 8;
  }
  if (w0 >= 0x10u) {
    bits += 4;
    w0 >>= 4;
  }

  /* Table lookup for the last part: */
  bits += hexbits[w0];
  
  if (bits < EROS_PAGE_ADDR_BITS)
    return 0;

  bits -= EROS_PAGE_ADDR_BITS;

  bits += (EROS_NODE_LGSIZE - 1);
  bits /= EROS_NODE_LGSIZE;
  
  return bits;
}
  
uint64_t
BlssMask(uint32_t blss)
{
  if (blss == 0)
    return 0ull;

  uint32_t bits_to_shift = BlssToL2v(blss);

  if (bits_to_shift >= UINT64_BITS)
    return -1ull;	/* all 1's */
  else return (1ull << bits_to_shift) -1ull;
}

uint32_t
BlssSlotNdx(uint64_t offset, uint32_t blss)
{
  if (blss == 0)
    return 0;

  uint32_t bits_to_shift = BlssToL2v(blss);
  if (bits_to_shift >= UINT64_BITS)
    return 0;

  return offset >> bits_to_shift;
}

enum {
  type_GPT,
  type_Page,
  type_Unknown,
} GetKeyType(cap_t cap)
{
  capros_key_type kt;
  result_t result = capros_key_getType(cap, &kt);
  if (result == RC_OK) {
    if (kt == IKT_capros_GPT)
      return type_GPT;
    if (kt == IKT_capros_Page)
      return type_Page;
  }
  return type_Unknown;
}

// krMem must be a GPT.
uint8_t
GetGPTBlss(uint32_t krMem)
{
  uint8_t l2v;
  uint32_t result;

  result  = capros_GPT_getL2v(krMem, &l2v);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Error geting GPT l2v!\n");
    return 0;
  }
  return L2vToBlss(l2v);
}

uint8_t
GetBlss(cap_t krMem)
{
  if (GetKeyType(krMem) != type_GPT)
    return 0;	// assume it is a page key
  return GetGPTBlss(krMem);
}

bool
CapIsReadOnly(cap_t cap)
{
  uint32_t perms;
  result_t result = capros_Memory_getRestrictions(cap, &perms);
  return (result != RC_OK || (perms & capros_Memory_readOnly));
}

#if 1

cap_t
AllocPage(state * pState, result_t * resultp)
{
  result_t result;
  if (pState->npage == 0) {
    DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: alloc 3 pages\n",
                         __FILE__, __LINE__);
    result = capros_SpaceBank_alloc3(KR_BANK,
          capros_Range_otPage
          | ((capros_Range_otPage | (capros_Range_otPage << 8)) << 8),
			            KR_PG_CACHE + 0,
			            KR_PG_CACHE + 1,
			            KR_PG_CACHE + 2);
    if (result == RC_OK) {
      pState->npage = 3;
    } else {
      // We could try allocating 2 pages here,
      // but that does not seem like a worthwhile optimization,
      // since there is a good chance it won't succeed.
      DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: alloc 1 page\n",
                           __FILE__, __LINE__);
      result = capros_SpaceBank_alloc1(KR_BANK,
                                       capros_Range_otPage,
  			               KR_PG_CACHE + 0 );
      if (result == RC_OK) {
        pState->npage = 1;
      } else {
        DEBUG(returns) kprintf(KR_OSTREAM, "AllocPage failed\n");
        *resultp = result;
        return KR_VOID;
      }
    }
  }
  DEBUG(alloc) kdprintf(KR_OSTREAM, "%s:%d: get page from cache\n",
                       __FILE__, __LINE__);
  return KR_PG_CACHE + --pState->npage;
}

// Free any extra pages we allocated.
void
ClearPageCache(state * pState)
{
  DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: free %d\n",
                       __FILE__, __LINE__, pState->npage);
  switch (pState->npage) {
  default:
    assert(false);
  case 2:
    capros_SpaceBank_free2(KR_BANK, KR_PG_CACHE + 0, KR_PG_CACHE + 1);
    break;
  case 1:
    capros_SpaceBank_free1(KR_BANK, KR_PG_CACHE + 0);
    break;
  case 0:
    break;
  }
}

#else

cap_t
AllocPage(state * pState, result_t * resultp)
{
  result_t result;
  DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: alloc 1 page\n",
                       __FILE__, __LINE__);
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage, KR_TEMP0);
  if (result == RC_OK)
    return KR_TEMP0;
  else {
    DEBUG(returns) kprintf(KR_OSTREAM, "AllocPage failed\n");
    *resultp = result;
    return KR_VOID;
  }
}

void
ClearPageCache(state * pState)
{
}

#endif

void
InsertNewPage(state * pState, uint64_t orig_offset)
{
  pState->was_access = true;
  pState->last_offset = orig_offset;
  
  /* We assume at this point that the page in question no longer
     contains zeros, so the first zero offset must be above this
     point. */
  if (orig_offset >= pState->copied_limit)
    pState->copied_limit = orig_offset + EROS_PAGE_SIZE;
}

uint32_t
HandleSegmentFault(Message *pMsg, state *pState)
{
  uint32_t slot = 0;
  uint64_t offset = ((uint64_t) pMsg->rcv_w3) << 32
                    | (uint64_t) (pMsg->rcv_w2 & - EROS_PAGE_SIZE);
  uint32_t offsetBlss = BiasedLSS(offset);

  switch (pMsg->rcv_w1) {
  case capros_Process_FC_InvalidAddr:
    {
      /* capros_Process_FC_InvalidAddr does not copy any pages.
	 It therefore cannot alter the value of copied_limit.  */
      
      result_t result;
      
      pState->was_access = false;
      
      DEBUG(invalid)
	kprintf(KR_OSTREAM, "FC_SegInvalidAddr at %#llx\n", offset);
      
      /* fetch out the subseg key */
      capros_GPT_getSlot(KR_SEGMENT, 0, KR_SCRATCH);

      /* find out its BLSS: */
      uint8_t subsegBlss = GetBlss(KR_SCRATCH);
      
      DEBUG(invalid) kprintf(KR_OSTREAM, "FC_SegInvalidAddr: subsegBlss %d offsetBlss %d\n", subsegBlss, offsetBlss);

      if (subsegBlss < offsetBlss) {
        // Need to make the tree taller.
	while (subsegBlss < offsetBlss) {
	  DEBUG(invalid) kprintf(KR_OSTREAM, "  Growing: subsegblss %d offsetblss %d\n",
				  subsegBlss, offsetBlss);
      
	  /* Buy a new GPT to expand with: */
          DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: alloc 1 GPT\n",
                               __FILE__, __LINE__);
          result = capros_SpaceBank_alloc1(KR_BANK,
                                           capros_Range_otGPT, KR_TEMP0);
	  if (result != RC_OK)
	    return result;
      
	  /* Make that node have BLSS == subsegBlss+1: */
          capros_GPT_setL2v(KR_TEMP0, BlssToL2v(subsegBlss+1));

	  /* Insert the old subseg into slot 0: */
	  capros_GPT_setSlot(KR_TEMP0, 0, KR_SCRATCH);

#if 1
	  /* Populate slots 1 and higher with suitable primordial zero
	     subsegments. */
          /* This isn't necessary; leaving the slots void will result in
          faulting them to zero as they are referenced.
          However, it's probably faster to populate them now. */
          capros_Node_getSlot(KR_ZINDEX, subsegBlss, KR_SCRATCH);
	  {	 
	    int i;
	    for (i = 1; i < EROS_NODE_SIZE; i++)
	      capros_GPT_setSlot(KR_TEMP0, i, KR_SCRATCH);
	  }
#endif

	  COPY_KEYREG(KR_TEMP0, KR_SCRATCH);

	  subsegBlss++;
	}

	/* Finally, insert the new subseg into the original GPT */
	capros_GPT_setSlot(KR_SEGMENT, 0, KR_SCRATCH);

        /* Note: there is a window between setting slot 0 of the
           original GPT, and setting its blss. That's not a problem;
           you just won't be able to access the taller portion until
           the blss is set. (The taller portion is void anyway.) */

        capros_GPT_setL2v(KR_SEGMENT, BlssToL2v(subsegBlss + 1));

        /* This might have been only a read reference, in which case
        populating with primordial zeroes was enough. 
        If it is a write (which is likely, actually),
        the client will fault again with capros_Process_FC_AccessViolation.
        We mustn't fall through to the code below, because it expects
        to find an invalid key. */
        DEBUG(returns)
          kdprintf(KR_OSTREAM, 
                   "Returning from capros_Process_FC_InvalidAddr after growing"
                   " managed segment to blss=%d\n", subsegBlss);
        return RC_OK;
      }

      /* Segment is now tall enough, but some portion was unpopulated. */

      DEBUG(invalid) kprintf(KR_OSTREAM,
        "Invalid address -- falling through to COW logic for znode\n"
        "  subsegBlss %d, offset %#llx\n",
			     subsegBlss, offset);

      /* Traverse downward to the invalid key.  No need to check
	 writability.  Had that been the problem we would have gotten
	 an access violation instead.

	 This logic will work fine as long as the whole segment tree
	 is non-opaque GPTs and pages, but will fail miserably if
         an opaque subsegment is plugged in here somewhere. */

      cap_t krParent = KR_SEGMENT;
      cap_t krChild = KR_SCRATCH;
      // krParent[slot] == krChild
      while (1) {
        int type = GetKeyType(krChild);
        DEBUG(invalid)
          kprintf(KR_OSTREAM,
                  "  child type %d, slot %d, par %d, child %d, subsegBlss %d, offset %#llx\n",
                  type, slot, krParent, krChild, subsegBlss, offset);

        switch (type) {
        case type_Page:
          // Since we got an Invalid fault, we shouldn't succeed in walking
          // the tree all the way to a page.
          assert(false);

        case type_GPT:
          assert(GetGPTBlss(krChild) == subsegBlss);
          // Child is the new parent.
          krParent = krChild;
          // Key register for the new Child:
          krChild ^= KR_SCRATCH ^ KR_SCRATCH2;

          slot = BlssSlotNdx(offset, subsegBlss);
          offset &= BlssMask(subsegBlss);

          subsegBlss--;
          capros_GPT_getSlot(krParent, slot, krChild);
          continue;

        case type_Unknown:	// should be Void
          break;
        }
        break;
      }

      // Key in krChild is the invalid key.
      // krParent[slot] == krChild

      /* Replace the offending subsegment with a primordial zero segment of
	   suitable size: */
      capros_Node_getSlot(KR_ZINDEX, subsegBlss, KR_TEMP0);
      capros_GPT_setSlot(krParent, slot, KR_TEMP0);

      DEBUG(returns)
	kprintf(KR_OSTREAM,
		 "Returning from capros_Process_FC_InvalidAddr after populating"
		 " zero subsegment\n");
      return RC_OK;
    }
    
  case capros_Process_FC_AccessViolation:
    {
      /* Subseg is read-only */

      uint8_t subsegBlss;
      result_t result;
      uint64_t orig_offset = offset;
      bool needTraverse = true;

      DEBUG(access)
	kprintf(KR_OSTREAM, "capros_Process_FC_AccessViolation at 0x%llx\n",
		 offset);
      
      cap_t krParent = KR_SEGMENT;
      cap_t krChild = KR_SCRATCH;
#ifdef L1_OPTIMIZATION
      /* If the last thing we got hit with was an access fault, and
	 the present fault would land within the same L1 node, there
	 is no need to re-execute the traversal; simply reuse the
	 result from the last traversal:

	 It is fairly obvious that this is safe if the access pattern
	 is of the form

	     0b=========00001xxxx
	     0b=========00002xxxx

	 It is also safe if the access pattern was:
	 
	     ???

	 Because the second fault will result in an invalid address
	 before generating the access violation, which will cause the
	 segment to grow and invalidate the walk cache.

	 The tricky one is if the pattern is the other way:

	     0b=========00001xxxx
	     0b=========00000xxxx

         In this case, we will have done tree expansion in the first
	 path and have cached the right information. */
      
      if (pState->was_access) {
	uint64_t xoff = offset ^ pState->last_offset;
	xoff >>= (EROS_PAGE_ADDR_BITS + EROS_NODE_LGSIZE);
	if (xoff == 0) {
	  needTraverse = false;
	  slot = (offset >> EROS_PAGE_ADDR_BITS) & EROS_NODE_SLOT_MASK;
	  //subsegBlss = 2;
          krParent = KR_L1_NODE;
	  capros_GPT_getSlot(krParent, slot, krChild);

	  DEBUG(access)
	    kprintf(KR_OSTREAM, "Re-traverse suppressed at slot %d!\n", slot);
	}
      }
#endif
      
      if (needTraverse) {
	pState->was_access = false; /* we COULD fail! */
	
	/* fetch out the subseg key */
	result = capros_GPT_getSlot(krParent, slot, krChild);
        assert(result == RC_OK);

	/* find out its BLSS: */
        subsegBlss = GetBlss(krChild);
      
	DEBUG(access) kprintf(KR_OSTREAM,
                        "  traverse slot %d, subsegBlss %d, offset 0x%llx\n",
			slot, subsegBlss, offset);

	while (subsegBlss > 0) {
	  DEBUG(access) kprintf(KR_OSTREAM, "  Walking down: subsegBlss %d\n",
				 subsegBlss);

          assert(GetKeyType(krChild) == type_GPT);
          if (CapIsReadOnly(krChild)) {
	    DEBUG(access) kprintf(KR_OSTREAM, "  subsegBlss %d unwritable.\n",
				   subsegBlss);
	    break;
	  }

          // Traverse downward past this writable GPT.
          // Child is the new parent.
          krParent = krChild;
          // Key register for the new Child:
          krChild ^= KR_SCRATCH ^ KR_SCRATCH2;
	  slot = BlssSlotNdx(offset, subsegBlss);
	  offset &= BlssMask(subsegBlss);

	  DEBUG(access) kprintf(KR_OSTREAM,
                          "  traverse slot %d, blss %d, offset 0x%llx\n",
			  slot, subsegBlss, offset);

	  subsegBlss--;
	  capros_GPT_getSlot(krParent, slot, krChild);
	}
        // krParent[slot] == krChild

        assert(subsegBlss > 0
               || (GetKeyType(krChild) == type_Page
                   && CapIsReadOnly(krChild) ) );
      
	/* Have now hit a read-only subtree, which we need to traverse,
	   turning the R/O GPTs into R/W GPTs. */

	while (subsegBlss > 0) {
	  DEBUG(access) kprintf(KR_OSTREAM,
                          "  Walking down: COW subsegBlss %d\n", subsegBlss);

	  /* Buy a new read-write GPT: */
          DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: alloc 1 GPT\n",
                               __FILE__, __LINE__);
          result = capros_SpaceBank_alloc1(KR_BANK,
                                           capros_Range_otGPT, KR_TEMP0);
	  if (result != RC_OK)
	    return result;
      
	  /* Copy the slots and l2v from the old subseg into the new: */
	  result = capros_GPT_clone(KR_TEMP0, krChild);
          assert(result == RC_OK);

	  /* Replace the old subseg with the new */
	  result = capros_GPT_setSlot(krParent, slot, KR_TEMP0);
          assert(result == RC_OK);
          COPY_KEYREG(KR_TEMP0, krChild);	// the new child

	  /* Now traverse downward in the tree: */
          // Child is the new parent.
          krParent = krChild;
          // Key register for the new Child:
          krChild ^= KR_SCRATCH ^ KR_SCRATCH2;

	  slot = BlssSlotNdx(offset, subsegBlss);
	  offset &= BlssMask(subsegBlss);

	  DEBUG(access) kprintf(KR_OSTREAM,
                          "  traverse slot %d, blss %d, offset 0x%llx\n",
			  slot, subsegBlss, offset);

	  subsegBlss--;
	  result = capros_GPT_getSlot(krParent, slot, krChild);
          assert(result == RC_OK);
	}

#ifdef L1_OPTIMIZATION
	/* krParent now points to the parent of the page key we are
	   COWing. If we are doing the L1 node memorization, record the
	   last manipulated L1 node here. */
	COPY_KEYREG(krParent, KR_L1_NODE);
#endif
      }
      // krParent[slot] == krChild
      // krChild is at the page level.

      DEBUG(access) kprintf(KR_OSTREAM, "  Walking down: COW"
			     " orig_offset %#llx copied limit %#llx\n",
			     orig_offset, pState->copied_limit);
      
      if (orig_offset >= pState->copied_limit) {
	uint32_t count;
	
	/* Insert new zero pages at this location. As an optimization,
	   insert ZERO_EXTEND_BY pages at a time. */
	for (count = 0;
             (slot < EROS_NODE_SIZE) && (count < ZERO_EXTEND_BY);
             count++, slot++, orig_offset += EROS_PAGE_SIZE) {
	  cap_t kr = AllocPage(pState, &result);

	  if (kr == KR_VOID) {	// couldn't get a page
	    if (count)
	      break;		// abandon inserting extra pages
	    else
	      return result;	// failed to get any pages
	  }

	  result = capros_GPT_setSlot(krParent, slot, kr);
          assert(result == RC_OK);
          InsertNewPage(pState, orig_offset);
	}
	DEBUG(returns)
	  kdprintf(KR_OSTREAM,
		   "Returning from capros_Process_FC_AccessViolation\n"
                   " after appending %d empty pages to %#llx\n",
                   count, orig_offset);
      } else {
	cap_t kr = AllocPage(pState, &result);
	if (kr == KR_VOID)
	  return result;

	result = capros_Page_clone(kr, krChild);
        assert(result == RC_OK);

	/* Replace the old page with the new */
	result = capros_GPT_setSlot(krParent, slot, kr);
        assert(result == RC_OK);
	InsertNewPage(pState, orig_offset);

	DEBUG(returns)
	  kdprintf(KR_OSTREAM,
		   "Returning from capros_Process_FC_AccessViolation after duplicating"
		   " page at %#llx\n", orig_offset);

      }
      return RC_OK;
    }

  default:
    return pMsg->rcv_w1;	/* fault code */
  }
}

/* Recursive procedure to destroy the subsegment at kr. */
void
DestroySubseg(cap_t kr)
{
  int type = GetKeyType(kr);
  DEBUG(destroy) kprintf(KR_OSTREAM,
         "Destroying subseg in slot %d, type %d\n", kr, type);
  switch (type) {
  case type_GPT:
    if (CapIsReadOnly(kr))
      break;
    {
      int i;
      cap_t nextkr = kr+1;
      for (i = 0; i < EROS_NODE_SIZE; i++) {
	capros_GPT_getSlot(kr, i, nextkr);
        DestroySubseg(nextkr);
      }
    }
    DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: free GPT\n",
                         __FILE__, __LINE__);
    capros_SpaceBank_free1(KR_BANK, kr);
    break;

  case type_Page:
    if (CapIsReadOnly(kr))
      break;
    DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: free page\n",
                         __FILE__, __LINE__);
    capros_SpaceBank_free1(KR_BANK, kr);
  default:	// type_Unknown, should be Void
    break;
  }
}

static void
DestroySegment(state *mystate)
{
  DEBUG(destroy) kdprintf(KR_OSTREAM, "Destroying seg\n");
  capros_GPT_getSlot(KR_SEGMENT, 0, KR_Stack);
  DestroySubseg(KR_Stack);
  DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: free top GPT\n",
                       __FILE__, __LINE__);
  capros_SpaceBank_free1(KR_BANK, KR_SEGMENT);
}

static int
ProcessRequest(Message *argmsg, state *pState)
{
  uint32_t result = RC_OK;
  
  switch(argmsg->rcv_code) {
  case OC_capros_key_getType:			/* check alleged keytype */
    {
      argmsg->snd_w1 = IKT_capros_VCS;
      break;
    }

  case OC_capros_Memory_fault:
    if (pState->frozen) {
      result = RC_capros_key_RequestError;
      break;
    }
    
    result = HandleSegmentFault(argmsg, pState);
    break;

  case OC_capros_Memory_reduce:
    {
      result = capros_Memory_reduce(KR_SEGMENT,
                           capros_Memory_opaque | argmsg->rcv_w1,
                           KR_TEMP0);
      assert(result == RC_OK);
      argmsg->snd_key0 = KR_TEMP0;
      break;
    }

#if 0
  case OC_Vcsk_Truncate:
  case OC_Vcsk_Pack:
    if (pState->frozen) {
      result = RC_capros_key_RequestError;
      break;
    }
#endif
    
  case OC_capros_key_destroy:
    {
      /* Call ClearPageCache *before* DestroySegment, because we are
         reusing its key registers. */
      ClearPageCache(pState);
      DestroySegment(pState);
      Sepuku(RC_OK);
      break;
    }
    
  default:
    result = RC_capros_key_UnknownRequest;
    break;
  }
  
  argmsg->snd_code = result;
  return 1;
}

void
Sepuku(result_t retCode)
{
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_TEMP0);

  /* Invoke the protospace to destroy us and return. */
  protospace_destroy_small(KR_TEMP0, retCode);
}

int
main(void)
{
  result_t result;
  Message msg;

  state Mystate, * mystate = &Mystate;
  mystate->was_access = false;
  mystate->npage = 0;
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_ZINDEX, KR_ZINDEX);

  // Ensure there is enough room for KR_Stack:
  assert(KR_TEMP3 - KR_Stack >= maxBlss);
    
  DEBUG(init) kdprintf(KR_OSTREAM, "Buy new GPT\n");
  DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: alloc top GPT\n",
                       __FILE__, __LINE__);
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_SEGMENT);
  if (result != RC_OK)	// failed to allocate a GPT
    Sepuku(result);	// return failure to caller

  capros_Node_getSlot(KR_CONSTIT, KC_FROZEN_SEG, KR_TEMP0);

  /* write the immutable seg to slot 0 */
  result = capros_GPT_setSlot(KR_SEGMENT, 0, KR_TEMP0);

  DEBUG(init) kdprintf(KR_OSTREAM, "Fetch BLSS of frozen seg\n");
  /* find out BLSS of frozen segment: */
  /* FIXME: need to validate KR_TEMP0 - it must be an "original key". */
  uint8_t segBlss = GetBlss(KR_TEMP0);

  DEBUG(init) kdprintf(KR_OSTREAM, "BLSS of frozen seg was %d\n", segBlss);

  segBlss += 1;
  
  mystate->copied_limit = 1ull << BlssToL2v(segBlss);

  DEBUG(init) kdprintf(KR_OSTREAM, "Initialize it\n");

  result = capros_GPT_setL2v(KR_SEGMENT, BlssToL2v(segBlss));
  assert(result == RC_OK);

  /* Make a start key to ourself and set as keeper. */
  capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  result = capros_GPT_setKeeper(KR_SEGMENT, KR_TEMP0);

  result = capros_Memory_reduce(KR_SEGMENT, capros_Memory_opaque, KR_TEMP0);

  DEBUG(init) kdprintf(KR_OSTREAM, "Initialized VCSK\n");
    
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_TEMP0;	/* first return: seg key */
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_code = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  /* Beware: if we are called as a keeper, a non-opaque key to the GPT
     comes as key argument 0. 
     If we are called explictly, it comes as key argument 2.
     Fortunately, we don't need to receive the key, because we serve
     only one GPT. */
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    mystate->frozen = msg.rcv_keyInfo;
    msg.snd_key0 = KR_VOID;	/* until otherwise proven */
  } while ( ProcessRequest(&msg, mystate) );

  return 0;
}
