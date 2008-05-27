#ifndef __OBJECTSOURCE_H__
#define __OBJECTSOURCE_H__
/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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
#include <kerninc/PhysMem.h>
#include <kerninc/ObjectCache.h>

struct ObjectSource;

typedef struct ObjectRange {
  OID start;
  OID end;	/* last OID +1 */
  const struct ObjectSource * source;
  PmemInfo *pmi;
} ObjectRange;

/* The ObjectSource structure defines a producer/consumer of objects.
 * The ObjectRange structure defines a producer/consumer of objects
 * in a specific range of OIDs. */

struct ObjectSource {
  const char *name;
  
  /* Fetch page (node) from the backing implementation for this
     range. On success, return an ObjectHeader (Node) pointer for
     the requested object, which has been brought into memory (if
     needed). On failure, return null pointer.

     It is entirely up to the range provider whether the presenting
     the object will cause some existing entry in the object cache to
     be evicted. For example, a ROM range may leave the object in ROM
     until a copy on write is performed.  In the event that an object
     needs to be evicted, it is the responsibility of the range
     provider to choose a frame and tell the object cache manager to
     evict the current resident. This is often done by allowing the
     object cache ager to choose the frame.

     Note that the object cache evictor will very likely implement
     eviction by turning around and calling ObjectRange::Evict() for
     some range. Therefore, executing an inbound path (the
     GetPage/GetNode cases) must not preclude calling the outbound
     path on the same ObjectRange (the Evict procedure) prior to
     completion of the inbound path.

     The GetPage()/GetNode() implementation is free to yield.
  */
     
  ObjectHeader *
  (*objS_GetObject)(ObjectRange * thisPtr, OID oid, ObType obType, ObCount count, bool useCount);

  /* Write a page to backing store. Note that the "responsible"
   * ObjectSource can refuse, in which case the page will not be
   * cleanable and will stay in memory. WritePage() is free to yield.
   */
  bool (*objS_WriteBack)(ObjectRange * thisPtr, ObjectHeader *obHdr, 
                         bool inBackground /*@ default = false @*/);
};

bool objC_AddRange(ObjectRange * rng);

/**********************************************************************
 *
 * Any preloaded ram regions are object sources
 *
 **********************************************************************/

void PreloadObSource_Init(void);

/**********************************************************************
 *
 * The "Physical Page Range" is an object source
 *
 **********************************************************************/

void PhysPageObSource_Init(void);
void PhysPageSource_Init(PmemInfo * pmi);

#endif /* __OBJECTSOURCE_H__ */
