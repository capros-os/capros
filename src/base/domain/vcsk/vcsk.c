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

A key to a VCS is an opaque key to a GPT.
(The key may have other restrictions such as RO.)
The GPT has a keeper that is a start key to a process (one process per VCS).
The keyInfo of the start key is 0 if the segment is not frozen,
nonzero if it is frozen (not implemented yet). 

Slot 0 of the top-level GPT has a key to a subtree. No other slots are used.

A subtree key is either a page key or a non-opaque GPT key. 
Void keys may also be present?
A page key is either RO and part of the background segment,
or RW having been copied from the background segment. 
A GPT key is either sensory (weak, nocall, and RO restrictions)
  and part of the background segment,
or non-sensory (no restrictions) having been copied from the background segment.

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
#include <eros/NodeKey.h>
#include <idl/capros/Page.h>
#include <idl/capros/GPT.h>
#include <eros/ProcessKey.h>
#include <eros/StdKeyType.h>
#include <eros/cap-instr.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/Number.h>

#include <domain/VcskKey.h>
#include <domain/domdbg.h>
#include <idl/capros/SpaceBank.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define dbg_init    0x1
#define dbg_invalid 0x2
#define dbg_access  0x4
#define dbg_op      0x8
#define dbg_destroy 0x10
#define dbg_returns 0x20

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_INITSEG  KR_APP(0)	/* used in primordia init */
#define KR_SCRATCH  KR_APP(1)
#define KR_SCRATCH2 KR_APP(2)

#define KR_ZINDEX   KR_APP(3)	/* index of primordial zero space */ 
#define KR_OSTREAM  KR_APP(4)
#define KR_L1_NODE  KR_APP(5)		/* last L1 node we COW'd a page into */
#define KR_PG_CACHE KR_APP(6)		/* really 13, 14, 15 */

#define KR_NEWOBJ  KR_ARG(0)
#define KR_SEGMENT KR_ARG(2)

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

   If this was a kernel-generated fault invocation, KR_ARG(0) and KR_ARG(1)
   hold void keys, KR_SEGMENT holds a non-opaque key to the segment GPT,
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
   the last non-zero page.  Anything past this is known to be zero.
   We take advantage of this to know when the capros_Page_clone() operation
   can be skipped.  The variable is /first_zero_offset/.

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
  uint64_t first_zero_offset;	/* first known-zero offset */
  int frozen;
  uint32_t npage;
} state;

void Sepuku();
void DestroySegment(state *);

static unsigned int
L2vToBlss(unsigned int l2v)
{
  return (l2v - EROS_PAGE_ADDR_BITS) / EROS_NODE_LGSIZE + 1 + EROS_PAGE_BLSS;
}

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1 - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
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
    return EROS_PAGE_BLSS;

  bits -= EROS_PAGE_ADDR_BITS;

  bits += (EROS_NODE_LGSIZE - 1);
  bits /= EROS_NODE_LGSIZE;
  bits += EROS_PAGE_BLSS;
  
  return bits;
}
  
uint64_t
BlssMask(uint32_t blss)
{
  if (blss <= EROS_PAGE_BLSS)
    return 0ull;

  uint32_t bits_to_shift = BlssToL2v(blss);

  if (bits_to_shift >= UINT64_BITS)
    return -1ull;	/* all 1's */
  else return (1ull << bits_to_shift) -1ull;
}

uint32_t
BlssSlotNdx(uint64_t offset, uint32_t blss)
{
  if (blss <= EROS_PAGE_BLSS)
    return 0;

  uint32_t bits_to_shift = BlssToL2v(blss);
  if (bits_to_shift >= UINT64_BITS)
    return 0;

  return offset >> bits_to_shift;
}

uint8_t
GetBlss(uint32_t krMem)
{
  uint8_t l2v;
  uint32_t result = capros_GPT_getL2v(krMem, &l2v);
  if (result == RC_capros_key_UnknownRequest)
    return EROS_PAGE_BLSS; /* it must be a page key */
  return L2vToBlss(l2v);
}
  
uint32_t
AllocPage(state *pState)
{
#if 1
  if (pState->npage == 0) {
    if (capros_SpaceBank_alloc3(KR_BANK,
          capros_Range_otPage
          | ((capros_Range_otPage | (capros_Range_otPage << 8)) << 8),
			       KR_PG_CACHE + 1,
			       KR_PG_CACHE + 2,
			       KR_PG_CACHE + 3) == RC_OK) {
      pState->npage = 3;
    }
    else if (capros_SpaceBank_alloc2(KR_BANK,
               capros_Range_otPage | (capros_Range_otPage << 8),
			       KR_PG_CACHE + 1,
			       KR_PG_CACHE + 2 ) == RC_OK) {
      pState->npage = 2;
    }
    else if (capros_SpaceBank_alloc1(KR_BANK,
                               capros_Range_otPage,
			       KR_PG_CACHE + 1 ) == RC_OK) {
      pState->npage = 1;
    }
    else
      return KR_VOID;
  }
  
  {
    uint32_t kr = KR_PG_CACHE + pState->npage;
    pState->npage--;

    return kr;
  }
#else
  if (capros_SpaceBank_alloc1(KR_BANK, capros_Range_otPage,
			     KR_NEWOBJ ) == RC_OK)
    return KR_NEWOBJ;
  else
    return KR_VOID;
#endif
}

uint32_t
HandleSegmentFault(Message *pMsg, state *pState)
{
  uint32_t slot = 0;
  uint32_t kt = RC_capros_key_Void;
  uint32_t segBlss = EROS_PAGE_BLSS + 1; /* until otherwise proven */
  uint64_t offset = ((uint64_t) pMsg->rcv_w3) << 32 | (uint64_t) pMsg->rcv_w2;
  uint32_t offsetBlss = BiasedLSS(offset);

  switch (pMsg->rcv_w1) {
  case FC_InvalidAddr:
    {
      /* Subseg is too small */
      
      /* OPTIMIZATION NOTE:

	 The effect of FC_InvalidAddr is to zero-extend the
	 segment.  It therefore cannot alter the value of
	 /first_zero_offset/.
      */
      
      uint32_t result;
      
      pState->was_access = false;
      
      DEBUG(invalid)
	kdprintf(KR_OSTREAM, "FC_SegInvalidAddr at 0x%08x %08x\n",
		 (uint32_t) (offset>>32),
		 (uint32_t) offset);
      
      /* fetch out the subseg key */
      capros_GPT_getSlot(KR_SEGMENT, slot, KR_SCRATCH);

      /* find out its BLSS: */
      uint8_t subsegBlss = GetBlss(KR_SCRATCH);

      segBlss = subsegBlss + 1;
      
      DEBUG(invalid) kdprintf(KR_OSTREAM, "FC_SegInvalidAddr: segBlss %d offsetBlss %d\n", segBlss, offsetBlss);

      if (subsegBlss < offsetBlss) {
        // Need to make the tree taller.
	while (subsegBlss < offsetBlss) {
	  DEBUG(invalid) kdprintf(KR_OSTREAM, "  Growing: subsegblss %d offsetblss %d\n",
				  subsegBlss, offsetBlss);
      
	  /* Buy a new GPT to expand with: */
	  if (capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_NEWOBJ)
              != RC_OK)
	    return RC_capros_key_NoMoreNodes;
      
	  /* Make that node have BLSS == subsegBlss+1: */
          capros_GPT_setL2v(KR_NEWOBJ, BlssToL2v(subsegBlss+1));

	  /* Insert the old subseg into slot 0: */
	  capros_GPT_setSlot(KR_NEWOBJ, 0, KR_SCRATCH);

	  /* Populate slots 1 and higher with suitable primordial zero
	     subsegments. */
	  {	 
	    int i;

	    node_copy(KR_ZINDEX, subsegBlss, KR_SCRATCH);
	
	    for (i = 1; i < EROS_NODE_SIZE; i++)
	      capros_GPT_setSlot(KR_NEWOBJ, i, KR_SCRATCH);
	  }

	  COPY_KEYREG(KR_NEWOBJ, KR_SCRATCH);

	  /* Finally, insert the new subseg into the original GPT */
          // FIXME: only need to do this after the iteration
	  capros_GPT_setSlot(KR_SEGMENT, 0, KR_SCRATCH);

	  subsegBlss++;
	}

        /* FIXME: there is a window between setting slot 0 of the
           original GPT, and setting its blss, in which the segment
           is wrong. */

	if (subsegBlss >= segBlss) {	// FIXME: this is always true!
	  /* Segment has grown.  Rewrite the format key to reflect the
	     new segment size. */

	  DEBUG(invalid) kdprintf(KR_OSTREAM, "  Red seg must grow\n");

	  segBlss = subsegBlss + 1;
          capros_GPT_setL2v(KR_SEGMENT, BlssToL2v(segBlss));
	}
      
	DEBUG(returns)
	  kdprintf(KR_OSTREAM,
		   "Returning from FC_InvalidAddr after growing"
		   " managed segment to blss=%d\n", subsegBlss);
	return RC_OK;
      }

      /* Segment is big enough, but some internal portion was
	   unpopulated.  Note (very important) that we have not yet
	   clobbered KR_SEGMENT. */

      DEBUG(invalid) kdprintf(KR_OSTREAM, "Invalid internal address -- falling through to COW logic for znode\n");

      DEBUG(invalid) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x\n",
			     slot, segBlss,
			     (uint32_t) (offset >> 32),
			     (uint32_t) offset);

      /* Traverse downward past all nodes.  No need to check
	 writability.  Had that been the problem we would have gotten
	 an access violation instead.

	 This logic will work fine as long as the whole segment tree
	 is nodes and pages, but will fail miserably if a subsegment
	 is plugged in here somewhere. */

      while (subsegBlss > EROS_PAGE_BLSS) {
	DEBUG(invalid) kdprintf(KR_OSTREAM, "  Walking down: subsegBlss %d\n",
				subsegBlss);

	result = capros_key_getType(KR_SCRATCH, &kt);
	if (result != RC_OK || kt != AKT_GPT) {
	  DEBUG(invalid) kdprintf(KR_OSTREAM, "  subsegBlss %d invalid!\n",
				  subsegBlss);
	  break;
	}

	COPY_KEYREG(KR_SCRATCH, KR_SEGMENT);

	slot = BlssSlotNdx(offset, subsegBlss);
	offset &= BlssMask(subsegBlss);

	DEBUG(invalid) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x\n",
			       slot, subsegBlss,
			       (uint32_t) (offset>>32),
			       (uint32_t) offset);

	subsegBlss--;
	
	capros_GPT_getSlot(KR_SEGMENT, slot, KR_SCRATCH);
      }
      
      /* Key in KR_SCRATCH is the read-only or invalid key.  Key in
	 KR_SEGMENT is its parent. */
      DEBUG(invalid) kdprintf(KR_OSTREAM, 
			      "Found rc=0x%x kt=0x%x at subsegBlss=%d\n",
			      result, kt, subsegBlss);

      /* Replace the offending subsegment with a primordial zero segment of
	   suitable size: */
      // Isn't subsegBlss always EROS_PAGE_BLSS here??
      {	 
	node_copy(KR_ZINDEX, subsegBlss, KR_SCRATCH);
	  
	capros_GPT_setSlot(KR_SEGMENT, slot, KR_SCRATCH);
      }

      DEBUG(returns)
	kdprintf(KR_OSTREAM,
		 "Returning from FC_InvalidAddr after populating"
		 " zero subsegment\n");
      return RC_OK;
    }
    
  case FC_Access:
    {
      /* Subseg is read-only */

      uint8_t subsegBlss;
      
      uint32_t result;
      uint32_t orig_offset = offset;
      
      bool needTraverse = true;

      DEBUG(access)
	kdprintf(KR_OSTREAM, "FC_Access at 0x%08x %08x\n",
		 (uint32_t) (offset>>32),
		 (uint32_t) offset);
      
#if KR_L1_NODE != KR_SEGMENT
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
	 path and have cached the right information.
	 
	 This hack only possible with 32-slot nodes. (Why??) */
      
      if (pState->was_access) {
	uint64_t xoff = offset ^ pState->last_offset;
	xoff >>= (EROS_PAGE_ADDR_BITS + EROS_NODE_LGSIZE);
	if (xoff == 0) {
	  needTraverse = false;
	  slot = (offset >> EROS_PAGE_ADDR_BITS) & EROS_NODE_SLOT_MASK;
	  subsegBlss = 2;
	  capros_GPT_getSlot(KR_L1_NODE, slot, KR_SCRATCH);

	  DEBUG(access)
	    kdprintf(KR_OSTREAM, "Re-traverse suppressed at slot %d!\n", slot);
	}
      }
#endif
      
      if (needTraverse) {
	pState->was_access = false; /* we COULD fail! */
	
	/* fetch out the subseg key */
	capros_GPT_getSlot(KR_SEGMENT, slot, KR_SCRATCH);

	/* find out its BLSS: */
        subsegBlss = GetBlss(KR_SCRATCH);
      
	segBlss = subsegBlss + 1;
      
	DEBUG(access) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x\n",
			      slot, segBlss,
			      (uint32_t) (offset >> 32),
			      (uint32_t) offset);

	/* Traverse downward past all writable GPTs: */

	while (subsegBlss > EROS_PAGE_BLSS) {
	
	  DEBUG(access) kdprintf(KR_OSTREAM, "  Walking down: subsegBlss %d\n",
				 subsegBlss);

	  result = capros_key_getType(KR_SCRATCH, &kt);
	  if (result != RC_OK || kt != AKT_GPT) {
	    DEBUG(access) kdprintf(KR_OSTREAM, "  subsegBlss %d invalid!\n",
				   subsegBlss);
	    break;
	  }

	  uint32_t perms;
          result = capros_Memory_getRestrictions(KR_SCRATCH, &perms);
	  if (result != RC_OK || (perms & capros_Memory_readOnly)) {
	    DEBUG(access) kdprintf(KR_OSTREAM, "  subsegBlss %d unwritable.\n",
				   subsegBlss);
	    break;
	  }

	  COPY_KEYREG(KR_SCRATCH, KR_SEGMENT);

	  slot = BlssSlotNdx(offset, subsegBlss);
	  offset &= BlssMask(subsegBlss);

	  DEBUG(access) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x %08x\n",
				slot, subsegBlss,
				(uint32_t) (offset>>32),
				(uint32_t) offset);

	  subsegBlss--;
	
	  capros_GPT_getSlot(KR_SEGMENT, slot, KR_SCRATCH);
	}
      
	/* Have now hit a read-only subtree, which we need to traverse,
	   turning the R/O GPTs into R/W GPTs. */

	while (subsegBlss > EROS_PAGE_BLSS) {
	  DEBUG(access) kdprintf(KR_OSTREAM, "  Walking down: COW subsegBlss %d\n",
				 subsegBlss);

	  /* Buy a new read-write GPT: */
	  if (capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_NEWOBJ)
              != RC_OK )
	    return RC_capros_key_NoMoreNodes;
      
	  /* Copy the slots and l2v from the old subseg into the new: */
	  capros_GPT_clone(KR_NEWOBJ, KR_SCRATCH);

	  /* Replace the old subseg with the new */
	  capros_GPT_setSlot(KR_SEGMENT, slot, KR_NEWOBJ);

	  COPY_KEYREG(KR_NEWOBJ, KR_SEGMENT);

	  /* Now traverse downward in the tree: */
	  slot = BlssSlotNdx(offset, subsegBlss);
	  offset &= BlssMask(subsegBlss);

	  DEBUG(access) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x\n",
				slot, subsegBlss,
				(uint32_t) (offset>>32),
				(uint32_t) offset);

	  subsegBlss--;

	  capros_GPT_getSlot(KR_SEGMENT, slot, KR_SCRATCH);
	}

#if KR_L1_NODE != KR_SEGMENT
	/* KR_SEGMENT now points to the parent of the page key we are
	   COWing. If we are doing the L1 node memoization, record the
	   last manipulated L1 node here. */
	COPY_KEYREG(KR_SEGMENT, KR_L1_NODE);
#endif
      }

      /* Now KR_SCRATCH names a page and KR_L1_NODE is its containing
	 node, and KR_L1_NODE[slot] == KR_SCRATCH. */

      DEBUG(access) kdprintf(KR_OSTREAM, "  Walking down: COW"
			     " orig_offset 0x%08x first zero offset 0x%08x\n",
			     orig_offset,
			     pState->first_zero_offset);
      
      if (orig_offset >= pState->first_zero_offset) {
	uint32_t count;
	
	/* Insert new zero pages at this location. As an optimization,
	   insert ZERO_EXTEND_BY pages at a time. */
	for (count = 0; ((slot < EROS_NODE_SIZE) &&
			 (count < ZERO_EXTEND_BY)); count++, slot++) {
	  uint32_t kr = AllocPage(pState);

	  if (kr == KR_VOID) {
	    if (count)
	      break;
	    else
	      return RC_capros_key_NoMorePages;
	  }

	  capros_GPT_setSlot(KR_L1_NODE, slot, kr);

	  pState->was_access = true;
	  pState->last_offset = 
	    (orig_offset & ~EROS_PAGE_MASK) + (count-1) * EROS_PAGE_SIZE;
	}
	DEBUG(returns)
	  kdprintf(KR_OSTREAM,
		   "Returning from FC_Access after appending %d empty"
		   " pages at 0x%08x\n", count, orig_offset);
	return RC_OK;
      }
      else {
	uint32_t kr = AllocPage(pState);
	if (kr == KR_VOID)
	  return RC_capros_key_NoMorePages;

	capros_Page_clone(kr, KR_SCRATCH);
	    
	/* Replace the old page with the new */
	capros_GPT_setSlot(KR_L1_NODE, slot, kr);

	pState->was_access = true;
	pState->last_offset = orig_offset;
	
	/* We assume at this point that the page in question no longer
	   contains zeros, so the first zero offset must be above this
	   point. */
	if (orig_offset >= pState->first_zero_offset)
	  pState->first_zero_offset =
	    (orig_offset & ~EROS_PAGE_MASK) + EROS_PAGE_SIZE;

	DEBUG(returns)
	  kdprintf(KR_OSTREAM,
		   "Returning from FC_Access after duplicating"
		   " page at 0x%08x\n", orig_offset);

	return RC_OK;
      }
    }

  default:
    return pMsg->rcv_w1;	/* fault code */
  }
}

int
ProcessRequest(Message *argmsg, state *pState)
{
  uint32_t result = RC_OK;
  
  switch(argmsg->rcv_code) {
  case OC_capros_key_getType:			/* check alleged keytype */
    {
      argmsg->snd_w1 = AKT_VcskSeg;
      break;
    }      
  case OC_Vcsk_InvokeKeeper:
    if (pState->frozen) {
      result = RC_capros_key_RequestError;
      break;
    }
    
    result = HandleSegmentFault(argmsg, pState);
    break;

  case OC_Vcsk_MakeSpaceKey:
    {
      capros_Memory_reduce(KR_SEGMENT, capros_Memory_opaque | argmsg->rcv_w1,
                           KR_SEGMENT);
      argmsg->snd_key0 = KR_SEGMENT;
      result = RC_OK;
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
      DestroySegment(pState);

      Sepuku();
      break;
    }
    
    /* above not implemented yet */
  default:
    result = RC_capros_key_UnknownRequest;
    break;
  }
  
  argmsg->snd_code = result;
  return 1;
}

/* In spite of unorthodox fabrication, the constructor self-destructs
   in the usual way. */
void
Sepuku()
{
  node_copy(KR_CONSTIT, KC_PROTOSPC, KR_SEGMENT);

  capros_SpaceBank_free1(KR_BANK, KR_CONSTIT);

  /* Invoke the protospace with arguments indicating that we should be
     demolished as a small space domain */
  protospace_destroy(KR_VOID, KR_SEGMENT, KR_SELF,
		     KR_CREATOR, KR_BANK, 1);
}

/* Tree destruction is a little tricky.  We know the height of the
   tree, but we have no simple way to build a stack of node pointers
   as we traverse it.

   The following algorithm is pathetically brute force.
   */
int
ReturnWritableSubtree(uint32_t krTree)
{
  uint32_t perms = 0;
  uint32_t kt;
  uint32_t result = capros_key_getType(krTree, &kt);
  
  for (;;) {
    if (result != RC_OK || kt == RC_capros_key_Void)
      /* Segment has been fully demolished. */
      return 0;

    if (kt == AKT_Page || kt == AKT_GPT)
      result = capros_Memory_getRestrictions(krTree, &perms);

    if (perms & capros_Memory_readOnly)
      /* This case can occur if the entire segment is both tall and
         unmodified. */
      return 0;

    if (kt == AKT_Page) {
      capros_SpaceBank_free1(KR_BANK, krTree);
      return 1;			/* more to do */
    }
    else if (kt == AKT_GPT) {
      int i;
      for (i = 0; i < EROS_NODE_SIZE; i++) {
	uint32_t sub_kt;
	uint32_t result;
	uint32_t subPerms = 0;

	capros_GPT_getSlot(krTree, i, KR_SCRATCH2);
      
	result = capros_key_getType(KR_SCRATCH2, &sub_kt);

        // FIXME: this is clearly wrong!
	if (kt == AKT_Page || kt == AKT_GPT)
          result = capros_Memory_getRestrictions(krTree, &subPerms);

	if ((subPerms & capros_Memory_readOnly) == 0) {
	  /* do nothing */
	}
	else if (sub_kt == AKT_Page) {
	  capros_SpaceBank_free1(KR_BANK, KR_SCRATCH2);
	}
	else if (sub_kt == AKT_GPT) {
	  COPY_KEYREG(KR_SCRATCH2, KR_SCRATCH);
	  kt = sub_kt;
	  break;
	}
      }

      if (i == EROS_NODE_SIZE) {
	/* If we get here, this node has no children.  Return it, but
	   also return 1 because it may have a parent node. */
	capros_SpaceBank_free1(KR_BANK, KR_SCRATCH);
	return 1;
      }
    }
    else {
      kdprintf(KR_OSTREAM, "Returning a key with kt=0x%x\n", kt);
    }
  }
}

void
DestroySegment(state *mystate)
{
  for(;;) {
    capros_GPT_getSlot(KR_SEGMENT, 0, KR_SCRATCH);

    if (!ReturnWritableSubtree(KR_SCRATCH))
      break;
  }

  DEBUG(destroy) kdprintf(KR_OSTREAM, "Destroying red seg root\n");
  capros_SpaceBank_free1(KR_BANK, KR_SEGMENT);
}

void
Initialize(state *mystate)
{
  uint32_t result;
  
  mystate->was_access = false;
  mystate->first_zero_offset = ~0ull; /* until proven otherwise below */
  mystate->npage = 0;
  
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_copy(KR_CONSTIT, KC_ZINDEX, KR_ZINDEX);
  node_copy(KR_CONSTIT, KC_FROZEN_SEG, KR_ARG(0));

  DEBUG(init) kdprintf(KR_OSTREAM, "Fetch BLSS of frozen seg\n");
  /* find out BLSS of frozen segment: */
  uint8_t segBlss = GetBlss(KR_ARG(0));

  DEBUG(init) kdprintf(KR_OSTREAM, "BLSS of frozen seg was %d\n", segBlss);

  segBlss += 1;
  
  mystate->first_zero_offset = 1ull << BlssToL2v(segBlss);
    
  DEBUG(init) kdprintf(KR_OSTREAM, "Offsets above 0x%08x%08x are"
		       " presumed to be zero\n",
		       (uint32_t) (mystate->first_zero_offset >> 32),
		       (uint32_t) mystate->first_zero_offset);
  
  DEBUG(init) kdprintf(KR_OSTREAM, "Buy new GPT\n");
  
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_SEGMENT);

  DEBUG(init) kdprintf(KR_OSTREAM, "Initialize it\n");

  result = capros_GPT_setL2v(KR_SEGMENT, BlssToL2v(segBlss));

  /* write the immutable seg to slot 0 */
  result = capros_GPT_setSlot(KR_SEGMENT, 0, KR_ARG(0));

  /* Make a start key to ourself and set as keeper. */
  process_make_start_key(KR_SELF, 0, KR_ARG(0));
  result = capros_GPT_setKeeper(KR_SEGMENT, KR_ARG(0));

  result = capros_Memory_reduce(KR_SEGMENT, capros_Memory_opaque, KR_SEGMENT);

  DEBUG(init) kdprintf(KR_OSTREAM, "GPT now constructed... returning\n");
}

int
main()
{
  Message msg;

  state mystate;

  Initialize(&mystate);

  DEBUG(init) kdprintf(KR_OSTREAM, "Initialized VCSK\n");
    
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_SEGMENT;	/* first return: seg key */
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
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_SEGMENT;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    RETURN(&msg);
    mystate.frozen = msg.rcv_keyInfo;
    msg.snd_key0 = KR_VOID;	/* until otherwise proven */
  } while ( ProcessRequest(&msg, &mystate) );

  return 0;
}
