/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* Memmap -- A memory mapping domain based on the VCSK. Each address
   space that a client wishes to map requires a separate Memmap
   domain. */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/cap-instr.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/Range.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>

#include <domain/VcskKey.h>
#include <domain/MemmapKey.h>
#include <domain/domdbg.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>

#define min(a,b) ((a) <= (b) ? (a) : (b))

#include "constituents.h"

#define dbg_init      0x0001
#define dbg_invalid   0x0002
#define dbg_access    0x0004
#define dbg_op        0x0008
#define dbg_destroy   0x0010
#define dbg_returns   0x0020
#define dbg_msg_trunc 0x0040

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define KR_SCRATCH  KR_APP(1)
#define KR_SCRATCH2 KR_APP(2)

#define KR_OSTREAM  KR_APP(4)
#define KR_L1_NODE  KR_APP(5)		/* last L1 node we COW'd a page into */

#define KR_NEWPAGE   KR_APP(7)

#define KR_NEWOBJ    KR_ARG(0)
#define KR_PHYSRANGE KR_ARG(1)
#define KR_GPT   KR_ARG(2)

/* If this was a direct invocation of our start key, the key parameters hold
   whatever the user passed.

   If this was a pass-through invocation via a GPT key, these
   hold what the user passed except that KR_GPT has been replaced
   by a non-opaque key to the segment GPT.

   If this was a kernel-generated fault invocation, KR_ARG(0) and KR_PHYSRANGE
   hold void keys, KR_GPT holds a non-opaque key to the segment GPT,
   and KR_RETURN holds a fault key. */

/* IMPORTANT optimization:

   VCSK serves to implement both demand-copy and demand-zero
   segments.  Of the cycles spent invoking capabilities, it proves
   that about 45% of them are spent in page_clone, and another 45% in
   range key calls (done by the space bank).  The only call to
   page_clone is here.  It is unavoidable when we are actually doing a
   virtual copy, but very much avoidable if we are doing demand-zero
   extension on an empty or short segment -- the page we get from the
   space bank is already zeroed.

   We therefore remember in the VCSK state the offset of the *end* of
   the last non-zero page.  Anything past this is known to be zero.
   We take advantage of this to know when the page_clone() operation
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

/* globals */
uint64_t paddr_start = 0x0ul;	/* phys address that's mapped to start
				   of new space*/
uint64_t paddr_size  = 0x0ul;	/* size of new space (in bytes) */

/* Make sure the following buffer is large enough to hold all the
   arguments received from a client! */
uint32_t receive_buffer[4];

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
  else return (1ull << bits_to_shift) - 1ull;
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
HandleSegmentFault(Message *pMsg, state *pState)
{
  uint32_t slot = 0;
  uint32_t kt = RC_capros_key_Void;
  uint32_t segBlss = EROS_PAGE_BLSS + 1; /* until otherwise proven */
  uint64_t offset = ((uint64_t) pMsg->rcv_w3) << 32 | (uint64_t) pMsg->rcv_w2;
  uint64_t orig_offset = offset;
  uint32_t offsetBlss = BiasedLSS(offset);

  switch (pMsg->rcv_w1) {
  case capros_Process_FC_InvalidAddr:
    {
      /* Subseg is too small */
      
      /* OPTIMIZATION NOTE:

	 The effect of capros_Process_FC_InvalidAddr is to zero-extend the
	 segment.  It therefore cannot alter the value of
	 /first_zero_offset/.
      */
      
      uint32_t result;
      
      pState->was_access = false;
      
      DEBUG(invalid)
	kprintf(KR_OSTREAM, "FC_SegInvalidAddr at 0x%08x %08x\n",
		 (uint32_t) (offset>>32),
		 (uint32_t) offset);
      
      /* fetch out the subseg key */
      capros_GPT_getSlot(KR_GPT, slot, KR_SCRATCH);

      /* find out its BLSS: */
      uint8_t subsegBlss = GetBlss(KR_SCRATCH);

      segBlss = subsegBlss + 1;
      
      DEBUG(invalid) kprintf(KR_OSTREAM, "FC_SegInvalidAddr: segBlss %d "
			     "offsetBlss %d subsegBlss %d\n", segBlss, 
			     offsetBlss, subsegBlss);

      if (subsegBlss < offsetBlss) {
	// Need to make the tree taller.
	while (subsegBlss < offsetBlss) {
	  DEBUG(invalid) kprintf(KR_OSTREAM, "  Growing: subsegblss %d offsetblss %d\n",
				  subsegBlss, offsetBlss);
      
	  /* Buy a new GPT to expand with: */
	  if (capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_NEWOBJ)
              != RC_OK)
	    return RC_capros_key_NoMoreNodes;
      
	  /* Make that GPT have BLSS == subsegBlss+1: */
	  capros_GPT_setL2v(KR_NEWOBJ, BlssToL2v(subsegBlss+1));

	  /* Insert the old subseg into slot 0: */
	  capros_GPT_setSlot(KR_NEWOBJ, 0, KR_SCRATCH);

	  COPY_KEYREG(KR_NEWOBJ, KR_SCRATCH);

	  /* Finally, insert the new subseg into the original GPT.*/
          // FIXME: only need to do this after the iteration
	  capros_GPT_setSlot(KR_GPT, 0, KR_SCRATCH);

	  subsegBlss++;
	}

	if (subsegBlss >= segBlss) {	// FIXME: this is always true!
	  /* Segment has grown.  Rewrite the format key to reflect the
	     new segment size. */

	  DEBUG(invalid) kprintf(KR_OSTREAM, "  Red seg must grow\n");

	  segBlss = subsegBlss + 1;
          capros_GPT_setL2v(KR_GPT, BlssToL2v(segBlss));
	}
      
	DEBUG(returns)
	  kprintf(KR_OSTREAM,
		   "Returning from capros_Process_FC_InvalidAddr after growing"
		   " managed segment to blss=%d\n", subsegBlss);
	return RC_OK;
      }

      /* Segment is big enough, but some internal portion was
	   unpopulated.  Note (very important) that we have not yet
	   clobbered KR_GPT. */

      DEBUG(invalid) kprintf(KR_OSTREAM, "Invalid internal address -- falling through to COW logic for znode\n");

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
	DEBUG(invalid) kprintf(KR_OSTREAM, "  Walking down: subsegBlss %d\n",
				subsegBlss);

	result = capros_key_getType(KR_SCRATCH, &kt);
	if (result != RC_OK || kt != AKT_GPT) {
	  DEBUG(invalid) kprintf(KR_OSTREAM, "  subsegBlss %d invalid!\n",
				  subsegBlss);
	  break;
	}

	COPY_KEYREG(KR_SCRATCH, KR_GPT);

	slot = BlssSlotNdx(offset, subsegBlss);
	offset &= BlssMask(subsegBlss);

	DEBUG(invalid) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x\n",
			       slot, subsegBlss,
			       (uint32_t) (offset>>32),
			       (uint32_t) offset);

	subsegBlss--;
	
	capros_GPT_getSlot(KR_GPT, slot, KR_SCRATCH);
      }
      
      /* Key in KR_SCRATCH is the read-only or invalid key.  Key in
	 KR_GPT is its parent. */
      DEBUG(invalid) kprintf(KR_OSTREAM, 
			      "Found rc=0x%x kt=0x%x at subsegBlss=%d\n",
			      result, kt, subsegBlss);

      /* Shap:  if subsegBlss == EROS_PAGE_BLSS then buy a page key
	 else

buy a virgin node from space bank and set node key to have blss =
      subsegBLSS and stick that node in tree here.
      */
      if (subsegBlss == EROS_PAGE_BLSS) {
	uint64_t paddr = orig_offset + paddr_start;
	uint32_t result;

	DEBUG(invalid) kprintf(KR_OSTREAM, "... waiting on object key for "
			       "paddr = 0x%08x", paddr);

	if (paddr < (paddr_start + paddr_size)) {
	  result = capros_Range_waitPageKey(KR_PHYSRANGE,
					  (paddr / EROS_PAGE_SIZE) *
					  EROS_OBJECTS_PER_FRAME,
					  KR_NEWPAGE);

	  if (result != RC_OK)
	    kprintf(KR_OSTREAM, "** ERROR: range_waitobjectkey returned %u",
		    result);
	  capros_GPT_setSlot(KR_GPT, slot, KR_NEWPAGE);
	}
	else {

	  /* Client exceeded specified limit of this mapped range */
	  return RC_capros_key_RequestError;
	}
      }
      else {
	DEBUG(invalid) kprintf(KR_OSTREAM, "... buying a new GPT for slot "
			       "%u", slot);

	capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_SCRATCH);
	capros_GPT_setL2v(KR_SCRATCH, BlssToL2v(subsegBlss));
	capros_GPT_setSlot(KR_GPT, slot, KR_SCRATCH);
      }

      DEBUG(returns)
	kprintf(KR_OSTREAM,
		 "Returning from capros_Process_FC_InvalidAddr after populating"
		 " zero subsegment\n");
      return RC_OK;
    }
    
  case capros_Process_FC_AccessViolation:
    {
      kprintf(KR_OSTREAM, "*** MEMMAP:  should never generate capros_Process_FC_AccessViolation!");
      return RC_capros_key_RequestError;
      break;
    }

  default:
    return pMsg->rcv_w1;	/* fault code */
  }
  return RC_OK;
}

int
ProcessRequest(Message *argmsg, state *pState)
{
  uint32_t result = RC_OK;
  
  switch(argmsg->rcv_code) {

    /* This domain is an address space keeper, so it must implement
       the VCSK interface as well: */
  case OC_Vcsk_InvokeKeeper:
    if (pState->frozen) {
      result = RC_capros_key_RequestError;
      break;
    }
    
    result = HandleSegmentFault(argmsg, pState);
    break;

    /* The client has the GPT key from the constructor.  Now it
       needs to inform this domain about the actual start and size of
       the range the client needs mapped. Once this command is
       completed the needed space will be fabricated and mapped
       lazily, as the client causes address faults in the space. */
  case OC_Memmap_Map:
    {
      uint32_t got = min(argmsg->rcv_limit, argmsg->rcv_sent);
      uint32_t expect = 4 * sizeof(uint32_t);

      if (got != expect) {
	DEBUG(msg_trunc) kprintf(KR_OSTREAM, "** ERROR: memmap() message "
				 "truncated: expect=%u and got=%u", expect,
				 got);
	argmsg->snd_code = RC_capros_key_RequestError;
	return 1;
      }

      paddr_start = ((uint64_t)receive_buffer[1] << 32) |
	(uint64_t)receive_buffer[0];
      paddr_size = ((uint64_t)receive_buffer[3] << 32) |
	(uint64_t)receive_buffer[2];

      argmsg->snd_w1 = BiasedLSS(paddr_size-1);
      argmsg->rcv_key1 = KR_VOID;
      break;
    }
    
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
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_GPT);

  capros_SpaceBank_free1(KR_BANK, KR_CONSTIT);

  /* Invoke the protospace with arguments indicating that we should be
     demolished as a small space domain */
  protospace_destroy(KR_VOID, KR_GPT, KR_SELF,
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
      kprintf(KR_OSTREAM, "Returning a key with kt=0x%x\n", kt);
    }
  }
}

void
DestroySegment(state *mystate)
{
  for(;;) {
    capros_GPT_getSlot(KR_GPT, 0, KR_SCRATCH);

    if (!ReturnWritableSubtree(KR_SCRATCH))
      break;
  }

  DEBUG(destroy) kprintf(KR_OSTREAM, "Destroying red seg root\n");
  capros_SpaceBank_free1(KR_BANK, KR_GPT);
}

void
Initialize(state *mystate)
{
  uint32_t result;
  
  mystate->was_access = false;
  mystate->first_zero_offset = ~0ull; /* until proven otherwise below */
  mystate->npage = 0;
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  /* find out BLSS of frozen segment: */
  uint8_t segBlss = GetBlss(KR_ARG(0));

  segBlss += 1;

  mystate->first_zero_offset = 1ull << BlssToL2v(segBlss);
    
  DEBUG(init) kprintf(KR_OSTREAM, "Offsets above 0x%08x%08x are"
		       " presumed to be zero\n",
		       (uint32_t) (mystate->first_zero_offset >> 32),
		       (uint32_t) mystate->first_zero_offset);
  
  DEBUG(init) kprintf(KR_OSTREAM, "Buy new GPT\n");
  
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_GPT);

  DEBUG(init) kprintf(KR_OSTREAM, "Initialize it\n");

  result = capros_GPT_setL2v(KR_GPT, BlssToL2v(segBlss));

  /* write the immutable seg to slot 0 */
  capros_GPT_setSlot(KR_GPT, 0, KR_ARG(0));

  /* Make a start key to ourself and set as keeper. */
  capros_Process_makeStartKey(KR_SELF, 0, KR_ARG(0));
  result = capros_GPT_setKeeper(KR_GPT, KR_ARG(0));

  result = capros_Memory_reduce(KR_GPT, capros_Memory_opaque, KR_GPT);

  DEBUG(init) kprintf(KR_OSTREAM, "GPT now constructed... returning\n");
}

int
main()
{
  Message msg;

  state mystate;

  Initialize(&mystate);

  DEBUG(init) kprintf(KR_OSTREAM, "Initialized memmap\n");
    
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_GPT;	/* first return: GPT key */
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
  msg.rcv_key1 = KR_PHYSRANGE;
  msg.rcv_key2 = KR_GPT;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  do {
    msg.rcv_data = receive_buffer;
    msg.rcv_limit = sizeof(receive_buffer);
    RETURN(&msg);
    mystate.frozen = msg.rcv_keyInfo;
    msg.snd_key0 = KR_VOID;	/* until otherwise proven */
  } while ( ProcessRequest(&msg, &mystate) );

  return 0;
}
