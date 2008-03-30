/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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
#include <kerninc/Node.h>
#include <disk/DiskNodeStruct.h>
#include <kerninc/util.h>
#include <kerninc/Check.h>
#include <kerninc/Process.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/Activity.h>
#include <kerninc/Depend.h>
#include <kerninc/Invocation.h>
#include <kerninc/IRQ.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/GPT.h>

#define PREPDEBUG

#include <eros/Invoke.h>

void
node_ClearHazard(Node* thisPtr, uint32_t ndx)
{
  if (keyBits_IsHazard(&thisPtr->slot[ndx]) == false)
    return;

  switch(thisPtr->node_ObjHdr.obType) {
  case ot_NtUnprepared:
    fatal("Unprepared Node 0x%08x%08x Corrupted (slot %d).\n",
	    (uint32_t) (thisPtr->node_ObjHdr.oid>>32), 
	    (uint32_t) thisPtr->node_ObjHdr.oid, ndx);
    break;
    
  case ot_NtSegment:
    if ( keyBits_IsRdHazard(&thisPtr->slot[ndx]) )
      fatal("Segment Node Corrupted!\n");

    node_ClearGPTHazard(thisPtr, ndx);
    break;

  case ot_NtKeyRegs:
    proc_FlushKeyRegs(thisPtr->node_ObjHdr.prep_u.context);
    break;
      
  case ot_NtProcessRoot:
    /* TRY to Flush just the registers back out of the context
     * structure to clear the write hazard.  That is the common case,
     * and the less we flush the happier we will be:
     */

    if (ndx == ProcAddrSpace) {
      Depend_InvalidateKey(&thisPtr->slot[ndx]);
    }
    else if ( ndx == ProcGenKeys ) {
      proc_FlushKeyRegs(thisPtr->node_ObjHdr.prep_u.context);
    }
    else {
      /* FIX: If the slot is not smashed with a number key... */
      proc_FlushProcessSlot(thisPtr->node_ObjHdr.prep_u.context, ndx);
    }
    break;
  default:
    fatal("Clear hazard on unknown type\n");
    break;
  }

  if (keyBits_IsHazard(&thisPtr->slot[ndx]) != false)
    fatal("Error. Node ty=%d slot=%d still hazarded\n",
		  thisPtr->node_ObjHdr.obType, ndx);
}
     
void
node_ClearAllHazards(Node * thisPtr)
{
  unsigned int k;
  for (k = 0; k < EROS_NODE_SIZE; k++) {
    node_ClearHazard(thisPtr, k);
  }
}

// Caller must make the node dirty.
/* CAREFUL -- this operation can have the side effect of blowing away
 * the current thread!
 */
void
node_DoClearThisNode(Node* thisPtr)
{
  uint32_t k = 0;

  assert (InvocationCommitted);
  
  node_ClearAllHazards(thisPtr);

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    assert (keyBits_IsHazard(&thisPtr->slot[k]) == false); /* node is unprepared! */
    key_NH_SetToVoid(&thisPtr->slot[k]);
  }
  thisPtr->nodeData = 0;

  /* FIX: Not sure this is really necessary, but I think it is a good
   * idea.
   */
  objH_InvalidateProducts(DOWNCAST(thisPtr, ObjectHeader));

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("DoClearThisNode");
#endif
}

/* UNPREPARE AND RESCIND (and object relocation) OF HAZARDED SLOTS.
 * 
 * The catch is that we don't want to unload the current (or invokee)
 * contexts unnecessarily.  If this slot contains a key that chances
 * to be in a capability register of the current (invokee) context,
 * clearing the hazard will have the effect of rendering the current
 * (invokee) context unrunnable by virtue of having forced an unload
 * of the capability registers node.
 * 
 * If the capability that we are depreparing/rescinding is in a memory
 * context, then we really do need to clear the hazard in order to
 * force the corresponding PTEs to get invalidated.  If, however, the
 * capability that we are clearing is in a NtKeyRegs node, we can
 * cheat.  Ultimately, this capability will get overwritten anyway,
 * and if the capability in the actual context structure is still the
 * same as this one, it will be getting unprepared/rescinded as well
 * as we traverse the key ring.
 * 
 * Note that the last is true only because this function is called
 * ONLY from within KeyRing::RescindAll or KeyRing::UnprepareAll
 * (according to operation).
 */
void
node_UnprepareHazardedSlot(Node* thisPtr, uint32_t ndx)
{
  Key* key /*@ not null @*/ = &thisPtr->slot[ndx];
  
  assert(keyBits_IsPrepared(key));
  
  if (thisPtr->node_ObjHdr.obType == ot_NtKeyRegs) {
    uint8_t oflags = key->keyFlags;

    /* TEMPORARILY clear the hazard: */
    key->keyFlags &= ~KFL_HAZARD_BITS;

    key_NH_Unprepare(key);
    
    /* RESET the hazard: */
    key->keyFlags = oflags;
  }
  else {
    node_ClearHazard(thisPtr, ndx);
    
    key_NH_Unprepare(key);
  }

#ifdef OPTION_OB_MOD_CHECK
  if (!objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)))
    thisPtr->node_ObjHdr.check = objH_CalcCheck(DOWNCAST(thisPtr, ObjectHeader));
#endif
}

/* NOTE: Off the top of my head, I can think of no reason why this
 * capability should NOT be invalidated in place.  The mustUnprepare flag
 * gets set when the object in question already has on-disk
 * capabilities, but what relevance this carries for in-place
 * capabilities is decidedly NOT clear to me.
 */
void 
node_RescindHazardedSlot(Node* thisPtr, uint32_t ndx, bool mustUnprepare)
{
  Key* key /*@ not null @*/ = &thisPtr->slot[ndx];
  
  assert(keyBits_IsPrepared(key));
  
  if (thisPtr->node_ObjHdr.obType == ot_NtKeyRegs) {
    uint8_t oflags = key->keyFlags;

    /* TEMPORARILY clear the hazard: */
    key->keyFlags &= ~KFL_HAZARD_BITS;

    if (mustUnprepare)
      key_NH_Unprepare(key);
    else
      key_NH_RescindKey(key);
    
    /* RESET the hazard: */
    key->keyFlags = oflags;
  }
  else {
    node_ClearHazard(thisPtr, ndx);

    if (mustUnprepare)
      key_NH_Unprepare(key);
    else
      key_NH_RescindKey(key);
  }
  
#ifdef OPTION_OB_MOD_CHECK
  if (!objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)))
    thisPtr->node_ObjHdr.check = objH_CalcCheck(DOWNCAST(thisPtr, ObjectHeader));
#endif
}

#if 0
void
Node::ObMovedHazardedSlot(uint32_t ndx, ObjectHeader *pNewLoc)
{
  Key& key = slot[ndx];
  
  assert(key.IsPrepared());
  
  ClearHazard(ndx);

  key.ok.pObj = pNewLoc;

#ifdef OPTION_OB_MOD_CHECK
  if (!IsDirty())
    ob.check = CalcCheck();
#endif
}
#endif

static void
node_DoPrepareProcess(Node * pNode)
{
  assert(pNode);

  /* If it is prepared as anything else, the space bank or process creator
  messed up. */
  assert(pNode->node_ObjHdr.obType == ot_NtUnprepared);

  /* We mark the domain components dirty as a side effect.
   * Strictly speaking, this is not necessary, but the number of domains
   * that get prepared without modification closely approximates 0, and
   * it's easier to do it here than in the context prepare logic.  Also,
   * doing the marking here makes it unnecessary to test in the message
   * transfer path.
   */

  node_MakeDirty(pNode);

  Process * p = proc_allocate(true);

  p->procRoot = pNode;

  pNode->node_ObjHdr.prep_u.context = p; 
  pNode->node_ObjHdr.obType = ot_NtProcessRoot;
}

Process *
node_GetProcess(Node * pNode)
{
  if (pNode->node_ObjHdr.obType != ot_NtProcessRoot)
    node_DoPrepareProcess(pNode);

  assert(pNode->node_ObjHdr.prep_u.context);

  return pNode->node_ObjHdr.prep_u.context;
}

/* The domain preparation logic has several possible outcomes:
 * 
 * 1. It succeeds - all is well, and a node pointer is returned.
 * 
 * 2. It must be retried because I/O is required, in which case it
 * initiates the I/O on behalf of the calling thread and causes a new
 * thread to be scheduled, resulting in an unwind.
 * 
 * 3. It fails because the domain is malformed.  In this case, it
 * endeavours to run the keeper of the domain that could not be
 * prepared, if any.  There may not (transitively) be one to run, in
 * which case we end up with a thread occupying a busted domain, Stick
 * the thread on the busted domain stall queue, and force a
 * reschedule.
 */

void
node_SetSlot(Node * thisPtr, uint32_t slot, Invocation * inv)
{
  node_MakeDirty(thisPtr);

  COMMIT_POINT();
  
  /* Following will not cause dirty node because we forced it
   * dirty above the commit point.
   */
  node_ClearHazard(thisPtr, slot);

  key_NH_Set(node_GetKeyAtSlot(thisPtr, slot), inv->entry.key[0]);

  inv->exit.code = RC_OK;
}

bool
node_PrepAsSegment(Node* thisPtr)
{
  assert(thisPtr);
  if (thisPtr->node_ObjHdr.obType == ot_NtSegment)
    return true;

  /* If it is prepared as anything else, the space bank or process creator
  messed up. */
  assert(thisPtr->node_ObjHdr.obType == ot_NtUnprepared);
	  
  thisPtr->node_ObjHdr.obType = ot_NtSegment;
  thisPtr->node_ObjHdr.prep_u.products = 0;

  return true;
}

inline bool
node_IsCurrentDomain(Node* thisPtr)
{
  /* Note proc_Current() may be NULL.  If we are trying to
   * prepare a new thread to run (ageing can be called from trying
   * to prepare a thread, which can call us), the current thread may
   * indeed not have a context.
   */

  if (thisPtr->node_ObjHdr.prep_u.context == proc_Current())
    return true;

  return false;
}

bool
node_Unprepare(Node* thisPtr, bool zapMe)
{
  if (thisPtr->node_ObjHdr.obType == ot_NtUnprepared)
    return true;

#ifndef NDEBUG
  unsigned int originalObType = thisPtr->node_ObjHdr.obType;
  assert(originalObType <= ot_NtLAST_NODE_TYPE);
  Process * originalContext = thisPtr->node_ObjHdr.prep_u.context;
#endif

  if (thisPtr->node_ObjHdr.obType == ot_NtProcessRoot) {
    /* First check to make sure we don't deprepare ourselves if we
     * shouldn't.
     */

    if (zapMe == false && node_IsCurrentDomain(thisPtr)) {
      dprintf(true, "(0x%08x) Domroot 0x%08x%08x no zapme\n",
              thisPtr,
              (uint32_t) (thisPtr->node_ObjHdr.oid >> 32), 
              (uint32_t) thisPtr->node_ObjHdr.oid);
      return false;
    }

    proc_Unload(thisPtr->node_ObjHdr.prep_u.context);
  }
  else if (thisPtr->node_ObjHdr.obType == ot_NtKeyRegs || thisPtr->node_ObjHdr.obType == ot_NtRegAnnex) {
    /* First check to make sure we don't deprepare ourselves if we
     * shouldn't.
     */
#if 1

    if (inv_IsActive(&inv)
        && thisPtr->node_ObjHdr.prep_u.context == inv.invokee) {
      dprintf(true, "zapping keys/annex of invokee nd=0x%08x"
		" ctxt=0x%08x\n", thisPtr, thisPtr->node_ObjHdr.prep_u.context);
    }

#endif

    if (zapMe == false && node_IsCurrentDomain(thisPtr)) {
      dprintf(true, "(0x%08x) keys/annex 0x%08x%08x no zapme\n",
		      thisPtr,
		      (uint32_t) (thisPtr->node_ObjHdr.oid >> 32),
                      (uint32_t) thisPtr->node_ObjHdr.oid);
      return false;
    }

    proc_Unload(thisPtr->node_ObjHdr.prep_u.context);
  }
  else if (thisPtr->node_ObjHdr.obType == ot_NtSegment) {
    GPT_Unload(thisPtr);
  }

#ifndef NDEBUG
  // All slots should now be unhazarded.
  unsigned int k;
  for (k = 0; k < EROS_NODE_SIZE; k++) {
    if (keyBits_IsHazard(node_GetKeyAtSlot(thisPtr, k))) {
      dprintf(true, "Hazard after unprepared! slot %d, ot=%d, ctxt=0x%08x\n",
              k, originalObType, originalContext);
    }
  }
#endif

  thisPtr->node_ObjHdr.obType = ot_NtUnprepared;
  return true;
}

bool
node_Validate(Node* thisPtr)
{
  uint32_t i = 0;
  uint32_t k = 0;

  if ( thisPtr->node_ObjHdr.obType > ot_NtLAST_NODE_TYPE) {
    printf("Node 0x%08x has bad object type\n", thisPtr);
    return false;
  }
  assert (thisPtr->node_ObjHdr.obType <= ot_NtLAST_NODE_TYPE);
  
  if (thisPtr->node_ObjHdr.obType == ot_NtFreeFrame) {
    for (i = 0; i < EROS_NODE_SIZE; i++) {
      if (keyBits_IsUnprepared(&thisPtr->slot[i]) == false) {
	dprintf(true, "Free node 0x%08x has prepared slot %d\n",
			thisPtr, i);
	return false;
      }
    }

    return true;
  }
  
#ifndef NDEBUG
  if (keyR_IsValid(&thisPtr->node_ObjHdr.keyRing, thisPtr) == false)
    return false;
#endif
  
#ifdef OPTION_OB_MOD_CHECK
  if (objH_IsDirty(DOWNCAST(thisPtr, ObjectHeader)) == false) {
    uint32_t chk = objH_CalcCheck(DOWNCAST(thisPtr, ObjectHeader));
  
    if ( thisPtr->node_ObjHdr.check != chk ) {
      printf("Invalid Frame 0x%08x Chk=0x%x CalcCheck=0x%x on node ",
		     thisPtr, thisPtr->node_ObjHdr.check, chk);
      printOid(thisPtr->node_ObjHdr.oid);
      printf("\n");

#if 0
      for (uint32_t i = 0; i < EROS_NODE_SIZE; i++)
	slot[i].Print();
#endif

      return false;
    }
  }  
#endif

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    /* const Key& key = thisPtr->slot[k]; */
    Key* key /*@ not null @*/ = &thisPtr->slot[k];

#ifndef NDEBUG
    if (key_IsValid(key) == false) {
      printf("Key %d is bad in node 0x%x\n", k, thisPtr);
      key_Print(key);
      return false;
    }
#endif
    
    if (keyBits_IsHazard(key) &&
	thisPtr->node_ObjHdr.obType == ot_NtUnprepared) {
      printf("Unprepared node contains hazarded key\n");
      return false;
    }

    /* For now, do not check device keys. */
    if ( keyBits_IsObjectKey(key) == false )
      continue;
  }

  return true;
}
