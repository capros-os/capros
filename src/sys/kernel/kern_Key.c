/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
#include <kerninc/Check.h>
#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <kerninc/Node.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/KernStats.h>

#define dbg_prepare	0x1	/* steps in taking snapshot */

/* Following should be an OR of some of the above */
#define dbg_flags   (0)

#define DBCOND(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if DBCOND(x)
#define DEBUG2(x,y) if ((dbg_##x|dbg_##y) & dbg_flags)

#ifndef NDEBUG
extern bool InvocationCommitted;
#endif

ObjectHeader *
key_GetObjectPtr(const Key* thisPtr)
{
  assert( keyBits_IsPreparedObjectKey(thisPtr) );
  if (keyBits_IsGateKey(thisPtr))
    return DOWNCAST(thisPtr->u.gk.pContext->procRoot, ObjectHeader);
  else
    return thisPtr->u.ok.pObj;
}

OID
key_GetKeyOid(const Key* thisPtr)
{
  assert (keyBits_IsObjectKey(thisPtr));
  
  if ( keyBits_IsPreparedObjectKey(thisPtr) )
    return key_GetObjectPtr(thisPtr)->oid;
  return thisPtr->u.unprep.oid;
}

uint32_t
key_GetAllocCount(const Key* thisPtr)
{
  assert (keyBits_GetType(thisPtr) > LAST_GATE_KEYTYPE);
  assert (keyBits_GetType(thisPtr) <= LAST_OBJECT_KEYTYPE);
  
  if ( keyBits_IsPreparedObjectKey(thisPtr) )
    return key_GetObjectPtr(thisPtr)->allocCount;
  return thisPtr->u.unprep.count;
}

Key key_VoidKey;	/* default constructor */

/* NOTE: if we are running OB_MOD_CHECK, the key prepare logic does an
 * incremental recomputation on the check field in the containing object.
 */
/* May Yield. */
void
key_DoPrepare(Key* thisPtr)
{
  ObjectHeader *pObj = 0;
  assert( keyBits_IsUnprepared(thisPtr) );
  
  assert ( keyBits_NeedsPrepare(thisPtr) );

  KernStats.nKeyPrep++;
  
#ifdef TEST_STACK
  StackTester st;
#endif
 
#if 0
  if (ktByte > UNPREPARED(LAST_PREPARED_KEYTYPE))
    return;
#endif

#ifdef MEM_OB_CHECK
  uint32_t ck = thisPtr->CalcCheck();
#endif
  
  switch(keyBits_GetType(thisPtr)) {		/* not prepared, so not hazarded! */
  case KKT_Resume:
  case KKT_Start:
    {
      /* Gate keys are linked onto the context structure.  First, we
       * need to validate the key:
       */

      Process * context = 0;
      Node *pNode = (Node *) 
	objC_GetObject(thisPtr->u.unprep.oid, ot_NtUnprepared,
                       thisPtr->u.unprep.count,
                       keyBits_IsType(thisPtr, KKT_Resume) ? false : true);

      if (keyBits_IsType(thisPtr, KKT_Resume) && pNode->callCount != thisPtr->u.unprep.count)
	pNode = 0;

      if (pNode == 0) {
	DEBUG(prepare)
	  printf("Voiding invalid gate key\n");
	assert ( keyBits_IsHazard(thisPtr) == false );
	assert ( keyBits_IsUnprepared(thisPtr) );
	/* key was not prepared, so cannot be hazarded */
	key_NH_RescindKey(thisPtr);
	return;
      }
	
      assertex(pNode, objC_ValidNodePtr(pNode));
      
      objH_TransLock(DOWNCAST(pNode, ObjectHeader));

      context = node_GetDomainContext(pNode);
      if (! context )		/* malformed domain */
	fatal("Preparing gate key to malformed domain\n");

      /* Okay, we prepared successfully. */
      thisPtr->u.gk.pContext = context;

      /* Link into context chain on left or right according to key
       * type.
       */
      if ( keyBits_IsType(thisPtr, KKT_Resume) )
	link_insertBefore(&context->keyRing, &thisPtr->u.gk.kr);
      else
	link_insertAfter(&context->keyRing, &thisPtr->u.gk.kr);

      keyBits_SetPrepared(thisPtr);

#if 0
      printf("Prepared key ");
      Print();
#endif
  
#ifdef MEM_OB_CHECK
      assert(ck == thisPtr->CalcCheck());
#endif
    
#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr)
	check_Consistency("In Key::DoPrepare()");
#endif
#ifdef TEST_STACK
      st.check();
#endif
      return;
    }

  case KKT_Page:
    {
      pObj = objC_GetObject(thisPtr->u.unprep.oid, ot_PtDataPage, 
                            thisPtr->u.unprep.count, true);
      if (pObj)
	assertex(thisPtr, objC_ValidPagePtr(pObj));
      break;
    }
    
  case KKT_Node:
  case KKT_Segment:
  case KKT_Process:
  case KKT_Wrapper:
  case KKT_Forwarder:
  case KKT_GPT:
    pObj = objC_GetObject(thisPtr->u.unprep.oid, ot_NtUnprepared, 
                          thisPtr->u.unprep.count, true);
    break;

  default:
    keyBits_SetPrepared(thisPtr);
    return;
  }
  
  if (pObj == 0) {
    DEBUG(prepare)
      dprintf(true, "Voiding invalid key\n");

    assert ( keyBits_IsHazard(thisPtr) == false );
    assert ( keyBits_IsUnprepared(thisPtr) );
    key_NH_RescindKey(thisPtr);
#ifdef TEST_STACK
    st.check();
#endif
    return;
  }

  /* It's definitely an object key.  Pin the object it names. */
  objH_TransLock(pObj);
  
  /* Link as next key after object */
  thisPtr->u.ok.pObj = pObj;
  
  link_insertAfter(&pObj->keyRing, &thisPtr->u.ok.kr);

#ifdef MEM_OB_CHECK
  assert(ck == thisPtr->CalcCheck());
#endif
    
  keyBits_SetPrepared(thisPtr);
#if 0
  printf("Prepared key ");
  Print();
#endif
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("End Key::DoPrepare()");
#endif
#ifdef TEST_STACK
  st.check();
#endif
  return;
}

#ifndef NDEBUG
/* May Yield. */
void
key_Prepare(Key* thisPtr)
{
  assert (InvocationCommitted == false);

  assert(thisPtr);

  if (keyBits_IsUnprepared(thisPtr))
    key_DoPrepare(thisPtr);
      
  if ( keyBits_NeedsPin(thisPtr) )
    objH_TransLock(thisPtr->u.ok.pObj);
}
#endif

void
key_NH_Set(KeyBits *thisPtr, KeyBits* kb)
{
  /* Skip copy if src == dest (surprisingly often!) */
  if (kb == thisPtr)
    return;
    
#ifdef __KERNEL__
  keyBits_Unchain(thisPtr);
#endif

  thisPtr->keyType = kb->keyType;
  thisPtr->keyFlags = kb->keyFlags & ~KFL_HAZARD_BITS;
  thisPtr->keyPerms = kb->keyPerms;
  thisPtr->keyData = kb->keyData;

  thisPtr->u.nk.value[0] = kb->u.nk.value[0];
  thisPtr->u.nk.value[1] = kb->u.nk.value[1];
  thisPtr->u.nk.value[2] = kb->u.nk.value[2];

  /* Update the linkages if destination is now prepared: */
  if ( keyBits_IsPreparedObjectKey(thisPtr) ) {
#if 0
    link_Init(&thisPtr->u.ok.kr);
    link_InsertBefore(&kb->u.ok.kr, &thisPtr->u.ok.kr);
#else
    thisPtr->u.ok.kr.prev->next = &thisPtr->u.ok.kr;
    thisPtr->u.ok.kr.next = &kb->u.ok.kr;
    kb->u.ok.kr.prev = &thisPtr->u.ok.kr;
#endif
  }
}

void
key_SetToNumber(KeyBits *thisPtr, uint32_t hi, uint32_t mid, uint32_t lo)
{
#ifdef __KERNEL__
  keyBits_Unchain(thisPtr);
#endif

  keyBits_InitType(thisPtr, KKT_Number);
  keyBits_SetUnprepared(thisPtr);
  keyBits_SetReadOnly(thisPtr);
  thisPtr->u.nk.value[0] = lo;
  thisPtr->u.nk.value[1] = mid;
  thisPtr->u.nk.value[2] = hi;
}

#define CAP_REGMOVE
#ifdef CAP_REGMOVE
/* save_key(from, to) -- given a key address /from/ that is within a
 * capability register, copy that key to the key address /to/, which
 * is a slot in a capability page.
 * 
 * Note that all functions called by save_key() are prompt and do not
 * yield.  This is important, as we are running on an interrupt stack.
 */

void copy_key(Key *fromKeyReg, Key *toKeyReg);
void xchg_key(Key *kr0, Key *kr1);


void
copy_key(Key *from, Key *to)
{
  if (to == &act_CurContext()->keyReg[0])
    return;
  
  /* Do not bother to deprepare dest key, as it isn't going to disk as
   * a result of this operation.
   */
  key_NH_Set(to, from);
}

void
xchg_key(Key *cr0, Key *cr1)
{
  Key tmp;

  keyBits_InitToVoid(&tmp);
  key_NH_Set(&tmp, cr0);

  if (cr0 != &act_CurContext()->keyReg[0]) {
    key_NH_Set(cr0, cr1);
  }
  if (cr1 != &act_CurContext()->keyReg[0]) {
    key_NH_Set(cr1, &tmp);
  }
  
  key_NH_Unchain(&tmp);
}

#endif /* CAP_REGMOVE */

void
key_NH_Unprepare(Key* thisPtr)
{
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top Key::NH_Unprepare()");
#endif

  /* fatal("Unprepare() called\n"); */
  assert(keyBits_IsHazard(thisPtr) == false);

  if ( keyBits_IsUnprepared(thisPtr) )
    return;

  if ( keyBits_IsObjectKey(thisPtr) ) {
    ObjectHeader *pObj = thisPtr->u.ok.pObj;
  
    if (keyBits_IsGateKey(thisPtr) ) {
#ifndef NDEBUG
      if (ValidCtxtPtr(thisPtr->u.gk.pContext) == false)
	fatal("Key 0x%08x Kt %d, 0x%08x not valid ctxt ptr\n",
              thisPtr, keyBits_GetType(thisPtr), thisPtr->u.gk.pContext);
#endif
      pObj = DOWNCAST(thisPtr->u.gk.pContext->procRoot, ObjectHeader);
    }

#ifndef NDEBUG
    if ( keyBits_IsType(thisPtr, KKT_Page) ) {
      if ( objC_ValidPagePtr(pObj) == false )
	fatal("Key 0x%08x Kt %d, 0x%08x not valid page ptr\n",
		      thisPtr, keyBits_GetType(thisPtr), pObj);
    }
    else {
      if ( objC_ValidNodePtr((Node *)pObj) == false )
	fatal("Key 0x%08x Kt %d, 0x%08x not valid node ptr\n",
		      thisPtr, keyBits_GetType(thisPtr), pObj);
    }
#endif

    key_NH_Unchain(thisPtr);

    objH_SetFlags(pObj, OFLG_DISKCAPS);
    thisPtr->u.unprep.oid = pObj->oid;

    if ( keyBits_IsType(thisPtr, KKT_Resume) )
      thisPtr->u.unprep.count = ((Node *) pObj)->callCount;
    else
      thisPtr->u.unprep.count = pObj->allocCount;
  }

  keyBits_SetUnprepared(thisPtr);

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("End Key::NH_Unprepare()");
#endif

#if 0
  printf("Deprepared key ");
  Print();
#endif
}

void
key_Print(const Key* thisPtr)
{
  uint32_t * pWKey = (uint32_t *) thisPtr;

  if ( keyBits_IsPreparedObjectKey(thisPtr) ) {
    ObjectHeader * pObj = key_GetObjectPtr(thisPtr);
    
    uint32_t * pOID = (uint32_t *) &pObj->oid;

    if (keyBits_IsType(thisPtr, KKT_Resume)) {
#if 0
      printf("rsm 0x%08x 0x%08x 0x%08x 0x%08x (obj=0x%08x)\n",
		     pWKey[0], ((Node *)pObj)->callCount,
		     pOID[0], pOID[1], pObj);
      
#else
      printf("0x%08x rsm 0x%08x 0x%08x 0x%08x 0x%08x\n",
		     thisPtr,
		     pWKey[0], objH_ToNode(pObj)->callCount,
		     pOID[0], pOID[1]);
#endif
    }
    else {
#if 0
      printf("pob 0x%08x 0x%08x 0x%08x 0x%08x (obj=0x%08x)\n",
		     pWKey[0], ok.pObj->allocCount,
		     pOID[0], pOID[1], pObj);
#else
      printf("0x%08x pob 0x%08x 0x%08x 0x%08x 0x%08x\n",
		     thisPtr,
		     pWKey[0], pObj->allocCount,
		     pOID[0], pOID[1]);
#endif
    }
  }
  else {
#if 0
    printf("ukt 0x%08x 0x%08x 0x%08x 0x%08x\n",
		   pWKey[0], pWKey[1], pWKey[2], pWKey[3]);
#else
    printf("0x%08x ukt 0x%08x 0x%08x 0x%08x 0x%08x\n",
		   thisPtr,
		   pWKey[0], pWKey[1], pWKey[2], pWKey[3]);
#endif
  }
}

#ifdef OPTION_OB_MOD_CHECK
uint32_t
key_CalcCheck(Key* thisPtr)
{
  uint32_t *pWKey = 0;
  uint32_t ck = 0;

  assert(thisPtr);

#if 1
  /* Portable version: */
  {
    int i;
    Key k;			/* temporary in case send and receive */
				/* slots are the same. */
        
    keyBits_InitToVoid(&k);
    key_NH_Set(&k, thisPtr);
    key_NH_Unprepare(&k);

    assert(k.keyFlags == 0);
    
    pWKey = (uint32_t *) &k;

    for (i = 0; i < (sizeof(Key) / sizeof(uint32_t)); i++) 
      ck ^= pWKey[i];
  }
#else
  pWKey = (uint32_t *) thisPtr;

  if ( keyBits_IsPreparedObjectKey(thisPtr) ) {
    if (keyBits_IsType(thisPtr, KKT_Resume)) {
      /* Inject the checksum for a void key instead so that
       * zapping them won't change the checksum:
       */

      ck = key_CalcCheck(&key_VoidKey);
    }
    else if (keyBits_IsType(thisPtr, KKT_Start)) {
      /* This pointer hack is simply so that I don't have to remember
       * machine specific layout conventions for long long
       */
    
      Node *pDomain = thisPtr->u.gk.pContext->procRoot;
      uint32_t *pOID = (uint32_t *) &pDomain->node_ObjHdr.oid;
      ck ^= pOID[0];
      ck ^= pOID[1];

      ck ^= pDomain->node_ObjHdr.allocCount;
    }
    else {
      /* This pointer hack is simply so that I don't have to remember
       * machine specific layout conventions for long long
       */
      uint32_t *pOID = (uint32_t *) &thisPtr->u.ok.pObj->oid;
    
      ck ^= pOID[0];
      ck ^= pOID[1];
      ck ^= thisPtr->u.ok.pObj->allocCount;
    }
  }
  else {
    ck ^= pWKey[0];
    ck ^= pWKey[1];
    ck ^= pWKey[2];
  }

  /* mask out prepared, hazard bits! */
  {
    uint8_t saveKeyFlags = thisPtr->keyFlags;
    thisPtr->keyFlags &= ~KFL_ALL_FLAG_BITS;

    ck ^= pWKey[3];
  
    thisPtr->keyFlags = saveKeyFlags;
  }
#endif

  return ck;
}
#endif

/* New Key -- make it void, since in a couple of cases we do this on
 * the stack and there is no telling what garbage bits are sitting there.
 */


#ifndef NDEBUG
bool
key_IsValid(const Key* thisPtr)
{
  if ( keyBits_IsMiscKey(thisPtr) ) {
    if (thisPtr->u.nk.value[0] || thisPtr->u.nk.value[1] || thisPtr->u.nk.value[2])
      return false;
  }

#if defined(DBG_WILD_PTR)
  /* Following is a debugging-only check. */
  if (keyBits_IsObjectKey(thisPtr) && key_GetKeyOid(thisPtr) > 0x100000000llu) {
    OID oid = key_GetKeyOid(thisPtr);
    
    printf("Key 0x%08x has invalid OID 0x%08x%08x\n",
		   thisPtr, (uint32_t) (oid>>32), (uint32_t) oid);
  }
#endif
      
  if ( keyBits_IsPreparedObjectKey(thisPtr) ) {
#ifndef NDEBUG
    if ( keyBits_IsGateKey(thisPtr) ) {
      Process *ctxt = thisPtr->u.gk.pContext;
      if (ValidCtxtPtr(ctxt) == false)
	return false;
    }
    else if ( keyBits_IsType(thisPtr, KKT_Page) ) {
      ObjectHeader *pObject = thisPtr->u.ok.pObj;
      if ( objC_ValidPagePtr(pObject) == false ) {
	key_Print(thisPtr);
	printf("Key 0x%08x has invalid pObject 0x%08x\n",
		       thisPtr, pObject);
	return false;
      }
      if (pObject->obType == ot_PtFreeFrame) {
	key_Print(thisPtr);
	printf("Prepared key 0x%08x names free pObject 0x%08x\n",
		       thisPtr, pObject);
	return false;
      }
    }
    else {
      Node *pNode = 0;
      assertex (thisPtr, keyBits_IsObjectKey(thisPtr) );
      pNode = (Node *) thisPtr->u.ok.pObj;
      if ( objC_ValidNodePtr(pNode) == false ) {
	printf("0x%x is not a valid node ptr\n", pNode);
	return false;
      }
      if (pNode->node_ObjHdr.obType == ot_NtFreeFrame) {
	key_Print(thisPtr);
	printf("Prepared key 0x%08x names free pObject 0x%08x\n",
		       thisPtr, pNode);
	return false;
      }
    }
#endif

    if ( keyBits_IsObjectKey(thisPtr) ) {
      /* KeyRing pointers must either point to key slots or to
       * object root.
       */
#ifndef NDEBUG
      KeyRing * krn = thisPtr->u.ok.kr.next;
      KeyRing * krp = thisPtr->u.ok.kr.prev;

      if ( ! ( objC_ValidKeyPtr((Key *) krn) ||
	       objC_ValidPagePtr(keyR_ToObj(krn)) ||
	       objC_ValidNodePtr(objH_ToNode(keyR_ToObj(krn))) ||
	       ValidCtxtKeyRingPtr(krn) ) ) {
	printf("key 0x%x nxt key 0x%x bogus\n", thisPtr, krn);
	return false;
      }
      
      if ( ! ( objC_ValidKeyPtr((Key *) krp) ||
	       objC_ValidPagePtr(keyR_ToObj(krp)) ||
	       objC_ValidNodePtr(objH_ToNode(keyR_ToObj(krp))) ||
	       ValidCtxtKeyRingPtr(krp) ) ) {
	printf("key 0x%x prv key 0x%x bogus\n", thisPtr, krp);
	return false;
      }

      /* Prev and next pointers must be linked properly: */
      if (krp->next != (KeyRing *) thisPtr) {
	printf("Prepared key 0x%08x bad keyring links to prev\n",
		       thisPtr);
	return false;
      }
      if (krn->prev != (KeyRing *) thisPtr) {
	printf("Prepared key 0x%08x bad keyring links to next\n",
		       thisPtr);
	return false;
      }
#endif
    }
  }
  return true;
}
#endif
