/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/KeyRing.h>
#include <kerninc/Check.h>
#include <kerninc/Key.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Invocation.h>
#include <kerninc/Depend.h>
#include <kerninc/Key-inline.h>
#include <kerninc/ObjH-inline.h>

#ifndef NDEBUG
// This is like keyR_IsValid, but only requires one parameter.
static bool
keyR_IsValid1(const KeyRing * thisPtr)
{
  const void * pObj;
  if (ValidCtxtKeyRingPtr(thisPtr)) {
    pObj = keyR_ToProc((KeyRing *)thisPtr);
  } else {
    pObj = keyR_ToObj((KeyRing *)thisPtr);
  }
  return keyR_IsValid(thisPtr, pObj);
}
#endif

/* Clear any conditions causing keys in this KeyRing to be
   write hazarded.
   Does NOT clear if the key is also read-hazarded.
   This will clear all keys involved in memory mapping. */
void
keyR_ClearWriteHazard(KeyRing * thisPtr)
{
  KeyRing * cur = thisPtr->next;
  for (cur = thisPtr->next;
       cur != thisPtr;
       cur = cur->next) {
    Key * pKey = (Key *) cur;
    if ( keyBits_IsWrHazard(pKey)
         && ! keyBits_IsRdHazard(pKey) ) {
      /* By excluding read-hazarded keys, we ensure that clearing the
         hazard will not disturb the chain. */
      key_ClearHazard(pKey);
    }
  }
}

void
keyR_RescindAll(KeyRing * thisPtr)
{
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top RescindAll");
#endif

#ifndef NDEBUG
  if (! keyR_IsValid1(thisPtr))
    dprintf(true, "Keyring 0x%08x is invalid\n");
#endif
  
  while (thisPtr->next != thisPtr) {
    Key *pKey = (Key *) thisPtr->next;

#ifndef NDEBUG
    if (! key_ValidKeyPtr(pKey)) {
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
    KeyRing * nxt = pKey->u.ok.kr.next;
#endif

    if ( keyBits_IsHazard(pKey) ) {
      key_ClearHazard(pKey);

      /* Having cleared the hazard, the key we are presently examining 
	 may not even be on this ring any more -- it may be a
	 different key entirely.  We therefore restart the loop. */
      continue;
    }

    assert ( keyBits_IsPreparedObjectKey(pKey) );

    /* Note that we do NOT mark the containing object dirty.
    If it is clean, the copy on disk has an allocation count,
    the target object's allocation count is being incremented,
    and when the copy is read from disk it will be noted to be rescinded. */
    key_NH_SetToVoid(pKey);

    assert (nxt == thisPtr->next);

#ifdef OPTION_OB_MOD_CHECK
    Node * pNode = node_ContainingNodeIfNodeKeyPtr(pKey);
    if (pNode) {
      node_SetCheck(pNode);
    }
#endif
  }
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom RescindAll()");
#endif
}

void
keyR_ObjectMoved(KeyRing * keyR, ObjectHeader * newObj)
{
  Link * prev = keyR;
  while (prev->next != keyR) {
    Key * pKey = (Key *) prev->next;

    assert(keyBits_IsPrepared(pKey));

    if (keyBits_IsHazard(pKey)) {
      key_ClearHazard(pKey);

      /* Having cleared the hazard, the key we are presently examining 
	 may not even be on this ring any more -- it may be a
	 different key entirely.  We therefore check prev again. */
      if (! keyBits_IsPrepared(pKey))
	continue;
    }
    pKey->u.ok.pObj = newObj;

    prev = &pKey->u.ok.kr;
    
#ifdef OPTION_OB_MOD_CHECK
    Node * pNode = node_ContainingNodeIfNodeKeyPtr(pKey);
    if (pNode) {
      node_SetCheck(pNode);
    }
#endif
  }

  // Now move the whole chain to the new object.
  Link * next = keyR->next;
  prev = keyR->prev;
  keyR_ResetRing(keyR);
  newObj->keyRing.next = next;
  newObj->keyRing.prev = prev;
  next->prev = &newObj->keyRing;
  prev->next = &newObj->keyRing;
}

#ifndef NDEBUG
bool
keyR_IsValid(const KeyRing *thisPtr, const void *pObj)
{
  const KeyRing *cur = thisPtr;
  const KeyRing *nxt = thisPtr->next;
  
  while (nxt != thisPtr) {
    assert(nxt);
    Key * const nxtKey = (Key *)nxt;

    if (! key_ValidKeyPtr(nxtKey)) {
      dprintf(true, "Key 0x%08x is not valid in keyring 0x%08x\n",
		    nxt, thisPtr);
      return false;
    }

    if ( keyBits_IsUnprepared(nxtKey))  {
      dprintf(true, "Key 0x%08x is unprepared in keyring 0x%08x (cur 0x%08x)\n",
		    nxt, thisPtr, cur);
      return false;
    }
    
    if (! keyBits_IsPreparedObjectKey(nxtKey)) {
      dprintf(true, "Key 0x%08x is not object key in keyring 0x%08x\n",
		    nxt, thisPtr);
      return false;
    }
    
    if (nxt->prev != cur) {
      dprintf(true, "Prev of key 0x%08x is not cur in keyring 0x%08x (cur=0x%08x\n",
		    nxt, thisPtr, cur);
      return false;
    }
    
    if (nxtKey->u.ok.pObj != pObj) {
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
  Key *pKey = (Key *) thisPtr->prev;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top ZapResumeKeys");
#endif

  /* Prepared resume keys are never in an activity, so we do not need to
     check for that. FIXME: We should have a resume key in an Activity
     for processes that are sleeping on SleepQueue; this is necessary to
     handle the case of Send to the sleep key, passing a Resume key to
     the process to be awakened later, then invoking another Resume key
     to the process before the timer expires. 
     The Resume key in the Activity should be hazarded to avoid
     slowing the fast path.

     Prepared resume keys cksum as zero, so we do not need to
   * recompute the checksum.
   */
  
  while ( thisPtr->prev != thisPtr && keyBits_IsPreparedResumeKey(pKey) ) {
    // There are no cases in which a resume key can be hazarded:
    assert(! keyBits_IsHazard(pKey));
    
    thisPtr->prev = pKey->u.ok.kr.prev;

    /* Note that we do NOT mark the containing object dirty.  If the object is
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
  if (! keyR_IsValid1(thisPtr))
    dprintf(true, "Keyring 0x%08x is invalid\n", thisPtr);
#endif
  
  while (thisPtr->next != thisPtr) {
    Key *pKey = (Key *) thisPtr->next;

#ifndef NDEBUG
    if (! key_ValidKeyPtr(pKey)) {
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
    KeyRing * nxt = pKey->u.ok.kr.next;
#endif
#if 0
    printf("UnprepareAll kr=0x%08x pKey=0x%08x *pKey=0x%08x\n",
		   this, pKey, *((Word*) pKey)); 
#endif
    
    if ( keyBits_IsHazard(pKey) ) {
      key_ClearHazard(pKey);

      /* Having cleared the hazard, the key we are presently examining 
	 may not even be on this ring any more -- it may be a
	 different key entirely.  We therefore restart the loop. */
      continue;
    }
    else {
      assert ( keyBits_IsPreparedObjectKey(pKey) );

      /* If we are unpreparing a activity-resident key, and that activity
       * has a context pointer, the key might be out of date.  It is
       * perfectly okay to deprepare that key PROVIDED that we do not
       * alter the activity's context pointer.  This is safe because of
       * the sequence of conditions tested by Activity::IsRunnable() and
       * Activity::Prepare().
       */

#ifndef NDEBUG
      if (pKey->u.ok.kr.next != thisPtr
	  && ! key_ValidKeyPtr((Key*)pKey->u.ok.kr.next) )
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
    Node * pNode = node_ContainingNodeIfNodeKeyPtr(pKey);
    if (pNode) {
      node_SetCheck(pNode);
    }
#endif
  }

  assert ( keyR_IsEmpty(thisPtr) );
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom UnprepareAll()");
#endif
}

/* Find all keys on this KeyRing that could have Depend entries,
 * and call func for each one.
 * This object must be a page or node (not a Process). */
void
keyR_ProcessAllMaps(KeyRing * thisPtr,
  void (*func)(KeyDependEntry *))
{
  Key * pKey = (Key *) thisPtr->next;

#if 0
  printf("UnmapAll kr=0x%08x pKey=0x%08x\n", thisPtr, pKey); 
#endif
    
#ifndef NDEBUG
  if (! keyR_IsValid(thisPtr, keyR_ToObj(thisPtr)))
    dprintf(true, "Keyring 0x%08x is invalid\n", thisPtr);
#endif
  
  while ((KeyRing *)pKey != thisPtr) {
    assert(key_ValidKeyPtr(pKey));
    assert(keyBits_IsPreparedObjectKey(pKey));
    
    if (keyBits_IsHazard(pKey)) {
      Node * pNode = node_ContainingNode(pKey);
      switch (pNode->node_ObjHdr.obType) {
      case ot_NtProcessRoot:
        if ((pKey - pNode->slot) == ProcAddrSpace) {
      case ot_NtSegment:
          Depend_VisitEntries(pKey, func);

          if (func == &KeyDependEntry_Invalidate)	// if invalidating,
            keyBits_UnHazard(pKey);	// key is no longer hazarded
        }
        break;
      default:
        break;
      }
    }
    /* Following works fine for gate keys too - representation pun! */
    pKey = (Key *)pKey->u.ok.kr.next;
  }

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom UnmapAll()");
#endif
}
