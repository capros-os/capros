/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* Memmap -- A memory mapping domain based on the VCSK. Each address
   space that a client wishes to map requires a separate Memmap
   domain. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/PageKey.h>
#include <eros/ProcessKey.h>
#include <eros/StdKeyType.h>
#include <eros/cap-instr.h>
#include <eros/KeyConst.h>

#include <idl/eros/key.h>
#include <idl/eros/Range.h>
#include <idl/eros/Number.h>

#include <domain/VcskKey.h>
#include <domain/MemmapKey.h>
#include <domain/domdbg.h>
#include <domain/SpaceBankKey.h>
#include <domain/ProtoSpace.h>
#include <domain/Runtime.h>

#include <stdlib.h>

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
#define KR_WRAPPER   KR_ARG(2)

/* If this was a direct invocation of our start key, RK0..RESUME hold
   whatever the user passed.

   If this was a pass-through invocation via a wrapper key, these
   hold what the user passed except that KR_ARG(2) may have been replaced
   by a capage key to the red space capage.

   If this was a kernel-generated fault invocation, KR_ARG(0) and KR_ARG(1)
   hold zero data keys, KR_ARG(2) holds whatever the upcall pass-through
   convention specified, and KR_RETURN holds a fault key. */

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
  bool_t was_access;
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
  if (blss < EROS_PAGE_BLSS)
    return 0ull;

  {
#if (EROS_PAGE_ADDR_BITS == (EROS_PAGE_BLSS * EROS_NODE_LGSIZE))
    uint32_t bits_to_shift = blss * EROS_NODE_LGSIZE;
#else
    uint32_t bits_to_shift =
      (blss - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS; 
#endif

    if (bits_to_shift >= UINT64_BITS)
      return (uint64_t) -1;	/* all 1's */

    {
      uint64_t mask = (1ull << bits_to_shift);
      mask -= 1ull;
      
      return mask;
    }
  }
}

uint32_t
BlssSlotNdx(uint64_t offset, uint32_t blss)
{
  if (blss <= EROS_PAGE_BLSS)
    return 0;

  {
#if (EROS_PAGE_ADDR_BITS == (EROS_PAGE_BLSS * EROS_NODE_LGSIZE))
    uint32_t bits_to_shift = blss * EROS_NODE_LGSIZE;
#else
    uint32_t bits_to_shift =
      (blss - EROS_PAGE_BLSS - 1) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS; 
#endif

    if (bits_to_shift >= UINT64_BITS)
      return 0;

    return offset >> bits_to_shift;
  }
}

uint32_t
HandleSegmentFault(Message *pMsg, state *pState)
{
  uint32_t slot = 0;
  uint32_t kt = RC_eros_key_Void;
  uint32_t offsetBlss;
  uint32_t segBlss = EROS_PAGE_BLSS + 1; /* until otherwise proven */
  uint64_t offset = ((uint64_t) pMsg->rcv_w3) << 32 | (uint64_t) pMsg->rcv_w2;
  uint64_t orig_offset = offset;

  offsetBlss = BiasedLSS(offset);

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
      uint16_t subsegBlss;
      
      pState->was_access = false;
      
      DEBUG(invalid)
	kprintf(KR_OSTREAM, "FC_SegInvalidAddr at 0x%08x %08x\n",
		 (uint32_t) (offset>>32),
		 (uint32_t) offset);
      
      /* fetch out the subseg key */
      node_copy(KR_WRAPPER, slot, KR_SCRATCH);

      /* find out its BLSS: */
      result = get_lss_and_perms(KR_SCRATCH, &subsegBlss, 0);
      if (result == RC_eros_key_UnknownRequest) {
	DEBUG(invalid) kprintf(KR_OSTREAM, "    Assuming slot 0 is page key.");
	subsegBlss = EROS_PAGE_BLSS; /* it must be a page key */
      }
      
      subsegBlss &= SEGMODE_BLSS_MASK;

      segBlss = subsegBlss + 1;
      
      DEBUG(invalid) kprintf(KR_OSTREAM, "FC_SegInvalidAddr: segBlss %d "
			     "offsetBlss %d subsegBlss %d\n", segBlss, 
			     offsetBlss, subsegBlss);

      if (subsegBlss < offsetBlss) {
	/* Except at the top, we make no attempt to short-circuit the
	   space tree. */
	while (subsegBlss < offsetBlss) {
	  DEBUG(invalid) kprintf(KR_OSTREAM, "  Growing: subsegblss %d offsetblss %d\n",
				  subsegBlss, offsetBlss);
      
	  /* Buy a new node to expand with: */
	  if (spcbank_buy_nodes(KR_BANK, 1, KR_NEWOBJ, KR_VOID, KR_VOID) !=
	      RC_OK)
	    return RC_eros_key_NoMoreNodes;
      
	  /* Make that node have BLSS == subsegBlss+1: */
	  node_make_node_key(KR_NEWOBJ, subsegBlss+1, 0, KR_NEWOBJ);

	  /* Insert the old subseg into slot 0: */
	  node_swap(KR_NEWOBJ, 0, KR_SCRATCH, KR_VOID);

	  COPY_KEYREG(KR_NEWOBJ, KR_SCRATCH);

	  /* Finally, insert the new subseg into the original red
	     segment */
	  node_swap(KR_WRAPPER, 0, KR_SCRATCH, KR_VOID);

	  subsegBlss++;
	}

	if (subsegBlss >= segBlss) {
	  /* Segment has grown.  Rewrite the format key to reflect the
	     new segment size. */

	  eros_Number_value nkv;

	  DEBUG(invalid) kprintf(KR_OSTREAM, "  Red seg must grow\n");

	  segBlss = subsegBlss + 1;

	  nkv.value[0] = WRAPPER_SEND_NODE | WRAPPER_KEEPER;
	  WRAPPER_SET_BLSS(nkv, segBlss);

	  nkv.value[1] = 0;
	  nkv.value[2] = 0;

	  node_write_number(KR_WRAPPER, WrapperFormat, &nkv);
	}
      
	DEBUG(returns)
	  kprintf(KR_OSTREAM,
		   "Returning from FC_InvalidAddr after growing"
		   " managed segment to blss=%d\n", subsegBlss);
	return RC_OK;
      }

      /* Segment is big enough, but some internal portion was
	   unpopulated.  Note (very important) that we have not yet
	   clobbered KR_WRAPPER. */

      DEBUG(invalid) kprintf(KR_OSTREAM, "Invalid internal address -- falling through to COW logic for znode\n");

      DEBUG(invalid) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x\n",
			     slot, segBlss,
			     (uint32_t) (offset >> 32),
			     (uint32_t) offset);

      /* Traverse downward past all nodes.  No need to check
	 writability.  Had that been the problem we would have gotten
	 an access violation instead.

	 This logic will work fine as long as the whole segment tree
	 is nodes and pages, but will fail miserably is a subsegment
	 is plugged in here somewhere. */

      while (subsegBlss > EROS_PAGE_BLSS) {
	DEBUG(invalid) kprintf(KR_OSTREAM, "  Walking down: subsegBlss %d\n",
				subsegBlss);

	result = eros_key_getType(KR_SCRATCH, &kt);
	if (result != RC_OK || kt != AKT_Node) {
	  DEBUG(invalid) kprintf(KR_OSTREAM, "  subsegBlss %d invalid!\n",
				  subsegBlss);
	  break;
	}

	COPY_KEYREG(KR_SCRATCH, KR_WRAPPER);

	slot = BlssSlotNdx(offset, subsegBlss);
	offset &= BlssMask(subsegBlss-1);

	DEBUG(invalid) kprintf(KR_OSTREAM, "  traverse slot %d, blss %d, offset 0x%08x %08x\n",
			       slot, subsegBlss,
			       (uint32_t) (offset>>32),
			       (uint32_t) offset);

	subsegBlss--;
	
	node_copy(KR_WRAPPER, slot, KR_SCRATCH);
      }
      
      /* Key in KR_SCRATCH is the read-only or invalid key.  Key in
	 KR_WRAPPER is it's parent. */
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
	  result = eros_Range_waitPageKey(KR_PHYSRANGE,
					  (paddr / EROS_PAGE_SIZE) *
					  EROS_OBJECTS_PER_FRAME,
					  KR_NEWPAGE);

	  if (result != RC_OK)
	    kprintf(KR_OSTREAM, "** ERROR: range_waitobjectkey returned %u",
		    result);
	  node_swap(KR_WRAPPER, slot, KR_NEWPAGE, KR_VOID);
	}
	else {

	  /* Client exceeded specified limit of this mapped range */
	  return RC_eros_key_RequestError;
	}
      }
      else {
	DEBUG(invalid) kprintf(KR_OSTREAM, "... buying a new node for slot "
			       "%u", slot);

	spcbank_buy_nodes(KR_BANK, 1, KR_SCRATCH, KR_VOID, KR_VOID);
	node_make_node_key(KR_SCRATCH, subsegBlss, 0, KR_SCRATCH);
	node_swap(KR_WRAPPER, slot, KR_SCRATCH, KR_VOID);
      }

      DEBUG(returns)
	kprintf(KR_OSTREAM,
		 "Returning from FC_InvalidAddr after populating"
		 " zero subsegment\n");
      return RC_OK;
    }
    
  case FC_Access:
    {
      kprintf(KR_OSTREAM, "*** MEMMAP:  should never generate FC_Access!");
      return RC_eros_key_RequestError;
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
      result = RC_eros_key_RequestError;
      break;
    }
    
    result = HandleSegmentFault(argmsg, pState);
    break;

    /* The client has the wrapper key from the constructor.  Now it
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
	argmsg->snd_code = RC_eros_key_RequestError;
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
    
  case OC_eros_key_destroy:
    {
      DestroySegment(pState);

      Sepuku();
      break;
    }
    
    /* above not implemented yet */
  default:
    result = RC_eros_key_UnknownRequest;
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
  node_copy(KR_CONSTIT, KC_PROTOSPC, KR_WRAPPER);

  spcbank_return_node(KR_BANK, KR_CONSTIT);

  /* Invoke the protospace with arguments indicating that we should be
     demolished as a small space domain */
  protospace_destroy(KR_VOID, KR_WRAPPER, KR_SELF,
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
  uint16_t ndlss = 0;
  uint8_t  perms = 0;
  uint32_t kt;
  uint32_t result = eros_key_getType(krTree, &kt);
  
  for (;;) {
    if (result != RC_OK || kt == RC_eros_key_Void)
      /* Segment has been fully demolished. */
      return 0;

    if (kt == AKT_Page || kt == AKT_Node)
      get_lss_and_perms(krTree, &ndlss, &perms);

    if (perms & SEGPRM_RO)
      /* This case can occur if the entire segment is both tall and
         unmodified. */
      return 0;

    if (kt == AKT_Page) {
      spcbank_return_data_page(KR_BANK, krTree);
      return 1;			/* more to do */
    }
    else if (kt == AKT_Node) {
      int i;
      for (i = 0; i < EROS_NODE_SIZE; i++) {
	uint32_t sub_kt;
	uint32_t result;
	uint16_t subLss = 0;
	uint8_t subPerms = 0;

	node_copy(krTree, i, KR_SCRATCH2);
      
	result = eros_key_getType(KR_SCRATCH2, &sub_kt);

	if (kt == AKT_Page || kt == AKT_Node)
	  get_lss_and_perms(krTree, &subLss, &subPerms);

	if ((subPerms & SEGPRM_RO) == 0) {
	  /* do nothing */
	}
	else if (sub_kt == AKT_Page) {
	  spcbank_return_data_page(KR_BANK, KR_SCRATCH2);
	}
	else if (sub_kt == AKT_Node) {
	  COPY_KEYREG(KR_SCRATCH2, KR_SCRATCH);
	  kt = sub_kt;
	  break;
	}
      }

      if (i == EROS_NODE_SIZE) {
	/* If we get here, this node has no children.  Return it, but
	   also return 1 because it may have a parent node. */
	spcbank_return_node(KR_BANK, KR_SCRATCH);
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
  eros_Number_value offset;

  offset.value[0] = 0;
  offset.value[1] = 0;
  offset.value[2] = 0;

  for(;;) {
    node_copy(KR_WRAPPER, 0, KR_SCRATCH);

    if (!ReturnWritableSubtree(KR_SCRATCH))
      break;
  }

  DEBUG(destroy) kprintf(KR_OSTREAM, "Destroying red seg root\n");
  spcbank_return_node(KR_BANK, KR_WRAPPER);
}

void
Initialize(state *mystate)
{
  uint32_t result;
  uint16_t segBlss;
  eros_Number_value nkv;
  
  mystate->was_access = false;
  mystate->first_zero_offset = ~0ull; /* until proven otherwise below */
  mystate->npage = 0;
  
  node_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  /* find out BLSS of frozen segment: */
  result = get_lss_and_perms(KR_ARG(0), &segBlss, 0);
  if (result == RC_eros_key_UnknownRequest)
    segBlss = EROS_PAGE_BLSS; /* it must be a page key */
      
  segBlss &= SEGMODE_BLSS_MASK;

  mystate->first_zero_offset =
    EROS_PAGE_SIZE << ( (segBlss - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE);
    
  DEBUG(init) kprintf(KR_OSTREAM, "Offsets above 0x%08x%08x are"
		       " presumed to be zero\n",
		       (uint32_t) (mystate->first_zero_offset >> 32),
		       (uint32_t) mystate->first_zero_offset);

  segBlss += 1;
  
  DEBUG(init) kprintf(KR_OSTREAM, "Buy new seg node\n");
  
  spcbank_buy_nodes(KR_BANK, 1, KR_WRAPPER, KR_VOID, KR_VOID);

  DEBUG(init) kprintf(KR_OSTREAM, "Make it a wrapper\n");

  /* write the format key */
  nkv.value[0] = WRAPPER_SEND_NODE | WRAPPER_KEEPER;
  WRAPPER_SET_BLSS(nkv, segBlss);
  nkv.value[1] = 0;
  nkv.value[2] = 0;

  node_write_number(KR_WRAPPER, WrapperFormat, &nkv);

  /* write the immutable seg to slot 0 */
  node_swap(KR_WRAPPER, 0, KR_ARG(0), KR_VOID);

  /* make a start key to ourself and write that in slot 14 */
  process_make_start_key(KR_SELF, 0, KR_ARG(0));

  /* now wrap the start key */
  node_swap(KR_WRAPPER, WrapperKeeper, KR_ARG(0), KR_VOID);

  node_make_wrapper_key(KR_WRAPPER, 0, 0, KR_WRAPPER);

  DEBUG(init) kprintf(KR_OSTREAM, "Wrapped space now constructed... "
		       "returning\n");
}

int
main()
{
  Message msg;

  state mystate;

  Initialize(&mystate);

  DEBUG(init) kprintf(KR_OSTREAM, "Initialized VCSK\n");
    
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_WRAPPER;	/* first return: wrapper key */
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
  msg.rcv_key2 = KR_ARG(2);	/* which is also KR_WRAPPER! */
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
