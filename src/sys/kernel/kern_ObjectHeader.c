/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Check.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Activity.h>
#include <kerninc/SegWalk.h>
#include <kerninc/Depend.h>
#include <arch-kerninc/PTE.h>
#include <arch-kerninc/Machine-inline.h>

#define dbg_rescind	0x1	/* steps in taking snapshot */

/* Following should be an OR of some of the above */
#define dbg_flags   (0)

#define DBCOND(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if DBCOND(x)
#define DEBUG2(x,y) if ((dbg_##x|dbg_##y) & dbg_flags)


uint8_t objH_CurrentTransaction = 1; /* guarantee nonzero! */

#ifdef OPTION_DDB
const char *ddb_obtype_name(uint8_t t)
{
  const char * names[] = {
    "NtUnprep  ",
    "NtSegment ",
    "NtProcRoot",
    "NtKeyRegs ",
    "NtRegAnnex",
    "NtFreeFrm ",
    "PtDataPage",
    "PtNewAlloc",
    "PtKernHeap",
    "PtDevPage ",
    "PtFreeFrm "
    MD_PAGE_OBNAMES
    , 0
  };

  return names[t];
}
#endif

void
pageH_KernPin(PageHeader * thisPtr)
{
  assert(thisPtr->kernPin < BYTE_MAX);
  thisPtr->kernPin++;
}

void
pageH_KernUnpin(PageHeader * thisPtr)
{
  assert(thisPtr->kernPin);
  thisPtr->kernPin--;
}

void
objH_AddProduct(ObjectHeader * thisPtr, MapTabHeader * product)
{
  product->next = thisPtr->prep_u.products;
  product->producer = thisPtr;
  thisPtr->prep_u.products = product;
}

void
objH_DelProduct(ObjectHeader * thisPtr, MapTabHeader * product)
{
  assert(product->producer == thisPtr);
  
  // Unchain it.
  MapTabHeader * * mthpp = &thisPtr->prep_u.products;
  while (*mthpp != product) {
    assert(*mthpp);	// else not found in list
    mthpp = &(*mthpp)->next;
  }
  *mthpp = product->next;	// unchain it
  
  product->next = 0;
  product->producer = 0;
}

#if 0
void
ObjectHeader::DoCopyOnWrite()
{
  assert (obType > ObType::NtLAST_NODE_TYPE);
#if 0
  dprintf(true,
		  "Trying copy on write, ty %d oid 0x%08x%08x "
		  "hdr 0x%08x\n",
		  obType, (uint32_t) (oid >> 32), (uint32_t) (oid), this);
#endif

  assert(GetFlags(OFLG_CKPT) && IsDirty());
  
  ObjectHeader *pObj = ObjectCache::GrabPageFrame();

  assert (pObj->kr.IsEmpty());

  kva_t from = ObjectCache::ObHdrToPage(this);
  kva_t to = ObjectCache::ObHdrToPage(pObj);
    
  /* Copy the page data */
  memcpy((void *)to, (const void *)from, EROS_PAGE_SIZE);

  /* FIX -- the header needs to be copied with care -- perhaps this
   * should be expanded in more explicit form?
   */
  
  /* And the object header: */
  memcpy(pObj, this, sizeof(ObjectHeader));

  /* The key ring needs to be reset following the header copy */
  pObj->kr.ResetRing();
  
  /* Because the original may already be queued up for I/O, the copy
   * has to become the new version.  This poses a problem: we may have
   * gotten here trying to mark an object dirty to satisfy a write
   * fault, in which event there are very likely outstanding prepared
   * capabilities to this object sitting on the stack somewhere.  In
   * all such cases the object being copied will be pinned.  If the
   * object being copied is pinned we Yield(), which will force the
   * whole chain of events to be re-executed, this time arriving at
   * the correct object.
   */
  
  /* NOTE About the 'dirty' bit -- which I have 'fixed' twice now to
   * my regret.  It really should be zero.  We are only running this
   * code if the object was marked ckpt.  In that event, the
   * checkpointed version of the object is dirty until flushed, but
   * the COW'd version of the object is not dirty w.r.t the next
   * checkpoint until something happens along to try and dirty it.  We
   * are here because someone is trying to do that, but we must let
   * MakeObjectDirty() handle the marking rather than do it here.  The
   * prolem is that we haven't reserved a directory entry for the
   * object.  This can (and probably should) be resolved by calling
   * RegisterDirtyObject() from here to avoid the extra Yield(), but
   * for now be lazy, since I know that will actually work.
   */
  
  assert(kernPin == 0);
  
  ClearFlags(OFLG_CURRENT);
  pObj->SetFlags(OFLG_CURRENT);
  pObj->ClearFlags(OFLG_CKPT|OFLG_IO|OFLG_DIRTY|OFLG_REDIRTY);
#ifdef DBG_CLEAN
  printf("Object 0x%08x ty %d oid=0x%08x%08x COW copy cleaned\n",
		 pObj, pObj->obType,
		 (uint32_t) (pObj->oid >> 32),
		 (uint32_t) pObj->oid);
#endif
  pObj->SetFlags(GetFlags(OFLG_DISKCAPS));
  pObj->ioCount = 0;
  pObj->userPin = 0;
  pObj->prstPin = 0;
#ifdef OPTION_OB_MOD_CHECK
  pObj->check = pObj->CalcCheck();
#endif


  /* Switch the keyring to the new object, and update all of the keys
   * to point to the copy:
   */

  kr.ObjectMoved(pObj);

  Unintern();			/* take us out of the hash chain */
  pObj->Intern();		/* and put the copy in in our place. */
  
  /* we must now re-insert the old page as a log page, because the new
   * page might conceivably get aged out before the old page, at which
   * point we would find the wrong one.
   */
  

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom DoCopyOnWrite()");
#endif

  /* Since we may have come here by way of the page fault code, we are
   * now forced to Yield(), because there are almost certainly
   * outstanding pointers to this object on the stack:
   */
  Thread::Current()->Yield();
}
#endif

void
objH_FlushIfCkpt(ObjectHeader* thisPtr)
{
#ifdef OPTION_PERSISTENT
#error "This needs an implementation."
  /* If this page is involved in checkpointing, run the COW logic. */
  if (GetFlags(OFLG_CKPT) && IsDirty()) {
    assert (IsFree() == false);
    if (obType <= ObType::NtLAST_NODE_TYPE) {
      Checkpoint::WriteNodeToLog((Node *) this);
      assert (!IsDirty());
    }
    else  
      Persist::WritePage(this, true);
  }
#else
  assert(!objH_GetFlags(thisPtr, OFLG_CKPT));
#endif
}

void
objH_MakeObjectDirty(ObjectHeader* thisPtr)
{
#ifndef NDEBUG
  extern bool InvocationCommitted;
#endif

  assertex(thisPtr, objH_IsUserPinned(thisPtr));
  assertex(thisPtr, objH_GetFlags(thisPtr, OFLG_CURRENT));

  if ( objH_IsDirty(thisPtr) && (objH_GetFlags(thisPtr, OFLG_CKPT|OFLG_IO) == 0) )
    return;
  
  assert (InvocationCommitted == false);

  if (thisPtr->obType == ot_PtDataPage ||
      thisPtr->obType == ot_PtDevicePage ||
      thisPtr->obType <= ot_NtLAST_NODE_TYPE)
    assert(objH_IsUserPinned(thisPtr));
  
  objH_FlushIfCkpt(thisPtr);
  
#ifdef OPTION_OB_MOD_CHECK
  if (objH_IsDirty(thisPtr) == 0 && thisPtr->check != objH_CalcCheck(thisPtr))
    fatal("MakeObjectDirty(0x%08x): not dirty and bad checksum!\n",
		  thisPtr);
#endif

#if 0
  /* This was correct only because we were not reassigning new
   * locations every time an object was dirtied.  Now that we are
   * doing reassignment, we must reregister.
   */
  
  if (IsDirty()) {
    /* in case a write is in progress, mark reDirty, but must not do
     * this before registration unless we know the object is already
     * dirty.  Note that we already know this object to be current!
     */
    SetFlags(OFLG_REDIRTY);
    return;
  }
#endif
  
#ifdef OPTION_PERSISTENT
  Checkpoint::RegisterDirtyObject(this);
#endif

  objH_SetDirtyFlag(thisPtr);
  objH_ClearFlags(thisPtr, OFLG_CKPT);
  
#if 0
  /* FIX: Why should we ever do this here? */
  ClearFlags(OFLG_REDIRTY);
#endif

#ifdef DBG_CLEAN
  {
    OID oid = thisPtr->oid;
    dprintf(true,
	    "Marked pObj=0x%08x oid=0x%08x%08x dirty. dirty: %c chk: %c\n",
	    thisPtr,
	    (uint32_t) (oid >> 32), (uint32_t) (oid),
	    objH_GetFlags(thisPtr, OFLG_DIRTY) ? 'y' : 'n',
	    objH_GetFlags(thisPtr, OFLG_CKPT) ? 'y' : 'n');
  }
#endif

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Top RegisterDirtyObject()");
#endif

#if 0  
  uint32_t ocpl = IRQ::splhigh()
  printf("** Object ");
  print(oid);
  printf(" marked dirty.\n");
  IRQ::splx(ocpl);
#endif
}

void
objH_Rescind(ObjectHeader* thisPtr)
{
  bool hasCaps;

  DEBUG(rescind)
    dprintf(true, "Rescinding ot=%d oid=0x%08x%08x\n",
	    thisPtr->obType,
            (uint32_t) (thisPtr->oid >> 32), (uint32_t) thisPtr->oid);

  assert (objH_GetFlags(thisPtr, OFLG_IO|OFLG_CKPT) == 0);
  assert (objH_GetFlags(thisPtr, OFLG_CURRENT) == OFLG_CURRENT);
  
#ifndef NDEBUG
  if (!keyR_IsValid(&thisPtr->keyRing, thisPtr))
    dprintf(true, "Keyring of oid 0x%08x%08x invalid!\n",
	    (uint32_t)(thisPtr->oid>>32), (uint32_t)thisPtr->oid);
#endif

  hasCaps = objH_GetFlags(thisPtr, OFLG_DISKCAPS);
  
  keyR_RescindAll(&thisPtr->keyRing, hasCaps);

  DEBUG(rescind)
    dprintf(true, "After 'RescindAll()'\n");

  if (hasCaps) {
    thisPtr->allocCount++;

    /* If object has on-disk keys, must dirty the new object to ensure
     * that the new counts get written.
     */
    if (thisPtr->obType <= ot_NtLAST_NODE_TYPE) {
      Node * thisNode = objH_ToNode(thisPtr);
      thisNode->callCount++;
      node_MakeDirty(thisNode);
    } else {
      pageH_MakeDirty(objH_ToPage(thisPtr));
    }

    objH_ClearFlags(thisPtr, OFLG_DISKCAPS);
    DEBUG(rescind)
      dprintf(true, "After bump alloc count\n");
  }

  /* FIX: Explicitly zeroing defeats the sever operation. */

  if (thisPtr->obType <= ot_NtLAST_NODE_TYPE) {
    Node * thisNode = objH_ToNode(thisPtr);
    /* zeroing unprepares and invalidates products too */

    node_DoClearThisNode(thisNode);
    node_MakeDirty(thisNode);
    assert ( thisPtr->obType == ot_NtUnprepared );
  }
  else if (thisPtr->obType == ot_PtDataPage) {
    kva_t pPage;

    objH_InvalidateProducts(thisPtr);

    pPage = pageH_GetPageVAddr(objH_ToPage(thisPtr));
    bzero((void*)pPage, EROS_PAGE_SIZE);
    pageH_MakeDirty(objH_ToPage(thisPtr));
  }
  else if (thisPtr->obType == ot_PtDevicePage) {
    fatal("Rescind of device pages not tested -- see shap!\n");
    objH_InvalidateProducts(thisPtr);

    /* Do not explicitly zero device pages -- might be microcode! */
  }
  else
    fatal("Rescind of non-object!\n");

  DEBUG(rescind)
    dprintf(true, "After zero object\n");
}

void
objH_ZapResumeKeys(ObjectHeader* thisPtr)
{
  keyR_ZapResumeKeys(&thisPtr->keyRing);
}

#ifdef OPTION_OB_MOD_CHECK
uint32_t
objH_CalcCheck(const ObjectHeader * thisPtr)
{
  Node *pNode = 0;
  uint32_t i = 0;
  uint32_t ck = 0;
  uint32_t w = 0;
  
#ifndef NDEBUG
  uint8_t oflags = thisPtr->flags;
#endif
#if 0
  printf("Calculating cksum for 0x%08x\n", this);
  printf("OID is 0x%08x%08x, ty %d\n", (uint32_t) (oid>>32),
		 (uint32_t) oid, obType);
#endif
  
  if (thisPtr->obType <= ot_NtLAST_NODE_TYPE) {

    assert (objC_ValidNodePtr((Node *) thisPtr));
    /* Object is a node - compute XOR including allocation count, call
     * counts, and key slots.
     */


    pNode = (Node *) thisPtr;
#if 0
    ck ^= ((uint32_t *) &allocCount)[0];
    ck ^= ((uint32_t *) &allocCount)[1];
    ck ^= ((uint32_t *) &(pNode->callCount))[0];
    ck ^= ((uint32_t *) &(pNode->callCount))[1];
#else
    ck ^= thisPtr->allocCount;
    ck ^= pNode->callCount;
#endif
    

    for (i = 0; i < EROS_NODE_SIZE; i++)
      ck ^= key_CalcCheck(node_GetKeyAtSlot(pNode, i));

  }
  else {

    assert (objC_ValidPagePtr(thisPtr));

    const uint32_t *  pageData = (const uint32_t *)
      pageH_GetPageVAddr(objH_ToPage((ObjectHeader *)thisPtr));

    for (w = 0; w < EROS_PAGE_SIZE/sizeof(uint32_t); w++)
      ck ^= pageData[w];
  }

  assert(thisPtr->flags == oflags);

  return ck;
}
#endif

void
objH_InvalidateProducts(ObjectHeader * thisPtr)
{
  if (thisPtr->obType == ot_PtDataPage ||
      thisPtr->obType == ot_PtDevicePage ||
      thisPtr->obType == ot_NtSegment) {
    while (thisPtr->prep_u.products) {
      ReleaseProduct(thisPtr->prep_u.products);
    }
    mach_InvalidateProducts(thisPtr);
  }
}

#ifdef OPTION_DDB
void
objH_ddb_dump(ObjectHeader * thisPtr)
{
  extern void db_printf(const char *fmt, ...);

#ifdef OPTION_OB_MOD_CHECK
  printf("Object Header 0x%08x (%s) calcCheck 0x%08x:\n", thisPtr,
	 ddb_obtype_name(thisPtr->obType),
	 /* CalcCheck() */ 0);
  printf("    oid=0x%08x%08x ac=0x%08x check=0x%08x\n",
	 (uint32_t) (thisPtr->oid >> 32), (uint32_t) thisPtr->oid,
	 thisPtr->allocCount, thisPtr->check);
#else
  printf("Object Header 0x%08x (%s) oid=0x%08x%08x ac=0x%08x\n", thisPtr,
	 ddb_obtype_name(thisPtr->obType),
	 (uint32_t) (thisPtr->oid >> 32), (uint32_t) thisPtr->oid,
         thisPtr->allocCount);
#endif
  printf("    ioCount=0x%08x next=0x%08x flags=0x%02x obType=0x%02x usrPin=%d\n",
	 thisPtr->ioCount, thisPtr->next,
         thisPtr->flags, thisPtr->obType, thisPtr->userPin );

  switch(thisPtr->obType) {
  case ot_PtDataPage:
  case ot_PtDevicePage:
    printf("    pageAddr=0x%08x\n", objH_ToPage(thisPtr)->pageAddr);
  case ot_NtSegment:
    {
      MapTabHeader * oh = thisPtr->prep_u.products;
      printf("    products=");
      while (oh) {
	printf(" 0x%08x", oh);
	oh = oh->next;
      }
      printf("\n");
      break;
    }
  case ot_NtProcessRoot:
  case ot_NtKeyRegs:
  case ot_NtRegAnnex:
    printf("    context=0x%08x\n", thisPtr->prep_u.context);
    break;
  default:
    if (thisPtr->obType > ot_PtLAST_COMMON_PAGE_TYPE)
      pageH_mdType_dump_header(objH_ToPage(thisPtr));
    break;
  }
}
#endif

/* #define PIN_DEBUG */

#ifndef NDEBUG
void
objH_TransLock(ObjectHeader* thisPtr)
{
#ifdef PIN_DEBUG
  printf("Pinning obhdr 0x%08x\n", this);
#endif
  thisPtr->userPin = objH_CurrentTransaction;
}
#endif
