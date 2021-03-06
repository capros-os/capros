#ifndef __OBJECTHEADER_H__
#define __OBJECTHEADER_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006-2010, Strawberry Development Group.
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

#include <disk/ErosTypes.h>
#include <kerninc/KeyRing.h>
#include <arch-kerninc/KernTune.h>
#include <eros/container_of.h>

#ifndef __STDKEYTYPE_H__
#include <eros/StdKeyType.h>
#endif

typedef struct Process Process;
typedef struct Node Node;
typedef struct ObjectHeader ObjectHeader;
typedef struct PageHeader PageHeader;
struct PmemInfo;
struct IORequest;

#include <arch-kerninc/PageHeader.h>

extern ObCount restartNPCount;
extern ObCount maxNPCount;

extern unsigned long numDirtyObjectsWorking[];
extern unsigned long numDirtyObjectsNext[];
extern unsigned long numDirtyLogPots;

/* Enable the option OB_MOD_CHECK **in your configuration file** if
 * you are debugging the kernel, and want to verify that modified bits
 * are getting properly set.  Note that this option causes the
 * ObjectHeader structure to grow by a word, which requires that a
 * constant be suitably adjusted in the fast path implementation.  You
 * therefore should NOT simply #define the value here.
 */

/* "Object Type" field values.  These capture simultaneously whether
 * the object is a page or a node and how the object is currently
 * being interpreted.  Note that the interpretation is a runtime
 * artifact, and is never stored to the disk.

 Note, keep the array ddb_obtype_name in sync with this.
 */

enum ObType {
  ot_NtUnprepared,	/* unprepared vanilla node */
  ot_NtSegment,		// a GPT
  ot_NtProcessRoot,
  ot_NtKeyRegs,
  ot_NtRegAnnex,
  ot_NtFreeFrame,	/* unallocated */
  ot_NtLAST_NODE_TYPE = ot_NtFreeFrame,
  ot_PtDataPage,	/* page holding a user data Page */
  ot_PtDevBlock,	/* first frame of a block of device memory */
  ot_PtDMABlock,	/* first frame of a block allocated for DMA. */
  ot_PtSecondary,	/* subsequent frames of a DevBlock or DMABlock. */
	/* Note, the difference between ot_PtDevBlock and ot_PtDMABlock
	is that a DMABlock can be deallocated. */
  ot_PtLAST_OBJECT_TYPE = ot_PtSecondary,	// no objects after here
  ot_PtTagPot,		/* a tag pot. oid = OID of first frame. */
  ot_PtHomePot,		/* an object pot from a home range.
			 oid = OID of first object. */
  ot_PtLogPot,		/* an object pot from the log.
			 oid = LID of the pot. */
  ot_PtWorkingCopy,	/* The working generation version of a data page.
			It has an OID but is not found by objH_Lookup.
			OFLG_KRO must be set. */
  ot_PtNewAlloc,	/* newly allocated frame, not yet typed */
  ot_PtKernelUse,	/* in use as kernel heap or other kernel use */
  ot_PtFreeFrame,	/* first frame of a free block */
  ot_PtFreeSecondary,	/* Part of a multi-page free block, not the first frame.
			No other fields of PageHeader are valid,
			except physMemRegion. */
  ot_PtLAST_COMMON_PAGE_TYPE = ot_PtFreeSecondary
  MD_PAGE_OBTYPES	// machine-dependent types from PageHeader.h
};
typedef enum ObType ObType;

#ifdef OPTION_DDB
extern const char *ddb_obtype_name(uint8_t);
#endif

// Values for objAge:
enum {
  age_NewBorn	= 0,		/* just referenced */
  age_NewObjPot	= 1,		// not as important as user objects.
  age_Invalidate = 2,		/* time to invalidate to see if active */
  age_Clean	= 4,		/* time to clean */
  age_Steal	= 5,		/* time to steal */

  /* Mapping tables have more rapid aging,
  because they are easy to regenerate. */
  age_MTInvalidate = 2,
  age_MTSteal	= 4,
};

// Values in flags:
#define OFLG_DIRTY	0x01	/* object has been modified */
#define OFLG_Fetching	0x02	/* object is being fetched (from disk).
				Should only be on for pages and pots.
				If on, ioreq is non-NULL. */
#define OFLG_Cleanable  0x04	/* object is persistent.
				This is a shortcut for inquring of the object's
				ObjectSource. */
/* unused		0x08	*/
/* unused		0x10	*/
#define OFLG_KRO	0x20	/* object is Kernel-read-only */
#define OFLG_CallCntUsed 0x40	/* resume capabilities to this node exist
				that contain the current call count. */
#define OFLG_AllocCntUsed 0x80	/* non-resume capabilities to this object exist
				that contain the current allocation count. */

struct ObjectHeader {
/* Kludge alert: The first 4 fields here match exactly the corresponding
 * fields of Process(representation pun). */

/* N.B.: obType must be the first item in ObjectHeader.
   This puts it in the same location as PageHeader.kt_u.*.obType. */
  uint8_t obType;		/* page type or node prepcode */

  /* If space is at a premium, obType and objAge could be packed
   * into one byte. */
  uint8_t objAge;

  /* The object is pinned if userPin == objH_CurrentTransaction.
  It may be unlocked in two ways:
  - By setting it to zero (objH_CurrentTransaction is never zero).
  - At the end of a transaction (when we leave the kernel)
    by changing objH_CurrentTransaction.
   */
  uint16_t userPin;

  KeyRing keyRing;

  uint8_t flags;

  OID oid;
  
  ObCount allocCount;

  ObjectHeader * hashChainNext;

  union {
    /* Data for specific obType's */

    /* List of mapping tables produced by this object. */
    MapTabHeader * products;	/* if obType == ot_NtSegment
				        or ot_PtDataPage or ot_PtDevBlock
				        or ot_PtDMABlock or ot_PtSecondary */
    Process * context;		/* if obType == ot_NtProcessRoot
                                             or ot_NtKeyRegs
				             (or ot_NtRegAnnex if used),
                                   this field is valid and non-null. */
    ObjectHeader * nextFree;	/* if obType == ot_NtFreeFrame */
  } prep_u;

#ifdef OPTION_OB_MOD_CHECK
  uint32_t check;		/* check field */
#endif
};

INLINE ObCount
objH_GetAllocCount(const ObjectHeader * pObj)
{
  return pObj->allocCount;
}

INLINE bool
objH_isNodeType(const ObjectHeader * pObj)
{
  return pObj->obType <= ot_NtLAST_NODE_TYPE;
}

INLINE void
objH_SetReferenced(ObjectHeader * pObj)
{
  pObj->objAge = age_NewBorn;
}

struct PageHeader {
/* N.B.: kt_u must be the first item in PageHeader,
  so all the obType fields are in the same location. */
  union {
    struct {
      ObjectHeader obh;
      MD_PAGE_OBFIELDS
    } ob;

    struct {
      uint8_t obType;		/* only ot_PtFreeFrame */

      uint8_t log2Pages;	/* 2**log2Pages is the number of pages
				in this free block */
      Link freeLink;		// link in the free list
    } free;		/* if obType == ot_PtFreeFrame */

    struct {
      uint8_t obType;		/* this is only used by ReservePages in Ckpt.c,
				which lazily leaves obType == ot_PtNewAlloc */

      PageHeader * next;	// next reserved page
    } link;		/* if obType == ot_PtNewAlloc */

    MD_PAGE_VARIANTS

  } kt_u;

  struct PmemInfo * physMemRegion;	// The region this page is in.
  struct IORequest * ioreq;	// NULL iff there is no I/O to this page
};

INLINE PageHeader *
objH_ToPage(ObjectHeader * pObj)
{
  /* the ObjectHeader is the first component of PageHeader,
  so this is the null transformation: */
  return container_of(pObj, PageHeader, kt_u.ob.obh);
}

INLINE const PageHeader *
objH_ToPageConst(const ObjectHeader * pObj)
{
  return container_of(pObj, const PageHeader, kt_u.ob.obh);
}

INLINE ObjectHeader *
pageH_ToObj(PageHeader * pageH)
{
  return &pageH->kt_u.ob.obh;
}

INLINE unsigned int
pageH_GetObType(PageHeader * pageH)
{
  /* obType is in the same location in all variants of kt_u. */
  return pageH_ToObj(pageH)->obType;
}

INLINE void
pageH_SetReferenced(PageHeader * pageH)
{
  objH_SetReferenced(pageH_ToObj(pageH));
}

INLINE bool
pageH_IsFree(PageHeader * pageH)
{
  return pageH_GetObType(pageH) == ot_PtFreeFrame;
}

INLINE bool
pageH_IsObjectType(PageHeader * pageH)
{
  return pageH_GetObType(pageH) <= ot_PtLAST_OBJECT_TYPE;
}

#ifndef NDEBUG
INLINE const ObjectHeader *
keyR_ToObj(const KeyRing * kr)
{
  return container_of(kr, const ObjectHeader, keyRing);
}
#endif

extern uint16_t objH_CurrentTransaction; /* current transaction number */


#ifdef OPTION_OB_MOD_CHECK
uint32_t pageH_CalcCheck(PageHeader * pageH);
uint32_t objH_CalcCheck(ObjectHeader * pObj);
#endif

INLINE void
objH_SetFlags(ObjectHeader* thisPtr, uint32_t w)
{
  thisPtr->flags |= w;
}

INLINE uint32_t 
objH_GetFlags(const ObjectHeader* thisPtr, uint32_t w)
{
  return (thisPtr->flags & w);
}

INLINE void
objH_ClearFlags(ObjectHeader * thisPtr, uint32_t w)
{
  thisPtr->flags &= ~w;
}

INLINE void
pageH_ClearFlags(PageHeader * thisPtr, uint32_t w)
{
  objH_ClearFlags(pageH_ToObj(thisPtr), w);
}

INLINE void
objH_SetDirtyFlag(ObjectHeader* thisPtr)
{
  objH_SetFlags(thisPtr, OFLG_DIRTY);
}

INLINE bool
objH_IsKRO(const ObjectHeader * thisPtr)
{
  return objH_GetFlags(thisPtr, OFLG_KRO);
}

INLINE bool
objH_IsDirty(const ObjectHeader* thisPtr)
{
  return objH_GetFlags(thisPtr, OFLG_DIRTY);
}

INLINE bool
pageH_IsDirty(PageHeader * thisPtr)
{
  return objH_IsDirty(pageH_ToObj(thisPtr));
}

void objH_EnsureWritable(ObjectHeader* thisPtr);
INLINE void
pageH_EnsureWritable(PageHeader * pageH)
{
  objH_EnsureWritable(pageH_ToObj(pageH));
  pageH_SetReferenced(pageH);
}

INLINE void
objH_BeginTransaction()
{
  /* Increment by two, to ensure it is never zero.
     ObjectHeader.userPin == 0 means not pinned. */
  objH_CurrentTransaction += 2;
  /* This addition could overflow. The result is that a few objects
  will be erroneously considered pinned, but that is harmless. */
}
    
INLINE void
objH_TransLock(ObjectHeader* thisPtr)	/* lock for current transaction */
{
  thisPtr->userPin = objH_CurrentTransaction;
  objH_SetReferenced(thisPtr);
}

INLINE bool   
objH_IsUserPinned(ObjectHeader* thisPtr)
{
  return (thisPtr->userPin == objH_CurrentTransaction);
}
  
INLINE void
objH_ResetKeyRing(ObjectHeader* thisPtr)
{
  keyR_ResetRing(&thisPtr->keyRing);
}

void objH_InitObj(ObjectHeader * pObj, OID oid);
void objH_InitPresentObj(ObjectHeader * pObj, OID oid);
void objH_InitDirtyObj(ObjectHeader * pObj, OID oid, unsigned int baseType,
  ObCount allocCount);
void objH_Intern(ObjectHeader* thisPtr);	/* intern object on the ObList. */
void objH_Unintern(ObjectHeader* thisPtr);	/* remove object from the ObList. */

void objH_EnsureNotFetching(ObjectHeader * pObj);

void objH_Rescind(ObjectHeader* thisPtr);
void objH_ClearObj(ObjectHeader * thisPtr);

/* Procedures for handling machine-dependent page ObTypes. */
bool pageH_mdType_CheckPage(PageHeader * pageH, unsigned int * nmtf);
void pageH_mdType_dump_pages(PageHeader * pageH);
void pageH_mdFields_dump_header(PageHeader * pageH);
void pageH_mdType_dump_header(PageHeader * pageH);
void pageH_mdType_EvictFrame(PageHeader * pageH);
bool pageH_mdType_Aging(PageHeader * pageH);

void objH_InvalidateProducts(ObjectHeader * thisPtr);
void objH_AddProduct(ObjectHeader * thisPtr, MapTabHeader * product);
void objH_DelProduct(ObjectHeader * thisPtr, MapTabHeader * product);
  
  /* Machine dependent -- defined in Mapping.c */
void ReleaseProduct(MapTabHeader * mth);

ObjectHeader * objH_Lookup(OID oid, unsigned int type);
  
void objH_StallQueueInit();

struct StallQueue*    objH_ObjectStallQueue(uint32_t ndx);

INLINE struct StallQueue*
ObjectStallQueueFromOID(OID oid)
{
  uint32_t ndx = oid % KTUNE_NOBSLEEPQ;
  return objH_ObjectStallQueue(ndx);
}
  
INLINE struct StallQueue*
ObjectStallQueueFromObHdr(ObjectHeader* thisPtr)
{
  return ObjectStallQueueFromOID(thisPtr->oid);
}

#ifdef OPTION_DDB
void objH_ddb_dump(ObjectHeader* thisPtr);
void objH_ddb_dump_hash_hist();
void objH_ddb_dump_bucket(uint32_t bucket);
#endif

/* PageHeader functions */

kpa_t pageH_GetPhysAddr(const PageHeader * pageH);

INLINE kva_t
pageH_GetPageVAddr(const PageHeader * pageH)
{
  /* This could be made faster by keeping the result in the PageHeader,
  trading space for time. 
  But that only works if all pages are always mapped. */
  return PTOV(pageH_GetPhysAddr(pageH));
}

PageHeader * pageH_MitigateKRO(PageHeader * old);
void node_MitigateKRO(Node * pNode);

// Machine-dependent procedures for coherent mapping:
kva_t pageH_MapCoherentRead(PageHeader * pageH);
void pageH_UnmapCoherentRead(PageHeader * pageH);
kva_t pageH_MapCoherentWrite(PageHeader * pageH);
void pageH_UnmapCoherentWrite(PageHeader * pageH);
void pageH_PrepareForDMAOutput(PageHeader * pageH);
void pageH_PrepareForDMAInput(PageHeader * pageH);

Node * pageH_GetNodeFromPot(PageHeader * pageH, unsigned int obIndex);

/* MEANINGS OF FLAGS FIELDS:
 * 
 * dirty    object has been mutated, and needs to be written to disk.
 * 
 * pinned   object is pinned in memory, and should not be reclaimed.
 * 
 * current  object is the current version of the object.
 * 
 * chkpt    object is the version that was current as of the last
 *          checkpoint.  When the chkpt logic runs, it checks to see
 *          if the object is current and dirty.  If so, it sets the
 *          chkpt, pageOut, and wHazard bits.
 * 
 * wHazard  Object must not be modified, either because their is an
 *          outbound I/O using its contents or it is scheduled for
 *          pageOut.
 * 
 * rHazard  Object content is undefined because an I/O is in progress
 *          into the object. Read/Write at your own risk, without
 *          any guarantees.
 * 
 * pageOut  Object is marked for pageout. Pageout will proceed to
 *          either the current area or the chkpt area depending on the
 *          chkpt bit value.  When pageout is complete, pageOut and
 *          dirty bits will be cleared (assuming no other I/O is in
 *          progress).
 * 
 *          It is usually a side-effect of pageout completion that
 *          ioCount goes to zero.  Whenever ioCount goes to zero,
 *          wHazard and rHazard are cleared.
 * 
 *          If object is no longer current and object is not already
 *          free, its age will be set to Age::Free and it will be
 *          placed on the free list.
 * 
 * When a user goes to modify an object the wHazard bit is first
 * checked to see if the modification can proceed.  If it can, dirty
 * is set to 1.  If it cannot, the object may be COW'd or the user may
 * be forced to wait.
 * 
 * When the checkpointer starts up, it marks all dirty objects with
 * chkpt=1, pageout=1, and wHazard=1.  As they are paged out, pageout
 * and wHazard are cleared.
 * 
 * When all pages have gone out, the migrator comes along and starts
 * to migrate the pages.  Before it reloads each page from the
 * checkpoint area, it checks to see if a version of the page is in
 * memory with chkpt=1 and dirty=0.  If so, that version is the same
 * as the one in the log, and the log version does not need to be
 * reloaded.  Otherwise, it reloads the necessary objects from the
 * log.  In either case, if a version was found in memory the chkpt
 * bit is turned off.
 * 
 * By the end of the migration phase, no chkpt bits should be on.
 * 
 */

/* FREE LIST HANDLING
 * 
 * Objects marked for pageout can be on the free list before they are
 * cleaned.  This creates a problem.  On the one hand, you don't want
 * to preferentially steal clean pages from the free list, because
 * this has the effect of preferentially removing code pages (which
 * are often shared) from memory.  On the other hand, you need to know
 * that there are enough free objects to satisfy any pending disk
 * reads.
 * 
 * The way we handle this is a bit crufty, but it pretty much works.
 * When an object is "placed on the free list", it is first placed on
 * the "free pageout list", WHETHER OR NOT it is dirty.  Pages on the
 * free pageout list are preferentially cleaned by the pageing daemon,
 * and moved to the "real" free list.  If a user-initiated operation
 * would lower the number of objects on the "real" free list below the
 * number of pending read requests, the operation is stalled until
 * there are more clean free objects.
 * 
 * Two optimizations can be done without inducing priority inversion:
 * if an object is clean and not current when it is freed, it can be
 * placed directly onto the "real" free list.  If it is dirty and not
 * current, it is placed at the front of the "free pageout list."  The
 * idea is to preferentially free stale object versions.
 * 
 * To understand the implications of the dual free list design, we
 * need to consider which operations remove objects from the free
 * list:
 * 
 *    1. COW processing
 *    2. Object resurrection
 *    3. Reading in new objects.
 * 
 * Case 1 is relatively simple -- if there aren't enough free frames
 * to COW an object without jeopardizing pending reads, then COWing
 * the object was the wrong thing to do in any case.  The design
 * decision to make is whether the user should wait for a free frame
 * and COW or should wait for the object to go out.  The answer is to
 * wait on the free list.
 * 
 * Case 2 is fairly easy - it doesn't matter what free list the object
 * is on, just hand the user back the object and it will turn into the
 * COW case if the object is dirty.  That's actually good.  If the COW
 * succeeds, the user will proceed, and the dirty object will get
 * moved to the front of the dirty free list for prompt writeout,
 * making it available.  If the COW fails, the next clean page will
 * get allocated to this process, which will probably happen faster
 * than their page will get written out, and in the worst case will
 * only delay them by one write.  In either case, the COW operation
 * will now succeed, and a page has been freed as a result.
 * 
 * Case 3 is also simple - if there are not enough free frames on the
 * clean free list, the reader should block until there are more.
 * 
 * Note that all of this is fairly unlikely, as the pageout daemon
 * tries fairly hard to force dirty pages to the disk before they get
 * to the free list.
 */

#endif /* __OBJECTHEADER_H__ */
