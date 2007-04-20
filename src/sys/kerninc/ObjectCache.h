#ifndef __OBJECTCACHE_H__
#define __OBJECTCACHE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include "ObjectHeader.h"
#include "PhysMem.h"

typedef struct Depend Depend;
typedef struct ObjectSource ObjectSource;
/*typedef struct PmemInfo PmemInfo;*/

struct ObjectCache {
  
#if 0
  /* For an explanation of why this particular set of calls is needed,
   * see kern_Persist.cxx
   */
  static void ReserveIoPageFrame()
  {
    WaitForAvailablePageFrame();
    nReservedIoPageFrames++;
  }
  static void CommitIoPageFrame()
  {
    nCommittedIoPageFrames = nReservedIoPageFrames;
  }
  static void ReleaseUncommittedIoPageFrames()
  {
    nReservedIoPageFrames = nCommittedIoPageFrames;
  }
  
  static void RequirePageFrames(uint32_t n);
  static ObjectHeader *IoCommitGrabPageFrame();	/* must be prompt! */
#endif

};

extern uint32_t objC_nNodes;
extern Node *objC_nodeTable;

extern uint32_t objC_nPages;
extern PageHeader * objC_coreTable;

extern uint32_t objC_nFreeNodeFrames;
extern uint32_t objC_nFreePageFrames;
extern uint32_t objC_nReservedIoPageFrames;
extern uint32_t objC_nCommittedIoPageFrames;
  
extern Node *objC_firstFreeNode;
extern PageHeader * objC_firstFreePage;


/* Former member functions of ObjectCache */

void objC_AllocateUserPages();

/* Initialization */
void objC_Init();

void objC_InitObjectSources();

/* Page management: */
PageHeader * objC_PhysPageToObHdr(kpa_t pagepa);

ObjectHeader *objC_OIDtoObHdr(uint32_t oidLo, uint16_t oidHi);

void objC_WaitForAvailableNodeFrame();
Node *objC_GrabNodeFrame();

PageHeader * objC_GrabPageFrame();

void objC_AddDevicePages(PmemInfo *);

INLINE uint32_t 
objC_TotalPages()
{
  return objC_nPages;
}

INLINE uint32_t 
objC_TotalNodes()
{
  return objC_nNodes;
}

/* Evict the current resident of the page frame. This is called
 * when we need to claim a particular page frame in the object
 * cache. It is satisfactory to accomplish this by grabbing some
 * other frame and moving the page to it. 
 */
bool objC_EvictFrame(PageHeader * pObj);

/* Releases node/page frame to free list */
void ReleasePageFrame(PageHeader * pageH);
void ReleaseObjPageFrame(PageHeader * pageH);
void ReleaseNodeFrame(Node * pNode);

Node *objC_ContainingNode(void *);

/* For use by the consistency checker: */
INLINE uint32_t 
objC_NumCorePageFrames()
{
  return objC_nPages;
}

INLINE uint32_t 
objC_NumCoreNodeFrames()
{
  return objC_nNodes;
}

PageHeader * objC_GetCorePageFrame(uint32_t ndx);
Node *objC_GetCoreNodeFrame(uint32_t ndx);

#ifndef NDEBUG
bool objC_ValidNodePtr(const Node *pNode);
bool objC_ValidPagePtr(const ObjectHeader *pObj);
bool objC_ValidKeyPtr(const Key *pKey);
#endif

#ifdef OPTION_DDB
void objC_ddb_dump_pinned_objects();
void objC_ddb_dump_pages();
void objC_ddb_dump_nodes();
#endif

/*************************************************************
 *
 * Interaction with ObjectSource(s):
 *
 *************************************************************/

bool objC_AddSource(ObjectSource *);
bool objC_HaveSource(OID oid);
bool objC_DetachSource(ObjectSource *);


#ifdef OPTION_DDB
void objC_ddb_DumpSources();
#endif

ObjectHeader *
objC_GetObject(OID oid, ObType obType, ObCount count, bool useCount);

/* Return true if object can be removed from the object cache. */
bool objC_IsRemovable(ObjectHeader *);

/* Write a page to backing store. Note that the "responsible"
 * ObjectSource can refuse, in which case the page will not be
 * cleanable and will stay in memory. WritePage() is free to yield.
 *
 * Returns true on success.
 */
bool objC_WriteBack(ObjectHeader *, bool inBackground /*@ default false @*/);

/* Unconditionally remove an object from the object cache. 
 *
 * Returns true on success.
 */
bool objC_Invalidate(ObjectHeader *);
  
void objC_FindFirstSubrange(OID start, OID end, 
                            OID* subStart /*@ not null @*/, OID* subEnd /*@ not null @*/);


#endif /* __OBJECTCACHE_H__ */
