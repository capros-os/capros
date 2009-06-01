/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, Strawberry Development Group.
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

#define dbg_rescind	0x1
#define dbg_ew		0x2	// EnsureWriteable

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0x0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)


uint16_t objH_CurrentTransaction = 1; /* guarantee nonzero! */

// The maximum allocation count or call count of any nonpersistent object.
ObCount maxNPCount = 0;

// The allocation count and call count of nonpersistent objects at restart.
ObCount restartNPCount = 0;

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
    "PtWkgCopy",
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
  if (objH_IsDirty(pObj)
      && ! objH_IsKRO(pObj) )	// already writeable
    return;

  // Non-persistent objects are always dirty and never KRO,
  // so the object must be persistent:
  assertex(pObj, objH_GetFlags(pObj, OFLG_Cleanable));

  // Can objects being fetched get here?
  assertex(pObj, ! objH_GetFlags(pObj, OFLG_Fetching));

  assert(! InvocationCommitted);

#ifdef OPTION_OB_MOD_CHECK
  if (pObj->check != objH_CalcCheck(pObj))
    fatal("objH_EnsureWritable(%#x): unwriteable and bad checksum!\n",
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
    assertex(pObj, ! objH_IsKRO(pObj));

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
      DEBUG(ew) printf("Log space low, %lld < %d\n",
                       retGenFrames, tentRes);
      goto declareDemarc;
    }

    // Calc space in working area:
    frame_t wkgSpace = OIDToFrame(logCursor - workingGenFirstLid);
    if (logCursor < workingGenFirstLid)	// account for circular log
      wkgSpace += OIDToFrame(logWrapPoint - MAIN_LOG_START);

    if ((wkgSpace + tentRes) > logSizeLimited) { // generation too big
      DEBUG(ew) printf("Gen too big, %d > %d\n",
                       (wkgSpace + tentRes), logSizeLimited);
      goto declareDemarc;
    }

    unsigned long softLimit = objC_nPages + objC_nNodes; // for now

    if (availWorkingDirEnts < softLimit) {	// dir ents getting low
      DEBUG(ew) printf("Low dir ents, %d < %d\n",
                       availWorkingDirEnts, softLimit);

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
    LID oldestNonNextRetiredGenLid = GetOldestNonNextRetiredGenLid();
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

    if (objH_IsKRO(pObj)) {
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
  
  DEBUG(ew) printf("Marked dirty pObj=0x%08x oid=%#llx.\n",
	 pObj, pObj->oid);
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
    // Rescind any unprepared Resume keys:
    node_BumpCallCount(objH_ToNode(thisPtr));

    bool clearedActivity = false;
    if (thisPtr->obType == ot_NtProcessRoot) {
      // Rescind any prepared Resume, Start, or Process keys:
      Process * proc = thisPtr->prep_u.context;
      keyR_RescindAll(&proc->keyRing);
      if (proc->curActivity) {
        // An Activity is similar to the kernel holding a Resume key. Zap it.
        act_DeleteActivity(proc_ClearActivity(proc));
        clearedActivity = true;
      }
    }
    if (! clearedActivity) {
      /* The node could be a process root that is not yet prepared as
      ot_NtProcessRoot or that has hz_DomRoot.
      In these cases there could be an Activity with this OID.
      We must find it and delete it: */
      Activity * act = act_FindByOid(objH_ToNode(thisPtr));
      if (act) {
        assert(! act_HasProcess(act));	// else we would have cleared it above
        /* Note: only the SpaceBank rescinds, and it never rescinds itself: */
        assert(act != act_Current());
        act_Dequeue(act);
        act_DeleteActivity(act);
      }
    }
    
  }

  DEBUG(rescind)
    dprintf(true, "After 'RescindAll()'\n");

  if (objH_GetFlags(thisPtr, OFLG_AllocCntUsed)) {
    // There are unprepared keys (other than Resume). Rescind them:
    thisPtr->allocCount++;

    // Track the maximum count of any nonpersistent object.
    if (! objH_GetFlags(thisPtr, OFLG_Cleanable)
        && thisPtr->allocCount > maxNPCount)
      maxNPCount = thisPtr->allocCount;

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
      && pNode->callCount > maxNPCount)
    maxNPCount = pNode->callCount;

  objH_ClearFlags(node_ToObj(pNode), OFLG_CallCntUsed);

  /* The object must be dirty to ensure that the new count gets saved. */
  assert(objH_IsDirty(node_ToObj(pNode)));
}

// Object must be writeable.
void
objH_ClearObj(ObjectHeader * thisPtr)
{
  if (thisPtr->obType <= ot_NtLAST_NODE_TYPE) {
    Node * thisNode = objH_ToNode(thisPtr);
    /* zeroing unprepares and invalidates products too */

    node_DoClearThisNode(thisNode);
  }
  else if (thisPtr->obType == ot_PtDataPage) {
    objH_InvalidateProducts(thisPtr);

    kva_t pPage = pageH_GetPageVAddr(objH_ToPage(thisPtr));
    kzero((void*)pPage, EROS_PAGE_SIZE);
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
pageH_CalcCheck(PageHeader * pageH)
{
  unsigned int w;
  uint32_t ck = 0;
  
  const uint32_t *  pageData = (const uint32_t *) pageH_MapCoherentRead(pageH);

  for (w = 0; w < EROS_PAGE_SIZE/sizeof(uint32_t); w++)
    ck ^= pageData[w];
  pageH_UnmapCoherentRead(pageH);

  return ck;
}

uint32_t
objH_CalcCheck(ObjectHeader * thisPtr)
{
  if (objH_isNodeType(thisPtr)) {
    return node_CalcCheck(objH_ToNode(thisPtr));
  } else {		// it's a page
    return pageH_CalcCheck(objH_ToPage(thisPtr));
  }
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
  case ot_PtWorkingCopy:
  case ot_PtDevicePage:
  case ot_PtTagPot:
  case ot_PtHomePot:
  case ot_PtLogPot:
    PrintObjData(thisPtr);
    pageH_mdFields_dump_header(objH_ToPage(thisPtr));
  case ot_PtNewAlloc:
  case ot_PtKernelUse:
  case ot_PtSecondary:
  case ot_PtDMABlock:
pgRgn:
    if (objH_ToPage(thisPtr)->ioreq)
      printf(" ioreq=%#x", objH_ToPage(thisPtr)->ioreq);
    printf(" region=0x%08x physAddr=%#x\n",
           objH_ToPage(thisPtr)->physMemRegion,
           pageH_GetPhysAddr(objH_ToPage(thisPtr)));
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
    printf(" proc=%#x\n", thisPtr->prep_u.context);
  case ot_NtUnprepared:
prNodeObj:
    PrintObjData(thisPtr);
  case ot_NtFreeFrame:
    break;
  }
}
#endif
