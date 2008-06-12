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

/* Note, ObjectRange is used for both object and log ranges,
 * and an OID is the same size as a LID. 
 * Log ranges can only have source = IOObSource. */
typedef struct ObjectRange {
  OID start;
  OID end;	/* last OID +1 */
  const struct ObjectSource * source;
  union {
    PmemInfo *pmi;
    struct {
      struct IORQ * iorq;
      uint32_t opaque;
    } rq;
  } u;
} ObjectRange;

/**********************************************************************
 *
 * ObjectLocator stuff:
 *
 **********************************************************************/

enum {
  objLoc_ObjectHeader,
  objLoc_TagPot,
  objLoc_Preload
};

typedef struct ObjectLocator {
  unsigned int locType;
  unsigned int objType;		// base type of the object
			// may be capros_Range_otPage, capros_Range_otNode,
			// or capros_Range_otNone if type is not determined
  union {
    // If locType == objLoc_ObjectHeader:
    ObjectHeader * objH;

    // If locType == objLoc_TagPot:
    struct {
       ObjectRange * range;
       PageHeader * tagPotPageH;
       unsigned int potEntry;
    } tagPot;

    // If locType == objLoc_Preload:
    struct {
       ObjectRange * range;
    } preload;
  } u;
} ObjectLocator;

ObjectLocator GetObjectType(OID oid);
ObCount GetObjectCount(OID oid, ObjectLocator * pObjLoc, bool callCount);
ObjectHeader * GetObject(OID oid, const ObjectLocator * pObjLoc);

/* The ObjectSource structure defines a producer/consumer of objects.
 * The ObjectRange structure defines a producer/consumer of objects
 * in a specific range of OIDs. */

typedef struct ObjectSource {
  const char *name;

  /* Note: the Get* methods may evict any unpinned object.
   * The evictor may in turn call a Write* method.
   * Therefore, executing Get* must not preclude executing Write*
   * on the same ObjectRange prior to completing the Get*. */
  /* The Get* and Write* methods may Yield. */

  ObjectLocator 
  (*objS_GetObjectType)(ObjectRange * rng, OID oid);

  ObCount
  (*objS_GetObjectCount)(ObjectRange * rng, OID oid,
                         ObjectLocator * pObjLoc, bool callCount);

  ObjectHeader * 
  (*objS_GetObject)(ObjectRange * rng, OID oid,
                    const ObjectLocator * pObjLoc);
  
  /* Write a frame to the range.
   * This will only be called for the IOObSource. */
  void
  (*objS_WriteRangeLoc)(ObjectRange * rng, frame_t rangeLoc,
                        PageHeader * pageH);
} ObjectSource;

bool objC_AddRange(const ObjectRange * rng);
bool AddLIDRange(const ObjectRange * rng);

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
