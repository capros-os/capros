/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
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

#include <idl/capros/Range.h>
#include <kerninc/kernel.h>
#include <kerninc/Check.h>
#include <kerninc/Process.h>
#include <kerninc/Activity.h>
#include <kerninc/Node.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/ObjectSource.h>
#include <kerninc/KernStats.h>
#include <kerninc/Invocation.h>
#include <eros/Invoke.h>

#define dbg_prepare	0x1	/* steps in taking snapshot */

/* Following should be an OR of some of the above */
#define dbg_flags   (0)

#define DBCOND(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if DBCOND(x)
#define DEBUG2(x,y) if ((dbg_##x|dbg_##y) & dbg_flags)

Key key_VoidKey;

#ifndef NDEBUG
extern bool InvocationCommitted;
#endif

ObjectHeader *
key_GetObjectPtr(const Key* thisPtr)
{
  assert(keyBits_IsPreparedObjectKey(thisPtr));
  if (keyBits_IsProcessType(thisPtr))
    return node_ToObj(thisPtr->u.gk.pContext->procRoot);
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
    return objH_GetAllocCount(key_GetObjectPtr(thisPtr));
  return thisPtr->u.unprep.count;
}

// May Yield.
bool
CheckObjectType(OID oid, ObjectLocator * pObjLoc, unsigned int baseType)
{
  *pObjLoc = GetObjectType(oid);

  if (pObjLoc->objType == capros_Range_otNone)
    pObjLoc->objType = baseType;	// we can pick the type to suit

  return (pObjLoc->objType == baseType);
}

static void
PrepareKeyToProcess(Key * key, Process * proc)
{
  // Keys to a process are linked onto the Process structure.
  key->u.gk.pContext = proc;

  // Link into process chain on left or right according to key type.
  if (keyBits_IsType(key, KKT_Resume))
    link_insertBefore(&proc->keyRing, &key->u.gk.kr);
  else
    link_insertAfter(&proc->keyRing, &key->u.gk.kr);
    
  keyBits_SetPrepared(key);
}

// May Yield.
static bool
CheckTypeAndAllocCount(Key * key, ObjectLocator * pObjLoc,
  unsigned int baseType)
{
  OID oid = key->u.unprep.oid;

  if (! CheckObjectType(oid, pObjLoc, baseType))
    return true;

  ObCount count = GetObjectCount(oid, pObjLoc, false);
  if (count != key->u.unprep.count)
    return true;

  // If the object happens to be in memory:
  if (pObjLoc->locType == objLoc_ObjectHeader) {
    ObjectHeader * pObj = pObjLoc->u.objH;
    if (! objH_GetFlags(pObj, OFLG_Fetching)) {
      /* Link as next key after object */
      key->u.ok.pObj = pObj;
      link_insertAfter(&pObj->keyRing, &key->u.ok.kr);
      keyBits_SetPrepared(key);
    }
  }

  return false;
}

// May Yield.
bool	// returns true iff key is voided
key_DoValidate(Key * thisPtr)
{
  ObjectLocator objLoc;

  assert(keyBits_IsUnprepared(thisPtr));
  
  switch(keyBits_GetType(thisPtr)) {	/* not prepared, so not hazarded! */
  case KKT_Resume:
  case KKT_Start:
  case KKT_Process:
    {
      if (! CheckObjectType(thisPtr->u.unprep.oid, &objLoc,
                            capros_Range_otNode))
        break;

      ObCount countToCompare =
        GetObjectCount(thisPtr->u.unprep.oid, &objLoc,
                       keyBits_IsType(thisPtr, KKT_Resume) );
      if (countToCompare != thisPtr->u.unprep.count)
        break;

      // If the object happens to be in memory:
      if (objLoc.locType == objLoc_ObjectHeader) {
        ObjectHeader * pObj = objLoc.u.objH;
        assert(! objH_GetFlags(pObj, OFLG_Fetching));	// because its a node

        PrepareKeyToProcess(thisPtr, node_GetProcess(objH_ToNode(pObj)));
      }

      return false;	// key is valid
    }

  case KKT_Page:
    if (! CheckTypeAndAllocCount(thisPtr, &objLoc, capros_Range_otPage)) {
      return false;	// key is valid
    }
    break;
    
  case KKT_Node:
  case KKT_Forwarder:
  case KKT_GPT:
    if (! CheckTypeAndAllocCount(thisPtr, &objLoc, capros_Range_otNode))
      return false;	// key is valid
    break;

  default:
    return false;
  }
  
  // Key is rescinded.
  DEBUG(prepare)
    dprintf(true, "Voiding invalid key\n");

  assert(! keyBits_IsHazard(thisPtr));
  assert(keyBits_IsUnprepared(thisPtr));
  key_NH_SetToVoid(thisPtr);
  return true;
}

// May Yield.
static ObjectHeader *
DoGetObject(Key * key, ObjectLocator * pObjLoc,
  unsigned int baseType)
{
  OID oid = key->u.unprep.oid;

  if (! CheckObjectType(oid, pObjLoc, baseType))
    return NULL;

  ObCount count = GetObjectCount(oid, pObjLoc, false);
  if (count != key->u.unprep.count)
    return NULL;

  ObjectHeader * pObj = GetObject(oid, pObjLoc);
  if (pObj) {
    /* Link as next key after object */
    key->u.ok.pObj = pObj;
    link_insertAfter(&pObj->keyRing, &key->u.ok.kr);
  }

  return pObj;
}

/* NOTE: if we are running OB_MOD_CHECK, the key prepare logic does not
 * change the check field in the containing object.
 * The check field is always computed as a function of the unprepared key. */
/* May Yield. */
bool	// returns true iff key is voided
key_DoPrepare(Key* thisPtr)
{
  ObjectHeader * pObj = NULL;
  ObjectLocator objLoc;

  assert( keyBits_IsUnprepared(thisPtr) );
  
  KernStats.nKeyPrep++;
  
#ifdef MEM_OB_CHECK
  uint32_t ck = thisPtr->CalcCheck();
#endif
  
  switch(keyBits_GetType(thisPtr)) {	/* not prepared, so not hazarded! */
  case KKT_Resume:
  case KKT_Start:
  case KKT_Process:
    {
      if (! CheckObjectType(thisPtr->u.unprep.oid, &objLoc,
                            capros_Range_otNode))
        break;	// with pObj == NULL

      ObCount countToCompare =
        GetObjectCount(thisPtr->u.unprep.oid, &objLoc,
                       keyBits_IsType(thisPtr, KKT_Resume) );
      if (countToCompare != thisPtr->u.unprep.count) {
        break;	// with pObj == NULL
      }

      pObj = GetObject(thisPtr->u.unprep.oid, &objLoc);
      Node * pNode = objH_ToNode(pObj);
      assert(objC_ValidNodePtr(pNode));

      PrepareKeyToProcess(thisPtr, node_GetProcess(pNode));

#ifdef MEM_OB_CHECK
      assert(ck == thisPtr->CalcCheck());
#endif
      return false;
    }

  case KKT_Page:
    pObj = DoGetObject(thisPtr, &objLoc, capros_Range_otPage);
    break;
    
  case KKT_Node:
  case KKT_Forwarder:
  case KKT_GPT:
    pObj = DoGetObject(thisPtr, &objLoc, capros_Range_otNode);
    break;

  default:
    keyBits_SetPrepared(thisPtr);
    return false;
  }
  
  if (pObj == 0) {
    DEBUG(prepare)
      dprintf(true, "Voiding invalid key %#x\n", thisPtr);

    assert ( keyBits_IsHazard(thisPtr) == false );
    assert ( keyBits_IsUnprepared(thisPtr) );
    key_NH_SetToVoid(thisPtr);
    keyBits_SetPrepared(thisPtr);
    return true;
  }

#ifdef MEM_OB_CHECK
  assert(ck == thisPtr->CalcCheck());
#endif
    
  keyBits_SetPrepared(thisPtr);
  return false;
}

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
key_NH_Move(Key * to, Key * from)
{
  assert(! keyBits_IsHazard(to));
  assert(! keyBits_IsHazard(from));
  assert(! keyBits_IsPrepared(to));

  *to = *from;

  if (keyBits_IsPreparedObjectKey(from)) {
    // Fix up chain pointers:
    to->u.ok.kr.prev->next = to->u.ok.kr.next->prev = &to->u.ok.kr;
  }
  keyBits_InitToVoid(from);
}

// key must already be unchained if necessary.
void
key_SetToObj(Key * key, ObjectHeader * pObj,
  unsigned int kkt, unsigned int keyPerms, unsigned int keyData)
{
  assert(! keyBits_IsHazard(key));

  keyBits_InitType(key, kkt);

  key->u.ok.pObj = pObj;
  key->keyData = keyData;
  key->keyPerms = keyPerms;
  link_insertAfter(&pObj->keyRing, &key->u.ok.kr);
  keyBits_SetPrepared(key);
}

// k must already be unchained if necessary.
void
key_SetToProcess(Key * k,
  Process * p, unsigned int keyType, unsigned int keyData)
{
  assert(keyR_IsValid(&p->keyRing, p));

  keyBits_InitType(k, keyType);
  k->keyData = keyData;
  k->keyPerms = 0;
  k->u.gk.pContext = p;
  link_insertAfter(&p->keyRing, &k->u.gk.kr);
  keyBits_SetPrepared(k);
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

void
copy_key(uint32_t fromSlot, uint32_t toSlot)
{
  if (toSlot != KR_VOID) {
    Process * p = act_CurContext();
    key_NH_Set(&p->keyReg[toSlot], &p->keyReg[fromSlot]);
  }
}

void
xchg_key(uint32_t slot0, uint32_t slot1)
{
  // Never overwrite KR_VOID.
  if (slot0 == KR_VOID) {
    copy_key(slot0, slot1);
  } else {
    Process * p = act_CurContext();
    Key * key0 = &p->keyReg[slot0];

    Key tmp;
    keyBits_InitToVoid(&tmp);
    key_NH_Set(&tmp, key0);

    Key * key1 = &p->keyReg[slot1];
    key_NH_Set(key0, key1);
    if (slot1 != KR_VOID)
      key_NH_Set(key1, &tmp);
  
    key_NH_Unchain(&tmp);
  }
}

// Disregards any read hazard on src.
/* Note, this procedure may set OFLG_AllocCntUsed or OFLG_CallCntUsed,
 * which can accelerate the overflow of the allocation or call counts,
 * so it should not be used indiscriminately.
 * It is used only when cleaning (which is limited by disk bandwidth)
 * or by the closely held KeyBits key. */
void
key_MakeUnpreparedCopy(Key * dst, const Key * src)
{
  *dst = *src;
  dst->keyFlags &= ~(KFL_HAZARD_BITS | KFL_PREPARED);
  if ((! keyBits_IsObjectKey(src)) || keyBits_IsUnprepared(src)) {
    return;
  }

  ObjectHeader * pObj;
  ObCount cnt;
  
  if (keyBits_IsProcessType(src)) {
    pObj = node_ToObj(src->u.gk.pContext->procRoot);

    if (keyBits_IsType(src, KKT_Resume)) {
      cnt = node_GetCallCount(objH_ToNode(pObj));
      objH_SetFlags(pObj, OFLG_CallCntUsed);
    } else {
      cnt = objH_GetAllocCount(pObj);
      objH_SetFlags(pObj, OFLG_AllocCntUsed);
    }
  } else {
    pObj = src->u.ok.pObj;
    cnt = objH_GetAllocCount(pObj);
    /* Note, setting OFLG_AllocCntUsed and OFLG_CallCntUsed does not
     * dirty the object. These bits are always assumed to be on on the disk. */
    objH_SetFlags(pObj, OFLG_AllocCntUsed);
  }

  dst->u.unprep.count = cnt;
  dst->u.unprep.oid = pObj->oid;
}

void
key_NH_Unprepare(Key* thisPtr)
{
  /* fatal("Unprepare() called\n"); */
  assert(keyBits_IsHazard(thisPtr) == false);

  if ( keyBits_IsUnprepared(thisPtr) )
    return;

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top Key::NH_Unprepare()");
#endif

  if ( keyBits_IsObjectKey(thisPtr) ) {
    ObjectHeader * pObj;
    ObCount cnt;
  
    if (keyBits_IsProcessType(thisPtr)) {
#ifndef NDEBUG
      if (ValidCtxtPtr(thisPtr->u.gk.pContext) == false)
	fatal("Key 0x%08x Kt %d, 0x%08x not valid ctxt ptr\n",
              thisPtr, keyBits_GetType(thisPtr), thisPtr->u.gk.pContext);
#endif
      pObj = node_ToObj(thisPtr->u.gk.pContext->procRoot);

      if (keyBits_IsType(thisPtr, KKT_Resume)) {
        cnt = node_GetCallCount(objH_ToNode(pObj));
        objH_SetFlags(pObj, OFLG_CallCntUsed);
      } else
        goto nonresume;
    } else {
      pObj = thisPtr->u.ok.pObj;
nonresume:
      cnt = objH_GetAllocCount(pObj);
      objH_SetFlags(pObj, OFLG_AllocCntUsed);
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

    thisPtr->u.unprep.oid = pObj->oid;
    thisPtr->u.unprep.count = cnt;
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
    
    if (keyBits_IsType(thisPtr, KKT_Resume)) {
      printf("0x%08x rsm 0x%08x 0x%08x ",
		     thisPtr,
		     pWKey[0], node_GetCallCount(objH_ToNode(pObj)));
    }
    else {
      printf("0x%08x pob 0x%08x 0x%08x ",
		     thisPtr,
		     pWKey[0], objH_GetAllocCount(pObj));
    }
    printOid(pObj->oid);
    printf("\n");
  }
  else {
    printf("0x%08x ukt 0x%08x 0x%08x 0x%08x 0x%08x\n",
		   thisPtr,
		   pWKey[0], pWKey[1], pWKey[2], pWKey[3]);
  }
}

#ifdef OPTION_OB_MOD_CHECK
uint32_t
key_CalcCheck(const Key * thisPtr)
{
  int i;
  Key k;

  assert(thisPtr);
        
  /* The following is similar to key_MakeUnpreparedCopy(&k, thisPtr),
   * but does not set AllocCntUsed or CallCntUsed flags. */
  k = *thisPtr;
  k.keyFlags &= ~(KFL_HAZARD_BITS | KFL_PREPARED);

  if (keyBits_IsObjectKey(thisPtr) && ! keyBits_IsUnprepared(thisPtr)) {
    ObjectHeader * pObj;
    ObCount cnt;
    
    if (keyBits_IsProcessType(thisPtr)) {
      pObj = node_ToObj(thisPtr->u.gk.pContext->procRoot);

      if (keyBits_IsType(thisPtr, KKT_Resume))
        cnt = node_GetCallCount(objH_ToNode(pObj));
      else
        cnt = objH_GetAllocCount(pObj);
    } else {
      pObj = thisPtr->u.ok.pObj;
      cnt = objH_GetAllocCount(pObj);
    }

    k.u.unprep.count = cnt;
    k.u.unprep.oid = pObj->oid;
  }
  
  uint32_t * pWKey = (uint32_t *) &k;
  uint32_t ck = 0;

  for (i = 0; i < (sizeof(Key) / sizeof(uint32_t)); i++) 
    ck ^= pWKey[i];

  return ck;
}
#endif

#ifndef NDEBUG
bool
key_IsValid(const Key* thisPtr)
{
  if (keyBits_IsMiscKey(thisPtr)) {
    switch (keyBits_GetType(thisPtr)) {
    // By default, misc keys should have no data.
    default:
      if (thisPtr->u.nk.value[0] || thisPtr->u.nk.value[1]
          || thisPtr->u.nk.value[2] ) {
	printf("Misc key %#x has data\n", thisPtr);
        return false;
      }
    case KKT_DevicePrivs:
    case KKT_IORQ:
    case KKT_RTC:
      break;
    }
    return true;
  }

  if ( keyBits_IsPreparedObjectKey(thisPtr) ) {
    if ( keyBits_IsProcessType(thisPtr) ) {
      Process * proc = thisPtr->u.gk.pContext;
      if (! ValidCtxtPtr(proc)) {
	printf("Key %#x has invalid proc ptr %#x\n", thisPtr, proc);
	return false;
      }
    }
    else if ( keyBits_IsType(thisPtr, KKT_Page) ) {
      ObjectHeader *pObject = thisPtr->u.ok.pObj;
      if (! objC_ValidPagePtr(pObject)) {
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
      if (! objC_ValidNodePtr(pNode)) {
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

    if ( keyBits_IsObjectKey(thisPtr) ) {
      /* KeyRing pointers must either point to key slots or to
       * object root.
       */
      KeyRing * krn = thisPtr->u.ok.kr.next;
      KeyRing * krp = thisPtr->u.ok.kr.prev;

      if ( ! ( key_ValidKeyPtr((Key *) krn) ||
	       objC_ValidPagePtr(keyR_ToObj(krn)) ||
	       objC_ValidNodePtr(objH_ToNodeConst(keyR_ToObj(krn))) ||
	       ValidCtxtKeyRingPtr(krn) ) ) {
	printf("key 0x%x nxt key 0x%x bogus\n", thisPtr, krn);
	return false;
      }
      
      if ( ! ( key_ValidKeyPtr((Key *) krp) ||
	       objC_ValidPagePtr(keyR_ToObj(krp)) ||
	       objC_ValidNodePtr(objH_ToNodeConst(keyR_ToObj(krp))) ||
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
    }
  }
  return true;
}
#endif

#ifdef OPTION_DDB
/* Returns:
 0 if pKey is not a valid key pointer,
 1 if it is a key in a Node,
 2 if it is a keyreg in a Process,
 3 if it is a key in the Invocation structure. */
int
key_ValidKeyPtr(const Key *pKey)
{
  if (inv_IsInvocationKey(&inv, pKey))
    return 3;

  if (proc_ValidKeyReg(pKey))
    return 2;

  if (node_ValidNodeKeyPtr(pKey))
    return 1;

  return 0;
}
#endif
