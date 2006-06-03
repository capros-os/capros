#ifndef __OBJECTHEADER_H__
#define __OBJECTHEADER_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <disk/ErosTypes.h>
#include <kerninc/KeyRing.h>
#include <arch-kerninc/KernTune.h>

#ifndef __STDKEYTYPE_H__
#include <eros/StdKeyType.h>
#endif

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
 */

enum ObType {
  ot_NtUnprepared,		/* unprepared vanilla node */
  ot_NtSegment,
  ot_NtProcessRoot,
  ot_NtKeyRegs,
  ot_NtRegAnnex,
  ot_NtFreeFrame,		/* unallocated */
  ot_NtLAST_NODE_TYPE = ot_NtFreeFrame,
  ot_PtDataPage,		/* page holding a user data Page */
  ot_PtNewAlloc,		/* newly allocated frame, not yet typed */
  ot_PtKernelHeap,		/* in use as kernel heap */
#ifdef USES_MAPPING_PAGES
  ot_PtMappingPage,		/* used in a virtual mapping table */
#endif
  ot_PtDevicePage,		/* data page, but device memory */
  ot_PtFreeFrame		/* unallocated */
};
typedef enum ObType ObType;

#ifdef OPTION_DDB
extern const char *ddb_obtype_name(uint8_t);
#endif

/* OBJECT AGING POLICY:
 * 
 * Objects are brought in as NewBorn objects, and age until they reach
 * the Invalidate generation.  At that point all outstanding keys are
 * deprepared.  If they make it to the PageOut generation we kick them
 * out of memory (writing if necessary).
 * 
 * When an object is prepared, we conclude that it is an important
 * object, and promote it back to the NewBorn generation.
 * 
 * PROJECT: Some student should examine the issues associated with
 * aging policy management.
 */

/*struct Age {*/
enum {
  age_NewBorn = 0,		/* just loaded into memory */
  age_LiveProc = 1,		/* node age for live processes */
  age_NewLogPg = 2,		/* not as important as user pages. */
  age_Invalidate = 6,		/* time to invalidate to see if active */
  age_PageOut = 7,		/* time to page out if dirty */
};
/*};*/


typedef struct SegWalk SegWalk;
typedef struct ObjectTable ObjectTable;
typedef struct Process Process;
typedef struct Node Node;
typedef struct ObjectHeader PageHeader;

#define OHAZARD_NONE      0x1u
#define OHAZARD_WRITE     0x1u
#define OHAZARD_READ      0x2u
#define OHAZARD_READWRITE 0x2u


#define OFLG_DIRTY	0x01u	/* object has been modified */
#define OFLG_REDIRTY	0x02u	/* object has been modified since */
				/* write initiated */
#if 0
#define OFLG_PIN	0x04u	/* object pinned in memory */
#endif
#define OFLG_CURRENT	0x08u	/* current version */
#define OFLG_CKPT	0x10u	/* checkpoint version */
#define OFLG_IO		0x20u	/* object has active I/O in progress */
#define OFLG_DISKCAPS	0x40u	/* capabilities to this version exist */
				/* on disk */

typedef struct ObjectHeader ObjectHeader;
struct ObjectHeader {
  KeyRing	keyRing;

  union {
    /* Special relationship pointers if prepared node */
    PageHeader * products;	/* if obType == ot_NtSegment
				             or ot_PtDataPage or ... */
    Process	  *context;	/* if obType == ot_NtProcessRoot
                                             or ot_NtKeyRegs
				             (or ot_NtRegAnnex if used) */
  } prep_u;

  kva_t   pageAddr;		/* speed up ObHdrToPage! */
  
  ObjectHeader   *next;		/* used by various chains */
  
  union {
    struct {
      ObCount	allocCount;
      OID   	oid;

#ifdef OPTION_OB_MOD_CHECK
      uint32_t		check;		/* check field */
#endif

      /* Note: when I do background keys they should be unioned with the
       * allocation count, since they only apply to mapping pages.
       */

      uint32_t	ioCount;	/* for object frames */
    } ob;	/* if obType is NOT one of the following:
		ot_NtFreeFrame, ot_PtNewAlloc, ot_PtKernelHeap,
		ot_PtMappingPage, ot_PtFreeFrame */

    struct {
      ObjectHeader * producer;
      struct Node *redSeg;	/* pointer to slot of keeper that
				 * dominated this mapping frame */
      unsigned char redSpanBlss;	/* blss of seg spanned by redSeg */
      bool wrapperProducer;
      uint8_t producerBlss; /* biased lss of map tbl producer.
			       NOTE: not the key, the object. */
      uint8_t rwProduct    : 1;	/* indicates mapping page is RW version */
      uint8_t caProduct    : 1;	/* indicates mapping page is callable version */
      /* The following fields are architecture-dependent,
         but I'm not going to reorganize them now,
         because this whole structure is architecture-dependent.
         A page could have more than one mapping table, or fewer than one. */
      uint8_t tableSize    : 1;
      uint8_t producerNdx  : 4;
      ula_t tableCacheAddr;
    } mp;	/* if obType == ot_PtMappingPage */
    
  } kt_u;
  
  uint8_t	flags;
  
  uint8_t	obType;		/* page type or node prepcode */
  uint8_t	age;

  uint8_t		userPin;
  uint8_t		kernPin;

  uint8_t ssid;		/* (Machine-dependent)
                           used if obType == ot_NtSegment
                                             or ot_PtDataPage or ... */
  
  ObjectHeader* hashChainNext;
};

INLINE unsigned int
pageH_GetObType(PageHeader * pageH)
{
  return pageH->obType;
}

INLINE bool
pageH_IsObjectType(PageHeader * pageH)
{
  unsigned int type = pageH_GetObType(pageH);
  return (type == ot_PtDataPage) || (type == ot_PtDevicePage);
}

INLINE PageHeader *
objH_ToPage(ObjectHeader * pObj)
{
  return (PageHeader *)pObj;
}

INLINE ObjectHeader *
pageH_ToObj(PageHeader * pageH)
{
  return (ObjectHeader *)pageH;
}

#ifndef NDEBUG
INLINE ObjectHeader *
keyR_ToObj(KeyRing * kr)
{
  return (ObjectHeader *) ((char *)kr - offsetof(ObjectHeader, keyRing));
}
#endif

extern uint8_t objH_CurrentTransaction; /* current transaction number */


/* Former member functions of ObjectHeader */

#ifdef OPTION_OB_MOD_CHECK
uint32_t objH_CalcCheck(const ObjectHeader* thisPtr);
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
objH_ClearFlags(ObjectHeader* thisPtr, uint32_t w)
{
    thisPtr->flags &= ~w;
}

INLINE void 
objH_SetDirtyFlag(ObjectHeader* thisPtr)
{
  thisPtr->flags |= (OFLG_DIRTY|OFLG_REDIRTY);
}

INLINE uint32_t 
objH_IsDirty(const ObjectHeader* thisPtr)
{
  return objH_GetFlags(thisPtr, OFLG_DIRTY|OFLG_REDIRTY);
}

INLINE uint32_t 
pageH_IsDirty(PageHeader * thisPtr)
{
  return objH_IsDirty(pageH_ToObj(thisPtr));
}

void objH_MakeObjectDirty(ObjectHeader* thisPtr);
INLINE void
pageH_MakeDirty(PageHeader * pageH)
{
  objH_MakeObjectDirty(pageH_ToObj(pageH));
}

  /* Object pin counts.  For the moment, there are several in order to
   * let me debug the logic.  Eventually they should all be mergeable.
   * 
   *   userPin -- pins held by the user activity.  Automatically
   *              released whenever the activity yields or leaves the
   * 		  kernel.
   * 
   *   kernPin -- pins held by a kernel activity.  Must be released
   *              explicitly.
   * 
   * userPin and kernPin are acquired and released via the same
   * interface -- Pin/Unpin.  Unpin is a no-op if the caller is a user
   * activity.
   * 
   * If ANY of the pin counts is nonzero, the 'pinned' bit is set.
   * 
   */
  
INLINE void 
objH_BeginTransaction()
{
  objH_CurrentTransaction += 2;
}
    
void          objH_KernPin(ObjectHeader* thisPtr);	/* object is pinned for kernel reasons */
void          objH_KernUnpin(ObjectHeader* thisPtr);	/* object is pinned for kernel reasons */

#ifdef NDEBUG
INLINE void 
objH_TransLock(ObjectHeader* thisPtr)	/* lock for current transaction */
{
  thisPtr->userPin = objH_CurrentTransaction;
}
INLINE void 
objH_TransUnlock(ObjectHeader* thisPtr)
{
  thisPtr->userPin = 0;
}
#else
void objH_TransLock(ObjectHeader* thisPtr);	/* lock for current transaction */
void objH_TransUnlock(ObjectHeader* thisPtr);
#endif

INLINE void	
objH_ReleasePinnedObjects()
{
}

INLINE bool   
objH_IsUserPinned(ObjectHeader* thisPtr)
{
  return (thisPtr->userPin == objH_CurrentTransaction);
}

INLINE bool   
objH_IsKernelPinned(ObjectHeader* thisPtr)
{
  return (thisPtr->kernPin != 0);
}
  
INLINE void 
objH_ResetKeyRing(ObjectHeader* thisPtr)
{
  keyR_ResetRing(&thisPtr->keyRing);
}

void 		objH_Intern(ObjectHeader* thisPtr);	/* intern object on the ObList. */
void 		objH_Unintern(ObjectHeader* thisPtr);	/* remove object from the ObList. */

void		objH_FlushIfCkpt(ObjectHeader* thisPtr);
void		objH_Rescind(ObjectHeader* thisPtr);
void		objH_ZapResumeKeys(ObjectHeader* thisPtr);

void            objH_InvalidateProducts(ObjectHeader* thisPtr);
void            objH_AddProduct(ObjectHeader* thisPtr, ObjectHeader *product);
void            objH_DelProduct(ObjectHeader* thisPtr, ObjectHeader *product);

ObjectHeader  * objH_Lookup(ObType, OID oid);

INLINE PageHeader *         
objH_LookupPage(OID oid)
{
  return objH_ToPage(objH_Lookup(ot_PtDataPage, oid));
}
  
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
  return ObjectStallQueueFromOID(thisPtr->kt_u.ob.oid);
}

#ifdef OPTION_DDB
void objH_ddb_dump(ObjectHeader* thisPtr);
void objH_ddb_dump_hash_hist();
void objH_ddb_dump_bucket(uint32_t bucket);
#endif

/* PageHeader functions */

INLINE kva_t
pageH_GetPageVAddr(const PageHeader * pHdr)
{
  return pHdr->pageAddr;
}

/* MEANINGS OF FLAGS FIELDS:
 * 
 * free     object is on free list, and may be reclaimed. ObjectTable*
 *          points to a free list linkage.
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
 * ioActive One or more I/Os are in progress on this object.  Implies
 *          ioCount > 0.
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
