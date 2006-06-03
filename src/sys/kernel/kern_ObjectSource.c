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

#include <kerninc/kernel.h>
#include <kerninc/Node.h>
#include <kerninc/ObjectSource.h>

#include <kerninc/Activity.h>

/* Should this be reorganized to keep nodes and pages separate? */
ObjectHeader *
ObCacheSource_GetObject(ObjectSource *thisPtr, OID oid, ObType obType, 
			 ObCount count, bool useCount)
{
  if (obType == ot_NtUnprepared) {
    Node * pObj = objH_LookupNode(oid);
    assertex(pObj, (pObj == 0) || node_Validate(pObj));
    return node_ToObj(pObj);
  }
  else {
    PageHeader * pObj = objH_LookupPage(oid);
    return pageH_ToObj(pObj);
  }
}

bool
ObCacheSource_WriteBack(ObjectSource *thisPtr, ObjectHeader *pObj, bool inBackground)
{
  return false;
}


bool
ObCacheSource_Invalidate(ObjectSource *thisPtr, ObjectHeader *pObj)
{
  return false;
}

bool
ObCacheSource_Detach(ObjectSource *thisPtr)
{
  fatal("ObCacheSource_Detach(): cannot detach object cache!\n");

  return false;
}

void
ObjectSource_InitObjectSource(ObjectSource *thisPtr, const char *nm, OID first, OID last)
{
  thisPtr->name = nm;
  thisPtr->start = first;
  thisPtr->end = last;
}

bool
ObjectSource_IsRemovable(ObjectSource *thisPtr, ObjectHeader *obHdr)
{
  /* Until proven otherwise: */
  return false;
}

/* A source can publish an incompletely populated OID range. For
 * example, the user source claims the entire persistent OID range,
 * but the machine is unlikely to have that much disk space actually
 * attached.
 *
 * For most in-kernel sources (ROM, Preload), the claimed range is the
 * real range, so it's useful to have a "generic" implementation
 * here. The exceptional case is the ResidentRange, because the number
 * of available resident frames can change as the kernel dynamically
 * (de)allocates memory.
 *
 * FIX: Perhaps this is a bug and we should implement a separate
 * persistent source for each range?
 */
void 
ObjectSource_FindFirstSubrange(ObjectSource *thisPtr, OID limStart, OID limEnd,
				OID* subStart, OID* subEnd)
{
  /* Validate that the requested range and this source really do
   * overlap:
   */
  assert (thisPtr->end >= limStart && thisPtr->start < limEnd);

  /* We shouldn't have been called unless we might have a better
   * answer: 
   */

  assert(thisPtr->start < *subStart);

  *subStart = max(thisPtr->start, limStart);
  *subEnd = min(thisPtr->end, limEnd);
}
