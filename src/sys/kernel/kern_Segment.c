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
#include <kerninc/KernStats.h>
#include <kerninc/Process.h>
#include <kerninc/SegWalk.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Depend.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <arch-kerninc/PTE.h>

/* Extracted as macro for clarity of code below, which is already too
 * complicated. 
 */
#define ADD_DEPEND(pKey) Depend_AddKey(pKey, pPTE, mapLevel);

/* WalkSeg() is a performance-critical path.  The segment walk
   logic is complex, and it occurs in the middle of page faulting
   activity, which for obvious reasons is performance-critical.

   The walker is also entered in stages to allow successive
   traversals.  This is mainly to keep the code machine-independent; a
   machine-dependent specialization of this routine could avoid a
   couple of procedure calls.

   The depth of the walk is bounded by MAX_SEG_DEPTH to detect
   possible loops in the segment tree.  Because the routine is
   multiply entered the actual depth limit can be two or three times
   the nominal limit.  The architecture guarantees a minimum traversal
   depth, but does not promise any particular maximum.


   The traversal process must maintain the following variables:

   /pSegKey/   pointer to the node slot containing the key that names
               the segment we are now considering.

   /segBlss/   size of the segment represented by pSegKey.
   
   /pKeptSeg/  pointer to the NODE that was the last red segment we
               went through.  Arguably, we should track two of these:
	       one for keepers and one for background segment logic.
	       At the present time we do not do so.

   /traverseCount/  the number of traversals we have done on this
               path.  At the moment this is not properly tracked if we
	       use the short-circuited fast walk.
   
   /canWrite/  whether the path traversed already conveyed write
               permission.
   /canCall/   whether the path traversed already allowed keeper
               invocation.
   
   /faultCode/ the fault code, if any, associated with this
               traversal.

   In addition, the following members of the WalkInfo structure are
   actually arguments passed from above.  They are stored in the
   WalkInfo structure because they do not change across the several
   calls:
   
   /writeAccess/  whether the translation is for writing
   /prompt/       whether we are willing to take a keeper fault to
                  get a translation.


   I have temporarily removed the support for capability pages to get
   one thing right at a time.
*/

INLINE uint64_t
BLSS_MASK64(uint32_t blss, uint32_t frameBits)
{
  uint32_t bits_to_shift =
    (blss - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + frameBits; 

  uint64_t mask = (1ull << bits_to_shift);
  return mask - 1ull;
}

#ifdef OPTION_DDB
extern bool ddb_segwalk_debug;
#endif

void
segwalk_init(SegWalk *wi /*@ not null @*/, Key *pSegKey)
{
  wi->pSegKey = pSegKey;
  wi->segBlss = EROS_ADDRESS_BLSS + 1;	/* until proven otherwise */
  wi->offset = wi->vaddr;
  wi->redSeg = 0;
  wi->redSegOffset = 0;
  wi->redSpanBlss = 0;
  wi->segObjIsWrapper = false;
  wi->canCall = true;		/* until proven otherwise */
  wi->canWrite = true;		/* until proven otherwise */
  wi->canCache = true;		/* until proven otherwise */
  wi->canFullFetch = true;	/* until proven otherwise */
}


#ifdef OPTION_DDB
#define WALK_DBG_MSG(prefix) \
  if (ddb_segwalk_debug) dprintf(true, prefix \
      " WalkSeg: wi.producer 0x%x, wi.segBlss %d wi.isWrap %d\n" \
      "wi.vaddr 0x%x wi.offset 0x%X flt %d  wa %d segKey 0x%x\n" \
      "canCall %d canWrite %d stopBlss %d\n" \
      "redSeg 0x%x redSpanBlss %d redOffset 0x%X\n", \
      wi->segObj, wi->segBlss, wi->segObjIsWrapper, \
      wi->vaddr, wi->offset, wi->faultCode, wi->writeAccess, wi->pSegKey, \
      wi->canCall, wi->canWrite, stopBlss, \
      wi->redSeg, wi->redSpanBlss, wi->redSegOffset)
#else
#define WALK_DBG_MSG(prefix)
#endif


#ifdef SEPARATE_NODE_HANDLER
#error This is not the case.
static fixreg_t
walkseg_handle_node_key(Process * p, SegWalk* wi, uint32_t stopBlss,
			PTE* pPTE0, PTE* pPTE1, bool canMerge)
{
  Node* segNode = 0;
  uint8_t keyType = keyBits_GetType(wi->pSegKey);

  uint32_t parentBlss = wi->segBlss;
	      
  wi->segBlss = keyBits_GetBlss(wi->pSegKey);
  if (wi->segBlss > MAX_RED_BLSS)
    return FC_SegMalformed;
	    
  /* This is NOT the same as the corresponding /if/ on many
   * systems -- the code generated by this expression contains
   * no branch.
   */
  wi->canWrite = BOOL(wi->canWrite && !keyBits_IsReadOnly(wi->pSegKey));
    
  wi->canFullFetch = BOOL(wi->canFullFetch && !keyBits_IsWeak(wi->pSegKey));
    
  if (!wi->canWrite && wi->writeAccess)
    return FC_Access;
	
  wi->canCall = BOOL(wi->canCall && !keyBits_IsNoCall(wi->pSegKey));
	
  wi->segObj = wi->pSegKey->u.ok.pObj;
  wi->segObjIsWrapper = false;

  if ( keyType == KKT_Node || keyType == KKT_Segment ) {
    if (wi->segBlss == EROS_PAGE_BLSS && wi->wantLeafNode) {
      WALK_DBG_MSG("lfnode");
      return FC_NoFault;
    }
	    
    segNode = (Node *) wi->segObj;
            

    if (node_PrepAsSegment(segNode) == false)
      return FC_SegMalformed;

  }
  else if ( keyType == KKT_Wrapper ) {
    Key *pFormatKey;

    segNode = (Node *) wi->segObj;
	  
    if (node_PrepAsSegment(segNode) == false)
      return FC_SegMalformed;
          
    pFormatKey = &segNode->slot[WrapperFormat];

    ADD_DEPEND(pFormatKey);

    wi->segBlss = WRAPPER_GET_BLSS(pFormatKey->u.nk);

    /* Note that we don't bother to extract the keeper key
     * location until we need it.
     */
    wi->segObjIsWrapper = true;
    wi->redSeg = segNode;
    wi->redSegOffset = wi->offset;
    wi->redSpanBlss = parentBlss - 1;

    WALK_DBG_MSG("wrapper");
  }

  ADD_DEPEND(wi->pSegKey);

  /* Loop around again. */
  return FC_NoFault;
}
#endif

/* Returns true if successful, false if wi->faultCode set. */
/* May Yield. */
bool
WalkSeg(SegWalk* wi /*@ not null @*/, uint32_t stopBlss,
	     void * pPTE, int mapLevel)
{
  const uint32_t MAX_SEG_DEPTH = 20;
  Key *pFormatKey = 0;
  KernStats.nWalkSeg++;


  if (wi->segObj == 0)
    goto process_key;
  
  for(;;) {
    /* If it's the leaf object and we wanted a node, it better be seg
     * key type. Conversely, if we *didn't* want a node, it better NOT
     * be seg key type. All non-page, non-node cases should already
     * have been filtered out in the key processing loop below as
     * malformed segments. */
    if (wi->segBlss == EROS_PAGE_BLSS &&
	BOOL(wi->wantLeafNode) != BOOL(keyBits_IsSegKeyType(wi->pSegKey)))
      goto seg_malformed;

    if (wi->segBlss <= stopBlss) {
      WALK_DBG_MSG("ret");
      return true;
    }
      
    KernStats.nWalkLoop++;

    /*********************************
     * 
     * BEGIN Traversal step logic:
     * 
     *********************************/

    {
      Node *segNode = 0;
      uint32_t initialSlots;
      uint32_t shiftAmt;
      uint32_t ndx;
      
      WALK_DBG_MSG("wlk");

#ifndef NDEBUG
      if (wi->segObj->obType == ot_NtUnprepared)
	fatal("Some node unprepare did not work at kern_Segment.cxx:%d\n",
		      __LINE__);
#endif
    
      segNode = (Node *) wi->segObj;
	
      assertex ( segNode,
		 segNode->node_ObjHdr.obType == ot_NtSegment );
	
      initialSlots = EROS_NODE_SLOT_MASK;

      if (wi->segObjIsWrapper) {
	assertex (&wi, segNode->node_ObjHdr.obType == ot_NtSegment);
	pFormatKey = &segNode->slot[WrapperFormat];

	ADD_DEPEND(pFormatKey);

	initialSlots = 0;
      }

      wi->traverseCount++;	

      shiftAmt = (wi->segBlss - EROS_PAGE_BLSS - 1) * EROS_NODE_LGSIZE
                 + wi->frameBits;
      ndx = wi->offset >> shiftAmt;

      if (ndx > initialSlots)
	goto invalid_addr;

      wi->offset &= (1ull << shiftAmt) - 1ull;

      wi->pSegKey = &segNode->slot[ndx];
    }
    /******************************
     * 
     * END Traversal step logic:
     * 
     ******************************/

    if (wi->traverseCount >= MAX_SEG_DEPTH)
      goto bad_seg_depth;

  process_key:

    /********************************
     * 
     * BEGIN key processing logic:
     * 
     ********************************/

    {
      uint8_t keyType;
#ifdef WALK_DBG
      if (wi.segObj == 0)
        WALK_DBG_MSG("key");
#endif

      key_Prepare(wi->pSegKey);

    
      keyType = keyBits_GetType(wi->pSegKey);
    
      /* Process everything but the per-object processing first: */
      switch(keyType) {
      case KKT_Void:
	goto invalid_addr;
#ifdef KKT_TimePage
#error This is not the case.
      case KKT_TimePage:
	{
	  wi.canWrite = false;
	  wi.canCall = false;
	  wi.segBlss = EROS_PAGE_BLSS;

	  wi.segObj = SysTimer::TimePageHdr;
	  wi.segObjIsWrapper = false;
      
	  if (wi.writeAccess)
	    goto access_fault;

	  ADD_DEPEND(wi.pSegKey);

	  return true;
	}
#endif

      case KKT_Segment:
      case KKT_Node:
      case KKT_Wrapper:
      case KKT_Page:
#ifdef SEPARATE_NODE_HANDLER
#error This is not the case.
	{
	  fixreg_t result = 
	    walkseg_handle_node_key(p, wi, stopBlss,
				    pPTE0, pPTE1, canMerge);
	  if (result != FC_NoFault) {
	    wi->faultCode = result;
	    goto fault_exit;
	  }
	  break;
	}
#else
	{
	  Node* segNode = 0;

	  uint32_t parentBlss = wi->segBlss;
	      
	  wi->segBlss = keyBits_GetBlss(wi->pSegKey);
	  if (wi->segBlss > MAX_RED_BLSS)
	    goto seg_malformed;
	    
	  /* This is NOT the same as the corresponding /if/ on many
	   * systems -- the code generated by this expression contains
	   * no branch.
	   */
	  wi->canWrite = BOOL(wi->canWrite && !keyBits_IsReadOnly(wi->pSegKey));
    
	  wi->canFullFetch = BOOL(wi->canFullFetch && !keyBits_IsWeak(wi->pSegKey));
    
	  if (!wi->canWrite && wi->writeAccess)
	    goto access_fault;
	
	  wi->canCall = BOOL(wi->canCall && !keyBits_IsNoCall(wi->pSegKey));
	
	  wi->segObj = wi->pSegKey->u.ok.pObj;
	  wi->segObjIsWrapper = false;

	  if ( keyType == KKT_Node || keyType == KKT_Segment ) {
	    if (wi->segBlss == EROS_PAGE_BLSS && wi->wantLeafNode) {
	      WALK_DBG_MSG("lfnode");
	      continue;
	    }
	    
	    segNode = (Node *) wi->segObj;
            

	    if (node_PrepAsSegment(segNode) == false)
	      goto seg_thru_process;

	  }
	  else if ( keyType == KKT_Wrapper ) {
	    segNode = (Node *) wi->segObj;
	  
	    if (node_PrepAsSegment(segNode) == false)
	      goto seg_malformed;
          
	    pFormatKey = &segNode->slot[WrapperFormat];

	    ADD_DEPEND(pFormatKey);

	    wi->segBlss = WRAPPER_GET_BLSS(pFormatKey->u.nk);

	    /* Note that we don't bother to extract the keeper key
	     * location until we need it.
	     */
	    wi->segObjIsWrapper = true;
	    wi->redSeg = segNode;
	    wi->redSegOffset = wi->offset;
	    wi->redSpanBlss = parentBlss - 1;

	    WALK_DBG_MSG("wrapper");
	  }

	  ADD_DEPEND(wi->pSegKey);

	  /* Loop around again. */
	  break;
	}
#endif

      case KKT_Number:
	{
	  uint32_t controlWord = wi->pSegKey->u.nk.value[2];

	  /* Window key of some sort. At this point:
	   *
	   *    wi->pSegKey points to the number key
	   *    wi->segObj  points to the containing node
	   *    wi->offset contains the offset within the subsegment
	   *            dominated by this window key
	   *    wi->segBlss contains the segBlss of wi->segObj (our
	   *            parent) 
	   *
	   * Our strategy is to pick out the correct key to process
	   * through (the windowed segment key), whack the wi struct to
	   * point to that key, and perform a GOTO to /process_key/
	   * above to reprocess this stage of the translation.
	   *
	   * Note that any wi->offset value that is legal for this
	   * window is likewise legal to attempt to pass in to the
	   * base segment.
	   */

	  switch(controlWord & EROS_NODE_SLOT_MASK) {
	  case 00:		/* local window key */
	    {
	      /* segNode will point to our containing node */
	      Node* segNode = (Node *) wi->segObj;
	      uint64_t bgSlot = 
		(controlWord >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK;
	      Key *bgSegKey = &segNode->slot[bgSlot];

	      uint64_t addr = wi->pSegKey->u.nk.value[1];
	      addr <<= 32;
	      addr = wi->pSegKey->u.nk.value[0];

	      /* Check: make sure that the window offset is an
	       * appropriate size multiple:
	       */
	      if ((addr & BLSS_MASK64(wi->segBlss-1, wi->frameBits)) != 0)
		goto seg_malformed;

	      printf("Adding 0x%08x to 0x%08x in window redirect\n",
		     addr, wi->offset);

	      /* adjust wi->offset to be an offset into the background
		 segment: */
	      wi->offset += addr;
	      /* Adjust wi->pSegKey to point to the background
		 segment. */
	      wi->pSegKey = bgSegKey;
	      /* Reprocess via the background key. */
	      goto process_key;
	    }
	  default:
	    goto invalid_addr;
	  }
	}
	break;
	
      default:
	goto seg_malformed;
      }
    }
    /********************************
     * 
     * END key processing logic:
     * 
     ********************************/

    /* Loop around again. */
  }

#ifndef SEPARATE_NODE_HANDLER
 access_fault:
  wi->faultCode = FC_Access;
  goto fault_exit;
#endif

 invalid_addr:
  wi->faultCode = FC_InvalidAddr;
  goto fault_exit;

 bad_seg_depth:
  wi->faultCode = FC_SegDepth;
  goto fault_exit;

#ifndef SEPARATE_NODE_HANDLER
 seg_thru_process:
#endif
 seg_malformed:
  wi->faultCode = FC_SegMalformed;

 fault_exit:
  WALK_DBG_MSG("flt");
  return false;
}

/* May Yield. */
void
proc_InvokeSegmentKeeper(Process* thisPtr, SegWalk* wi /*@ not null @*/)
{
  Key *keeperKey = 0;
  Key *pFormatKey = 0;
  uint32_t keptBlss = 0;
  bool sendNode = false;
  Key *keyToPass = 0;
  
  if (wi->redSeg && wi->canCall) {
    /* We know this key is valid or /wi.redSeg/ would not have been set. */
    assert(wi->redSeg->node_ObjHdr.obType == ot_NtSegment);

    pFormatKey = &wi->redSeg->slot[WrapperFormat];

    if (pFormatKey->u.nk.value[0] & WRAPPER_BLOCKED) {
      keyBits_SetWrHazard(pFormatKey);

      act_SleepOn(ObjectStallQueueFromObHdr(&wi->redSeg->node_ObjHdr));
      act_Yield();
    }

    if (pFormatKey->u.nk.value[0] & WRAPPER_KEEPER)
      keeperKey = &wi->redSeg->slot[WrapperKeeper];

    keptBlss = WRAPPER_GET_BLSS(pFormatKey->u.nk);
    sendNode = BOOL(pFormatKey->u.nk.value[0] & WRAPPER_SEND_NODE);

    if (keeperKey && keyBits_IsType(keeperKey, KKT_Start) == false) {
      /* THIS IS VALID, in the sense that it serves to suppress
       * reporting to the process keeper.
       */
      keeperKey = &key_VoidKey;
    }
  }
  
  if (keeperKey == 0 && BOOL(wi->invokeProcessKeeperOK) == false)
    return;

  /* Ensure retry on yield.  All segment faults are fast-path
   * restartable.
   */

  proc_SetFault(thisPtr, wi->faultCode, wi->vaddr, false);


  if (keeperKey == 0) {
    /* Yielding here will cause the thread to resume with a non-zero
     * fault code in the scheduler, at which point it will be shunted
     * off to the process keeper.
     */

    /* Thread::Current() changed to act_Current() */
    act_Yield();

  }

  assert(keeperKey);

  /* If this was a memory fault, clear the PF_Faulted bit.  This
   * allows us to restart the instruction properly without invoking
   * the process keeper in the event that we are forced to yield in the
   * middle of preparing the keeper key or calling InvokeMyKeeper;
   */
  thisPtr->processFlags &= ~PF_Faulted;
  
  /* The only policy decision to make here is whether to pass a key
   * argument.
   */
  keyToPass = &key_VoidKey;
  
  if (sendNode) {
    /* This could be a call driven by SetupExitString(), in which the
     * original invocation was to a red segment key with the node
     * passing option turned on, so do not reuse scratchKey.
     */

    keyBits_InitType(&inv.redNodeKey, KKT_Node);
    keyBits_SetPrepared(&inv.redNodeKey);
    objH_TransLock(DOWNCAST(wi->redSeg, ObjectHeader));
    inv.redNodeKey.u.ok.pObj = DOWNCAST(wi->redSeg, ObjectHeader);

    link_Init(&inv.redNodeKey.u.ok.kr);
    link_insertAfter(&wi->redSeg->node_ObjHdr.keyRing, &inv.redNodeKey.u.ok.kr);
    inv.flags |= INV_REDNODEKEY;

    keyToPass = &inv.redNodeKey;
  }

  /* NOTE DELICATE ISSUE
   * 
   *   If the keeper is a resume key we must mark the containing
   *   segment node dirty.  See the comments in kern_Invoke.cxx for an
   *   explanation.
   * 
   *   In practice, if you use a resume key in a keeper slot you are a
   *   bogon anyway, so I'm not real worried about it...
   * 
   *   The following may yield, but note that all segment faults are
   *   restartable, so it's okay.  There is no need for a similar
   *   check in InvokeProcessKeeper(), because process root is always
   *   dirty.
   */

  if (keyBits_IsType(keeperKey, KKT_Resume))
    node_MakeDirty(wi->redSeg);

#ifdef OPTION_DDB
  if (ddb_segwalk_debug) {
    static uint64_t last_offset = 0x0ll;
    if (wi->redSegOffset != last_offset) {
      dprintf(true, "Invoking segment keeper. redseg 0x%x"
		      " base 0x%X last 0x%X\n",
		      wi->redSeg, wi->redSegOffset, last_offset);
      last_offset = wi->redSegOffset;
    }
  }
#endif

  proc_InvokeMyKeeper(thisPtr, OC_SEGFAULT, wi->faultCode,
		 (uint32_t) wi->redSegOffset, (uint32_t) (wi->redSegOffset>>32),
		 keeperKey, keyToPass,
		 0, 0);

  /* NOTREACHED - this seems wrong */
}
