/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/KeyRing.h>
#include <kerninc/Check.h>
#include <kerninc/Key.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Invocation.h>

static inline bool
proc_IsKeyReg(const Key * pKey)
{
  return IsInProcess(pKey);
}

void
keyR_RescindAll(KeyRing *thisPtr, bool mustUnprepare)
{
#ifndef NDEBUG
  KeyRing *nxt = 0;
#endif
  Node *pNode = 0;
  uint32_t slot = 0;
  Process *pContext = 0;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top RescindAll");
#endif

#ifndef NDEBUG
  if (keyR_IsValid(thisPtr, thisPtr) == false)
    dprintf(true, "Keyring 0x%08x is invalid\n");
#endif
  
  while (thisPtr->next != (KeyRing*) thisPtr) {
    Key *pKey = (Key *) thisPtr->next;

#ifndef NDEBUG
    if ( objC_ValidKeyPtr(pKey)  == false ) {
      key_Print(pKey);
      fatal("Invalid keyring ptr. Ring=0x%08x pKey=0x%08x\n",
		    thisPtr, pKey);
    }

    if ( keyBits_IsPreparedObjectKey(pKey) == false)
      key_Print(pKey);

    assert ( keyBits_IsPreparedObjectKey(pKey) );
#endif
    
#ifndef NDEBUG
    /* Following works fine for gate keys too - representation pun! */
    nxt = pKey->u.ok.kr.next;
#endif

    /* Note that thread-embodied and process-embodied keys are never
     * hazarded:
     */
    if ( keyBits_IsHazard(pKey) ) {
      assert ( act_IsActivityKey(pKey) == false );
      assert ( proc_IsKeyReg(pKey) == false );
      pNode = objC_ContainingNode(pKey);
      assert ( objC_ValidNodePtr(pNode) );
      slot = pKey - pNode->slot;


      node_RescindHazardedSlot(pNode, slot, mustUnprepare);


      /* Having cleared the hazard, the key we are presently examining 
	 may not even be on this ring any more -- it may be a
	 different key entirely.  We therefore restart the loop. */
      if (keyBits_IsUnprepared(pKey))
	continue;
    }
    else {
      assert (keyBits_IsHazard(pKey) == false);
      assert ( keyBits_IsPreparedObjectKey(pKey) );

      /* hazard has been cleared, if any */
      if (mustUnprepare) {
	key_NH_Unprepare(pKey);
      }
      else {
	/* Note that we do NOT mark the object dirty.  If the object is
	 * already dirty, then it will go out to disk with the key
	 * zeroed.  Otherwise, the key will be stale when the node is
	 * re-read, and will be zeroed on preparation.
	 */
	key_NH_RescindKey(pKey);
	
	assert (nxt == thisPtr->next);
      }
    }

    /* If the rescinded key is in a activity that is currently running,
     * we cannot delete the activity.  The current domain may be
     * committing sepuku, but it might be the case that the domain has
     * contrived to return to someone else, and the *activity* will
     * therefore survive. [See: there *is* an afterlife!] This can
     * happen when the currently running domain calls "destroy this
     * node and return to fred" where "this node" is the domain root
     * of the currently running domain. Buggered completely, but in
     * principle this can happen.
     * 
     * Rather than test for that case, which is not MP compatible,
     * simply wake up the activity.  If the activity is already awake no
     * harm will ensue.  If the activity was asleep, we will attempt to
     * run it, discover its demise, and that will be the end of it.
     */

    if (act_IsActivityKey(pKey)) {
      Activity *containingActivity = act_ContainingActivity(pKey);
      assert(containingActivity);
      pContext = containingActivity->context;

      if (pContext) {
	/* The key within the activity may not be current.  I learned
	 * the hard way that it is possible for a live activity to have
	 * a prepared key to the thing being destroyed -- it was
	 * triggered by DCC.  This was hard to debug, as printing out
	 * the activity caused the key to be deprepared.
	 */

	/* If the activity key is prepared, then the context is loaded,
	 * which means we can rely on a valid domain root pointer:
	 */
	assert( pContext->procRoot );
	assert ( pContext->procRoot->node_ObjHdr.oid != key_GetKeyOid(pKey) );
	proc_SyncActivity(pContext);
	continue;
      }      
      else
	act_Wakeup(containingActivity);
    }

#ifdef OPTION_OB_MOD_CHECK
    if ( !(inv_IsActive(&inv) && inv_IsInvocationKey(&inv, pKey)) && 
	 !act_IsActivityKey(pKey) &&
	 !proc_IsKeyReg(pKey) ) {
      Node *pNode = objC_ContainingNode(pKey);
      pNode->node_ObjHdr.check = objH_CalcCheck(DOWNCAST(pNode, ObjectHeader));
    }
#endif
  }
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom RescindAll()");
#endif
}

#if 0
/* This is only called from the checkpoint code, and relies utterly on
 * the fact that it is never called for a node.
 */
void
KeyRing::ObjectMoved(struct ObjectHeader *newObj)
{
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top ObjectMoved");
#endif

  /* Algorithm below fails if the ring is empty */
  if (next == (KeyRing*) this)
    return;
  
  KeyRing *nxt = next;
  
  while (nxt != (KeyRing*) this) {
    Key *pKey = (Key *) nxt;

    assert (pKey->GetType() == KKT_Page);

    /* We need to ensure that any outstanding dependencies on the prior
     * location of this page (as occurs in page table entries) will be
     * reconstructed.  Actually, once we need to do this I'm half
     * tempted to just deprepare all of the keys
     */
    
    /* Note that activity-embodied and process-embodied keys are never hazarded: */
    if ( pKey->IsHazard() ) {
      assert ( Activity::IsActivityKey(pKey) == false );
      assert ( Process::IsKeyReg(pKey) == false );
      Node *pNode = ObjectCache::ContainingNode(pKey);
      assert ( ObjectCache::ValidNodePtr(pNode) );
      uint32_t slot = pKey - pNode->slot;

      pNode->ObMovedHazardedSlot(slot);

      /* Having cleared the hazard, the key we are presently examining 
	 may not even be on this ring any more -- it may be a
	 different key entirely.  We therefore restart the loop. */
      if (pKey->IsUnprepared())
	continue;
    }
    else {
      assert (pKey->IsHazard() == false);
      assert ( pKey->IsPrepared() );

      pKey->ok.pObj = newObj;

      nxt = pKey->ok.next;
    }
    
#ifdef OPTION_OB_MOD_CHECK
    if ( !inv.IsInvocationKey(pKey) && 
	 !Activity::IsActivityKey(pKey) &&
	 !Process::IsKeyReg(pKey) ) {
      Node *pNode = ObjectCache::ContainingNode(pKey);
      pNode->ob.check = pNode->CalcCheck();
    }
#endif
  }

  newObj->kr.next = next;
  newObj->kr.prev = prev;
  newObj->kr.next->prev = &newObj->kr;
  newObj->kr.prev->next = &newObj->kr;
    
  ResetRing();
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom ObjectMover()");
#endif
}
#endif

#ifndef NDEBUG
bool
keyR_IsValid(const KeyRing *thisPtr, const void *pObj)
{
  const KeyRing *cur = thisPtr;
  const KeyRing *nxt = thisPtr->next;
  
  while (nxt != thisPtr) {
    assert ( (Key *) nxt );

    if ( ! objC_ValidKeyPtr((Key *) nxt) ) {
      dprintf(true, "Key 0x%08x is not valid in keyring 0x%08x\n",
		    nxt, thisPtr);
      return false;
    }

    if ( keyBits_IsUnprepared((Key*)nxt))  {
      dprintf(true, "Key 0x%08x is unprepared in keyring 0x%08x (cur 0x%08x)\n",
		    nxt, thisPtr, cur);
      return false;
    }
    
    if ( keyBits_IsPreparedObjectKey((Key*)nxt) == false) {
      dprintf(true, "Key 0x%08x is not object key in keyring 0x%08x\n",
		    nxt, thisPtr);
      return false;
    }
    
    if (nxt->prev != cur) {
      dprintf(true, "Prev of key 0x%08x is not cur in keyring 0x%08x (cur=0x%08x\n",
		    nxt, thisPtr, cur);
      return false;
    }
    
    if (cur->next != nxt) {
      dprintf(true, "Next of key 0x%08x is not nxt in keyring 0x%08x\n",
		    nxt, thisPtr);
      return false;
    }

    if (((Key *)nxt)->u.ok.pObj!=pObj) {
      dprintf(false,
		      "nxt->ok.pObj=0x%08x pObj=0x%08x nxt=0x%08x\n",
		      ((Key *)nxt)->u.ok.pObj, pObj, nxt);
      return false;
    }

    cur = nxt;
    nxt = nxt->next;
  }
  return true;
}
#endif

bool
keyR_HasResumeKeys(const KeyRing *thisPtr)
{
  const Key *pKey = (Key *) thisPtr->prev;
  
  if ( keyBits_IsPreparedResumeKey(pKey) )
    return true;
  return false;
}

void
keyR_ZapResumeKeys(KeyRing *thisPtr)
{
  uint8_t oflags = 0;
  Key *pKey = (Key *) thisPtr->prev;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top ZapResumeKeys");
#endif

  /* Prepared resume keys are never in a activity, so we do not need to
   * check for that.  They cksum as zero, so we do not need to
   * recompute the checksum.
   * 
   * It used to be that they were never hazarded, but this is no
   * longer true -- if a resume key is sitting in a key register that
   * gets loaded into a process context it can become hazarded.  Note,
   * however that this is the ONLY case in which a resume key can be
   * hazarded, and in that case we are going to zap the copy in the
   * context anyway, so it is okay to bash them both.  The catch
   * (there is ALWAYS a catch) is that we must preserve the hazard
   * bits correctly.
   */
  
  while ( thisPtr->prev != thisPtr && keyBits_IsPreparedResumeKey(pKey) ) {
#ifndef NDEBUG

    if (keyBits_IsHazard(pKey)) {
      Node *pNode = objC_ContainingNode(pKey);
      assert (pNode->node_ObjHdr.obType == ot_NtKeyRegs);
    }

#endif
    
    oflags = pKey->keyFlags & KFL_HAZARD_BITS;

    thisPtr->prev = pKey->u.ok.kr.prev;

    /* Note that we do NOT mark the object dirty.  If the object is
     * already dirty, then it will go out to disk with the key
     * voided.  Otherwise, the key will be stale when the node is
     * re-read, and will be voided on preparation.
     */

    /* We use keyBits_InitToVoid() here rather than keyBits_NH_SetToVoid()
     * to avoid doing unnecessary unchains -- the whole chain is going
     * away anyway.
     *
     * We will patch the key chain at the bottom of the loop.
     *
     * Note that this may not work on an SMP!
     */
    
    keyBits_InitToVoid(pKey);

    pKey->keyFlags = oflags;

    pKey = (Key *) thisPtr->prev;
  }

  /* The following is a no-op if there is no resume key, and necessary
   * to patch the chain if there is.
   */

  pKey->u.ok.kr.next = (KeyRing*) thisPtr;
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom ZapResumeKeys()");
#endif
}

void
keyR_UnprepareAll(KeyRing *thisPtr)
{
#ifndef NDEBUG
  KeyRing *nxt = 0;
#endif
  Node *pNode = 0;
  uint32_t slot = 0;
#ifndef NDEBUG
  if (keyR_IsValid(thisPtr, thisPtr) == false)
    dprintf(true, "Keyring 0x%08x is invalid\n");
#endif
  
  while (thisPtr->next != (KeyRing*) thisPtr) {
    Key *pKey = (Key *) thisPtr->next;

#ifndef NDEBUG
    if ( objC_ValidKeyPtr(pKey)  == false ) {
      key_Print(pKey);
      fatal("Invalid keyring ptr. Ring=0x%08x pKey=0x%08x\n",
		    thisPtr, pKey);
    }

    if ( keyBits_IsPrepared(pKey) == false)
      key_Print(pKey);

    assert ( keyBits_IsPreparedObjectKey(pKey) );
#endif
    
#ifndef NDEBUG
    /* Following works fine for gate keys too - representation pun! */
    nxt = pKey->u.ok.kr.next;
#endif
#if 0
    printf("UnprepareAll kr=0x%08x pKey=0x%08x *pKey=0x%08x\n",
		   this, pKey, *((Word*) pKey)); 
#endif
    
    if ( keyBits_IsHazard(pKey) ) {
      assert ( act_IsActivityKey(pKey) == false );
      assert ( proc_IsKeyReg(pKey) == false );
      pNode = objC_ContainingNode(pKey);
      assert ( objC_ValidNodePtr(pNode) );
      slot = pKey - pNode->slot;

      node_UnprepareHazardedSlot(pNode, slot);


      /* Having cleared the hazard, the key we are presently examining 
	 may not even be on this ring any more -- it may be a
	 different key entirely.  We therefore restart the loop. */
      if (keyBits_IsUnprepared(pKey))
	continue;
    }
    else {
      assert (keyBits_IsHazard(pKey) == false);
      assert ( keyBits_IsPreparedObjectKey(pKey) );

      /* If we are unpreparing a activity-resident key, and that activity
       * has a context pointer, the key might be out of date.  It is
       * perfectly okay to deprepare that key PROVIDED that we do not
       * alter the activity's context pointer.  This is safe because of
       * the sequence of conditions tested by Activity::IsRunnable() and
       * Activity::Prepare().
       */

#ifndef NDEBUG
      if ( pKey->u.ok.kr.next != thisPtr &&
	   objC_ValidKeyPtr((Key*)pKey->u.ok.kr.next)  == false )
	fatal("keyring 0x%08x: pKey 0x%08x w/ bad next ptr 0x%08x\n",
		      thisPtr, pKey, pKey->u.ok.kr.next);
#endif
      
      key_NH_Unprepare(pKey);
    

#ifndef NDEBUG
      if (nxt != thisPtr->next)
	fatal("Depreparing key 0x%08x.  nxt was 0x%08x next now 0x%08x\n",
		      pKey, nxt, thisPtr->next);
#endif
      assert (nxt == thisPtr->next);
    
      assert ( keyBits_IsUnprepared(pKey) );
    }
    
#ifdef OPTION_OB_MOD_CHECK
    if ( !(inv_IsActive(&inv) && inv_IsInvocationKey(&inv, pKey)) && 
	 !act_IsActivityKey(pKey) &&
	 !proc_IsKeyReg(pKey) ) {
      Node *pNode = objC_ContainingNode(pKey);
      pNode->node_ObjHdr.check = objH_CalcCheck(DOWNCAST(pNode, ObjectHeader));
    }
#endif
  }

  assert ( keyR_IsEmpty(thisPtr) );
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom UnprepareAll()");
#endif
}
