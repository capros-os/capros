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

#include <kerninc/kernel.h>
#include <kerninc/KernStats.h>
#include <kerninc/Process.h>
#include <kerninc/GPT.h>
#include <kerninc/Depend.h>
#include <kerninc/Activity.h>
#include <kerninc/Invocation.h>
#include <arch-kerninc/PTE.h>
#include <idl/capros/GPT.h>
#include <eros/ProcessState.h>

// #define WALK_DBG

#define dbg_keeper	0x8

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#ifdef OPTION_DDB
#define WALK_DBG_MSG(prefix) \
  if (ddb_segwalk_debug) \
    dprintf(true, prefix \
      " WalkSeg: wi.memObj 0x%x, \n" \
      "wi.offset 0x%X flt %d  nw %d \n" \
      "restrictions %x \n", \
      wi->memObj, \
      wi->offset, wi->faultCode, wi->needWrite, \
      wi->restrictions)
#else
#define WALK_DBG_MSG(prefix)
#endif

/* WalkSeg() is a performance-critical path.  The segment walk
   logic is complex, and it occurs in the middle of page faulting
   activity, which is performance-critical.

   The depth of the walk is bounded by MAX_SEG_DEPTH to detect
   possible loops in the segment tree.  Because the routine is
   multiply entered the actual depth limit can be two or three times
   the nominal limit.  The architecture guarantees a minimum traversal
   depth, but does not promise any particular maximum.
*/

#ifdef OPTION_DDB
extern bool ddb_segwalk_debug;
#endif

/* If successful, returns true,
   otherwise returns false and sets wi->faultCode. */
// Caller must call Depend_AddKey.
// May Yield.
static bool
processKey(SegWalk * wi, Key * pSegKey, uint64_t va)
{
#ifdef WALK_DBG
  WALK_DBG_MSG("key");
#endif

  key_Prepare(pSegKey);

  switch (keyBits_GetType(pSegKey)) {
  case KKT_GPT:
    if (! node_PrepAsSegment(objH_ToNode(pSegKey->u.ok.pObj))) {
      /* This can only happen if a holder of a range key uses a node
      as both a GPT and something else. */
      wi->faultCode = capros_Process_FC_MalformedSpace;
      return false;
    }
    break;

  case KKT_Page:
#ifdef KKT_TimePage
  case KKT_TimePage:
#endif
    // nothing to do
    break;

  case KKT_Void:
    wi->faultCode = capros_Process_FC_InvalidAddr;
    return false;

  default:
    wi->faultCode = capros_Process_FC_MalformedSpace;
    return false;
  }

  // Check the guard.
  unsigned int l2g = keyBits_GetL2g(pSegKey);
#if 0
  printf("l2g %d, guard 0x%x\n", l2g, keyBits_GetGuard(pSegKey));
#endif
  /* Shift va in two steps, because a shift of 64 is undefined. */
  if ((va >> (l2g -1) >> 1) != keyBits_GetGuard(pSegKey)) {
    wi->faultCode = capros_Process_FC_InvalidAddr;
    return false;
  }
  // Strip off guard bits.
  // Shift by (l2g-1), because a shift of 64 is undefined.
  va &= (2ull << (l2g-1)) -1;
  wi->offset = va;

  wi->memObj = pSegKey->u.ok.pObj;

  wi->restrictions |= pSegKey->keyPerms;
  if ((wi->restrictions & capros_Memory_readOnly)
      && wi->needWrite) {
    wi->faultCode = capros_Process_FC_AccessViolation;
    return false;
  }

  return true;
}

/* If successful, returns true,
   otherwise returns false and sets wi->faultCode. */
// May Yield.
bool
segwalk_init(SegWalk * wi, Key * pSegKey, uint64_t va,
             void * pPTE, int mapLevel)
{
  wi->restrictions = 0;
  wi->backgroundGPT = 0;
  wi->keeperGPT = 0;
  if (! processKey(wi, pSegKey, va))
    return false;

  Depend_AddKey(pSegKey, pPTE, mapLevel);
  return true;
}


/* Walk a segment as described in wi until one of the following
 * occurs:
 * 
 *     You find an object with l2v < stopL2v (walk succeeded, return true)
 *     You conclude that the desired type of access as described in
 *        ISWRITE cannot be satisfied in this segment (walk failed,
 *        return false and set wii->faultCode)
 * 
 * At each stage in the walk, add dependency entries between the
 * traversed slots and the page table entries described by pPTE and mapLevel.
 * 
 * This routine is designed to be called repeatedly and cache its
 * intervening state in the SegWalk structure so that the walk
 * does not need to be repeated.
 */
/* May Yield. */
bool
WalkSeg(SegWalk * wi, uint32_t stopL2v,
	     void * pPTE, int mapLevel)
{
  const uint32_t MAX_SEG_DEPTH = 20;
  KernStats.nWalkSeg++;

  WALK_DBG_MSG("WalkSeg");

  for(;;) {
    if (wi->memObj->obType > ot_NtLAST_NODE_TYPE) {
      // It's a page, not a GPT.
      WALK_DBG_MSG("ret");
      return true;
    }

    GPT * gpt = objH_ToNode(wi->memObj);

    assert(gpt->node_ObjHdr.obType == ot_NtSegment);

    uint8_t l2vField = gpt_GetL2vField(gpt);
    unsigned int curL2v = l2vField & GPT_L2V_MASK;
    assert(curL2v >= EROS_PAGE_LGSIZE);

    if (curL2v < stopL2v) {
      WALK_DBG_MSG("ret");
      return true;
    }
      
    KernStats.nWalkLoop++;

    if (++ wi->traverseCount >= MAX_SEG_DEPTH) {
      wi->faultCode = capros_Process_FC_TraverseLimit;
      goto fault_exit;
    }
      
    WALK_DBG_MSG("wlk");
	
    unsigned int maxSlot;

    if (l2vField & GPT_KEEPER) {	// it has a keeper
      maxSlot = capros_GPT_keeperSlot -1;
      if (wi->keeperGPT != SEGWALK_GPT_UNKNOWN	// nocall is valid
          && ! (wi->restrictions & capros_Memory_noCall)	// can call it
          ) {
        wi->keeperGPT = gpt;
        wi->keeperOffset = wi->offset;
      }
    }
    else maxSlot = capros_GPT_nSlots -1;

    if (l2vField & GPT_BACKGROUND) {
      wi->backgroundGPT = gpt;
      maxSlot = capros_GPT_backgroundSlot -1;
        /* backgroundSlot < keeperSlot, so this must come last. */
    }

    uint64_t ndx = wi->offset >> curL2v;
    if (ndx > maxSlot) {
      wi->faultCode = capros_Process_FC_InvalidAddr;
      goto fault_exit;
    }

    wi->offset &= (1ull << curL2v) - 1ull;	// remaining bits of address

    Key * k = node_GetKeyAtSlot(gpt, ndx);

    if (keyBits_GetType(k) == KKT_Number) {
      // A window key.
      uint64_t addr = k->u.nk.value[1];
      addr <<= 32;
      addr |= k->u.nk.value[0];

      /* Check: make sure that the window offset is an
       * appropriate size multiple: */
      if (addr & ((1ull << curL2v) -1))
	goto seg_malformed;

      wi->offset += addr;

      uint32_t controlWord = k->u.nk.value[2];
      unsigned int wrestrictions = (controlWord >> 8) & 0xff;

      wi->restrictions |= wrestrictions;
      if ((wi->restrictions & capros_Memory_readOnly)
          && wi->needWrite) {
        wi->faultCode = capros_Process_FC_AccessViolation;
        goto fault_exit;
      }

      Depend_AddKey(k, pPTE, mapLevel);

      unsigned int wslot = controlWord & 0xff;

      if (wslot < capros_GPT_nSlots) {
        // A local window key.
	k = &gpt->slot[wslot];
      }
      else if (wslot == 0xff) {
        // A background window key.
        if (! wi->backgroundGPT)
	  goto seg_malformed;	// there is no background key

        // Background GPT had better have a background key:
        assert(gpt_GetL2vField(wi->backgroundGPT) & GPT_BACKGROUND);

        k = &wi->backgroundGPT->slot[capros_GPT_backgroundSlot];
      }
      else {
	goto seg_malformed;
      }
    }

    if (! processKey(wi, k, wi->offset)) {
      goto fault_exit;
    }

    Depend_AddKey(k, pPTE, mapLevel);
    /* Loop around again. */
  }
  assert(false);	// can't get here

seg_malformed:
  wi->faultCode = capros_Process_FC_MalformedSpace;
fault_exit:
  WALK_DBG_MSG("flt");
  return false;
}

/* May Yield. */
void
proc_InvokeSegmentKeeper(
  Process * thisPtr,	// the process that is to invoke the keeper
	/* (This might be different from act_CurContext(), if in the future
	we allow a CALL on a gate key to cause the callee to handle
	page faults.) */
  SegWalk * wi,
  bool invokeProcessKeeperOK,
  uva_t vaddr )
{
  Key *keeperKey = 0;
  
#ifdef WALK_DBG
  WALK_DBG_MSG("calling seg keeper");
#endif

  if (wi->keeperGPT == SEGWALK_GPT_UNKNOWN) {
    // re-walk from the top to find the keeper if any
    DEBUG (keeper) printf("Rewalking\n");

    wi->traverseCount = 0;
    if (segwalk_init(wi, node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace),
                     vaddr, 0, 0)) {
      WalkSeg(wi, EROS_PAGE_LGSIZE, 0, 0);
    }
    assert(wi->keeperGPT != SEGWALK_GPT_UNKNOWN);
  }

  DEBUG (keeper) dprintf(true, "calling seg keeper, wi=0x%x\n", wi);

  assert(wi->faultCode);

  // FIXME: noCall restriction might not be right if path was truncated.
  if (wi->keeperGPT) {
    // It has a keeper we can call. 
    DEBUG (keeper) printf("Has keeperGPT\n");

    assert(wi->keeperGPT->node_ObjHdr.obType == ot_NtSegment);
    assert(gpt_GetL2vField(wi->keeperGPT) & GPT_KEEPER);

    keeperKey = &wi->keeperGPT->slot[capros_GPT_keeperSlot];

    /* Save the faultCode and faultInfo that will be passed to the
    process keeper if the segment keeper rejects the fault. */
    thisPtr->faultCode = wi->faultCode;
    thisPtr->faultInfo = vaddr;
    /* Do not set the capros_Process_PF_FaultToProcessKeeper bit, because for now this is going
    to the segment keeper not the process keeper. */

#ifdef OPTION_DDB
    if (thisPtr->processFlags & PF_DDBTRAP)
      dprintf(true, "Process 0x%08x faulting to seg keeper\n", thisPtr);
#endif
  
    // Set up to send a key to the keeper.
    /* This could be a call driven by SetupExitString(), in which the
     * original invocation was to a red segment key with the node
     * passing option turned on, so do not reuse scratchKey.
     */

    keyBits_InitType(&inv.redNodeKey, KKT_GPT);
    // GPT key has no restrictions.
    keyBits_SetL2g(&inv.redNodeKey, 64);	// disable guard
    keyBits_SetPrepared(&inv.redNodeKey);
    objH_TransLock(node_ToObj(wi->keeperGPT));
    inv.redNodeKey.u.ok.pObj = node_ToObj(wi->keeperGPT);

    link_Init(&inv.redNodeKey.u.ok.kr);
    link_insertAfter(&wi->keeperGPT->node_ObjHdr.keyRing,
                     &inv.redNodeKey.u.ok.kr);
    inv.flags |= INV_REDNODEKEY;

    proc_InvokeMyKeeper(thisPtr, OC_SEGFAULT, wi->faultCode,
		 (uint32_t) wi->keeperOffset, (uint32_t) (wi->keeperOffset>>32),
		 keeperKey, &inv.redNodeKey,
		 0, 0);
  }
  else {		// no segment keeper
    DEBUG (keeper) dprintf(true, "No seg keeper, %s\n",
               invokeProcessKeeperOK ? "try proc kpr" : "dont try proc kpr");

    if (! invokeProcessKeeperOK)
      return;	// no segment keeper and can't invoke process keeper

    proc_SetFault(thisPtr, wi->faultCode, vaddr);

    /* Yielding here will cause the thread to resume with a non-zero
     * fault code in the scheduler, at which point it will be shunted
     * off to the process keeper.
     */
    act_Yield();
  }
}
