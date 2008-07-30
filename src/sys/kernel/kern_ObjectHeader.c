/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/Check.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Activity.h>
#include <kerninc/Depend.h>
#include <kerninc/Node.h>
#include <kerninc/Invocation.h>
#include <kerninc/Ckpt.h>
#include <kerninc/LogDirectory.h>
#include <kerninc/Node-inline.h>
#include <disk/DiskNode.h>
#include <disk/CkptRoot.h>
#include <arch-kerninc/PTE.h>
#include <arch-kerninc/Machine-inline.h>

#define dbg_rescind	0x1	/* steps in taking snapshot */

/* Following should be an OR of some of the above */
#define dbg_flags   (0)

#define DBCOND(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if DBCOND(x)
#define DEBUG2(x,y) if ((dbg_##x|dbg_##y) & dbg_flags)


uint16_t objH_CurrentTransaction = 1; /* guarantee nonzero! */

// The maximum allocation count of any nonpersistent object.
ObCount maxNPAllocCount = 0;

// The allocation count of nonpersistent objects at restart.
ObCount restartNPAllocCount = 0;

// The following are initialized to zero by the loader.
unsigned long numDirtyObjectsWorking[capros_Range_otNumBaseTypes];
unsigned long numDirtyObjectsNext[capros_Range_otNumBaseTypes];
unsigned long numDirtyLogPots;

#ifdef OPTION_DDB
const char *ddb_obtype_name(uint8_t t)
{
  const char * names[] = {
    "NtUnprep",
    "NtSegment",
    "NtProcRoot",
    "NtKeyRegs",
    "NtRegAnnex",
    "NtFree",
    "PtDataPage",
    "PtDevPage",
    "PtDMABlock",
    "PtDMASecondary",
    "PtTagPot",
    "PtHomePot",
    "PtLogPot",
    "PtNewAlloc",
    "PtKernUse",
    "PtFree",
    "PtSecondary"
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
  
  pObj->SetFlags(GetFlags(OFLG_DISKCAPS));
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
  
  /* Since we may have come here by way of the page fault code, we are
   * now forced to Yield(), because there are almost certainly
   * outstanding pointers to this object on the stack:
   */
  Thread::Current()->Yield();
}
#endif

static bool
EnoughWorkingDirEnts(unsigned long availWorkingDirEnts)
{
  unsigned long totalDirtyObjsWorking =
    numDirtyObjectsWorking[capros_Range_otNode]
    + numDirtyObjectsWorking[capros_Range_otPage];
  if (availWorkingDirEnts < totalDirtyObjsWorking) {	// not enough dir ents
    dprintf(true, "Not enough dirents to dirty object. %d %d\n",
            availWorkingDirEnts, totalDirtyObjsWorking);
    return false;
  }
  return true;
}

void
objH_EnsureWritable(ObjectHeader * pObj)
{
  assert(objH_IsUserPinned(pObj)
         || ((   pObj->obType == ot_NtProcessRoot
              || pObj->obType == ot_NtKeyRegs
              || pObj->obType == ot_NtRegAnnex)
             && objH_IsUserPinned(proc_ToObj(pObj->prep_u.context)) ) );

  if (objH_IsDirty(pObj)
      && ! objH_GetFlags(pObj, OFLG_KRO) )	// already writeable
    return;

  // Non-persistent objects are always dirty and never KRO,
  // so the object must be persistent:
  assert(objH_GetFlags(pObj, OFLG_Cleanable));

  assert(! InvocationCommitted);

#ifdef OPTION_OB_MOD_CHECK
  if (pObj->check != objH_CalcCheck(pObj))
    fatal("MakeObjectDirty(0x%08x): not dirty and bad checksum!\n",
		  pObj);
#endif

  /* If this system was started from a checkpoint,
   * including a big bang whose persistent objects are initialized
   * as a checkpoint on disk,
   * there won't be any persistent objects in memory until the restart
   * has begun fetching them from disk. 
   * If this system was started as a big bang with persistent objects
   * preloaded in RAM,
   * the IPL process won't start any persistent processes until the
   * restart is done. 
   * The migrator process initializes itself and allocates all the
   * storage it needs before it starts the restart.
   * Therefore in all cases, the migrator and at least one disk driver
   * have been initialized, and can run without allocating further RAM.
   * Therefore it is safe to begin dirtying objects, because
   * we know we will be able to clean them eventually. */

  unsigned int baseType = objH_GetBaseType(pObj);

  unsigned long availWorkingDirEnts = ld_numAvailableEntries(retiredGeneration);

  if (! ckptIsActive()) {
    assert(! objH_GetFlags(pObj, OFLG_KRO));

    // Tentatively count this object as dirty:
    numDirtyObjectsWorking[baseType]++;

    // Calc space needed to complete a checkpoint:
    unsigned long tentRes
      = CalcLogReservation(numDirtyObjectsWorking, ld_numWorkingEntries());

    // Calc space available for checkpoint:
    frame_t retGenFrames = OIDToFrame(oldestNonRetiredGenLid - logCursor);
    if (oldestNonRetiredGenLid < logCursor)	// account for circular log
      retGenFrames += OIDToFrame(logWrapPoint - MAIN_LOG_START);

    if (tentRes > retGenFrames) {	// not enough space in log
      goto declareDemarc;
    }

    // Calc space in working area:
    frame_t wkgSpace = OIDToFrame(logCursor - workingGenFirstLid);
    if (logCursor < workingGenFirstLid)	// account for circular log
      wkgSpace += OIDToFrame(logWrapPoint - MAIN_LOG_START);

    if ((wkgSpace + tentRes) > logSizeLimited) { // generation too big
      goto declareDemarc;
    }

    unsigned long softLimit = objC_nPages + objC_nNodes; // for now

    if (availWorkingDirEnts < softLimit) {	// dir ents getting low
  declareDemarc:
      numDirtyObjectsWorking[baseType]--;	// undo tentative count
      DeclareDemarcationEvent();
      goto ckptActive;		// a checkpoint is now active
    }

    if (! EnoughWorkingDirEnts(availWorkingDirEnts)) {
      numDirtyObjectsWorking[baseType]--;	// undo tentative count
      assert(!"complete");
    }

    // It is OK to dirty the object. Let the tentative count stand.
  } else {	// a checkpoint is active
  ckptActive:
    if (! EnoughWorkingDirEnts(availWorkingDirEnts)) {
      // Not enough dir ents to clean the dirty working versions.
      assert(!"complete");
    }

    // Tentatively count this object as dirty:
    numDirtyObjectsNext[baseType]++;

    GenNum nextRetiredGenNum = GetNextRetiredGeneration();

    unsigned long availNextDirEnts = ld_numAvailableEntries(nextRetiredGenNum);
    unsigned long totalDirtyObjs =
      numDirtyObjectsWorking[capros_Range_otNode]
      + numDirtyObjectsWorking[capros_Range_otPage]
      + numDirtyObjectsNext[capros_Range_otNode]
      + numDirtyObjectsNext[capros_Range_otPage];
    if (availNextDirEnts < totalDirtyObjs) {
      // Not enough dir ents to clean the dirty working and next versions.
      numDirtyObjectsNext[baseType]--;	// undo tentative count
      dprintf(true, "Not enough next dirents to dirty object. %d %d\n",
              availNextDirEnts, totalDirtyObjs);
      assert(!"complete");
    }

    // Calc space needed to complete the current checkpoint and the next one:
    unsigned long tentRes
      = CalcLogReservation(numDirtyObjectsWorking, ld_numWorkingEntries())
        + CalcLogReservation(numDirtyObjectsNext, 0);

    // Calc space available for those checkpoints:
    frame_t retGenFrames = OIDToFrame(oldestNonNextRetiredGenLid - logCursor);
    if (oldestNonNextRetiredGenLid < logCursor)	// account for circular log
      retGenFrames += OIDToFrame(logWrapPoint - MAIN_LOG_START);

    if (tentRes > retGenFrames	// not enough space in log
        || (workingGenerationNumber - nextRetiredGenNum)
           > MaxUnmigratedGenerations
		// not enough space in next CkptRoot.generations[]
       ) {
      numDirtyObjectsNext[baseType]--;	// undo tentative count
      assert(!"complete");	// wait for a migration to complete
    }

    // Undo tentative count, because MitigateKRO may Yield.
    numDirtyObjectsNext[baseType]--;

    if (objH_GetFlags(pObj, OFLG_KRO)) {
      switch (objH_GetBaseType(pObj)) {
      default:	// page
        if (pageH_MitigateKRO(objH_ToPage(pObj)) != objH_ToPage(pObj))
          // the current version moved
          act_Yield();	// simplest way to recover is to start over
        break;

      case capros_Range_otNode:
        node_MitigateKRO(objH_ToNode(pObj));
        break;
      }
    }

    // Now definitely count this object as dirty:
    numDirtyObjectsNext[baseType]++;
  }

  objH_SetDirtyFlag(pObj);
  
#if 1//// until tested, then #ifdef DBG_CLEAN
  dprintf(true,
	  "Marked pObj=0x%08x oid=%#llx dirty. dirty: %c wkg: %c\n",
	  pObj, pObj->oid,
	  objH_GetFlags(pObj, OFLG_DIRTY) ? 'y' : 'n',
	  objH_GetFlags(pObj, OFLG_Working) ? 'y' : 'n');
#endif
}

void
objH_Rescind(ObjectHeader * thisPtr)
{
  DEBUG(rescind)
    dprintf(true, "Rescinding ot=%d oid=%#llx\n",
	    thisPtr->obType, thisPtr->oid);

#ifndef NDEBUG
  if (!keyR_IsValid(&thisPtr->keyRing, thisPtr))
    dprintf(true, "Keyring of oid 0x%08x%08x invalid!\n",
	    (uint32_t)(thisPtr->oid>>32), (uint32_t)thisPtr->oid);
#endif
  
  keyR_RescindAll(&thisPtr->keyRing);

  if (objH_isNodeType(thisPtr)) {
    node_BumpCallCount(objH_ToNode(thisPtr));

    if (thisPtr->obType == ot_NtProcessRoot) {
      Process * proc = thisPtr->prep_u.context;
      keyR_RescindAll(&proc->keyRing);
    }
  }

  DEBUG(rescind)
    dprintf(true, "After 'RescindAll()'\n");

  if (objH_GetFlags(thisPtr, OFLG_AllocCntUsed)) {
    thisPtr->allocCount++;

    // Track the maximum count of any nonpersistent object.
    if (! objH_GetFlags(thisPtr, OFLG_Cleanable)
        && thisPtr->allocCount > maxNPAllocCount)
      maxNPAllocCount = thisPtr->allocCount;

    objH_ClearFlags(thisPtr, OFLG_AllocCntUsed);

    /* The object must be dirty to ensure that the new count gets saved. */
    assert(objH_IsDirty(thisPtr));
  }
}

void
node_DoBumpCallCount(Node * pNode)
{
  pNode->callCount++;

  // Track the maximum count of any nonpersistent object.
  if (! objH_GetFlags(node_ToObj(pNode), OFLG_Cleanable)
      && pNode->callCount > maxNPAllocCount)
    maxNPAllocCount = pNode->callCount;

  objH_ClearFlags(node_ToObj(pNode), OFLG_CallCntUsed);

  /* The object must be dirty to ensure that the new count gets saved. */
  assert(objH_IsDirty(node_ToObj(pNode)));
}

void
objH_ClearObj(ObjectHeader * thisPtr)
{
  if (thisPtr->obType <= ot_NtLAST_NODE_TYPE) {
    Node * thisNode = objH_ToNode(thisPtr);
    /* zeroing unprepares and invalidates products too */

    node_DoClearThisNode(thisNode);
    node_EnsureWritable(thisNode);
  }
  else if (thisPtr->obType == ot_PtDataPage) {
    objH_InvalidateProducts(thisPtr);

    kva_t pPage = pageH_GetPageVAddr(objH_ToPage(thisPtr));
    kzero((void*)pPage, EROS_PAGE_SIZE);
    pageH_EnsureWritable(objH_ToPage(thisPtr));
  }
  else if (thisPtr->obType == ot_PtDevicePage) {
    objH_InvalidateProducts(thisPtr);

    /* Do not explicitly zero device pages -- might be microcode! */
  }
  else
    fatal("Rescind of non-object!\n");

  DEBUG(rescind)
    dprintf(true, "After zero object\n");
}

// May Yield.
Node *
pageH_GetNodeFromPot(PageHeader * pageH, unsigned int obIndex)
{
  Node * pNode = objC_GrabNodeFrame();

  // A node pot is just an array of DiskNode's.
  DiskNode * dn = (DiskNode *)pageH_GetPageVAddr(pageH);
  dn += obIndex;
  node_SetEqualTo(pNode, dn);
  ObjectHeader * pObj = node_ToObj(pNode);
  pObj->obType = ot_NtUnprepared;
  objH_InitPresentObj(pObj, pObj->oid);
  objH_ClearFlags(pObj, OFLG_DIRTY);
  objH_SetFlags(pObj, OFLG_Cleanable);
  return pNode;
}

#ifdef OPTION_OB_MOD_CHECK
uint32_t
objH_CalcCheck(const ObjectHeader * thisPtr)
{
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

    Node * pNode = (Node *) thisPtr;
    ck ^= objH_GetAllocCount(thisPtr);
    ck ^= node_GetCallCount(pNode);
    

    for (i = 0; i < EROS_NODE_SIZE; i++)
      ck ^= key_CalcCheck(node_GetKeyAtSlot(pNode, i));

  }
  else {		// it's a page
    /* Note, we access the page via the physical map, which can cause
       cache incoherency if the page was written via some other
       virtual address. But this should not matter, because we only
       use the resultant checksum if we expect the page is not modified.
    */

    assertex(thisPtr, objC_ValidPagePtr(thisPtr));

    const uint32_t *  pageData = (const uint32_t *)
      pageH_GetPageVAddr(objH_ToPageConst(thisPtr));

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
  }
}

#ifdef OPTION_DDB

static void
PrintObjData(ObjectHeader * thisPtr)
{
  printf(" oid=%#llx ac=%#x\n", thisPtr->oid, objH_GetAllocCount(thisPtr));
  printf(" flags=0x%02x usrPin=%d",
         thisPtr->flags, thisPtr->userPin );
#ifdef OPTION_OB_MOD_CHECK
  printf(" check=%#x calcCheck=%#x", thisPtr->check,
#if 1
         objH_CalcCheck(thisPtr)
#else	// Sometimes we don't want to call objH_CalcCheck,
	// because it mutates the memory system.
         0xbadbad00
#endif
        );
#endif
  printf("\n");
}

void
objH_ddb_dump(ObjectHeader * thisPtr)
{
  printf("ObHdr %#x type %d (%s) ", thisPtr,
         thisPtr->obType,
	 ddb_obtype_name(thisPtr->obType) );

  switch(thisPtr->obType) {
  case ot_PtFreeFrame:
    printf(" lgsz=%d", objH_ToPage(thisPtr)->kt_u.free.log2Pages);
  case ot_PtDMASecondary:
    goto pgRgn;

  case ot_PtDataPage:
  case ot_PtDevicePage:
  case ot_PtTagPot:
  case ot_PtHomePot:
  case ot_PtLogPot:
    PrintObjData(thisPtr);
  case ot_PtNewAlloc:
  case ot_PtKernelUse:
  case ot_PtSecondary:
  case ot_PtDMABlock:
pgRgn:
    if (objH_ToPage(thisPtr)->ioreq)
      printf(" ioreq=%#x", objH_ToPage(thisPtr)->ioreq);
    if (objH_ToPage(thisPtr)->kernPin)
      printf(" kernPin=%d", objH_ToPage(thisPtr)->kernPin);
    printf(" region=0x%08x\n", objH_ToPage(thisPtr)->physMemRegion);
    break;

  default:
    if (thisPtr->obType > ot_PtLAST_COMMON_PAGE_TYPE) {
      pageH_mdType_dump_header(objH_ToPage(thisPtr));
      goto pgRgn;
    }
    break;

  case ot_NtSegment:
    {
      MapTabHeader * oh = thisPtr->prep_u.products;
      printf(" products=");
      while (oh) {
	printf(" 0x%08x", oh);
	oh = oh->next;
      }
      printf("\n");
      goto prNodeObj;
    }
  case ot_NtProcessRoot:
  case ot_NtKeyRegs:
  case ot_NtRegAnnex:
    printf(" context=0x%08x\n", thisPtr->prep_u.context);
  case ot_NtUnprepared:
prNodeObj:
    PrintObjData(thisPtr);
  case ot_NtFreeFrame:
    break;
  }
}
#endif
