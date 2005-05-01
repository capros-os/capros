#ifndef __NODE_H__
#define __NODE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* Node.hxx: Declaration of a Node. */

#include "ObjectHeader.h"
#include "Key.h"
#include <eros/ProcessState.h>

struct DiskNodeStruct;

/*typedef struct Node Node;*/

struct Node /*: public ObjectHeader*/ {
  /* bool PrepAsDomainSubnode(ObType, Node *parent); */
  
  ObjectHeader node_ObjHdr;
  ObCount callCount;
  
  Key slot[EROS_NODE_SIZE];
    
#if 0
  void InvokeDomainKeeper();	/* using existing fault code */
  void SetDomainFault(uint32_t code, uint32_t auxInfo = 0);
  static void InvokeSegmentKeeper(uint32_t code, uva_t vaddr = 0);
#endif


#if 0
  void ObMovedHazardedSlot(uint32_t ndx, ObjectHeader *pNewLoc);
#endif
  
} ;


/* Former member functions of Node */

INLINE Key *
node_GetKeyAtSlot(Node *thisPtr, int n)
{
  return & thisPtr->slot[n];
}

void node_SetEqualTo(Node *thisPtr, const struct DiskNodeStruct *);

bool node_Validate(Node* thisPtr);

void node_ClearHazard(Node* thisPtr, uint32_t ndx);

void node_UnprepareHazardedSlot(Node* thisPtr, uint32_t ndx);

void node_RescindHazardedSlot(Node* thisPtr, uint32_t ndx, bool mustUnprepare);

/* Prepare node under various sorts of contracts.  In unprepare, set
 * zapMe to true if the *calling* domain should be deprepared in
 * order to satisfy the request.  It shouldn't to satisfy segment
 * walks or to prepare a domain, but it should to set a key in a
 * node slot.
 */
bool node_PrepAsSegment(Node* thisPtr);
void node_PrepAsDomain(Node* thisPtr);
bool node_PrepAsDomainSubnode(Node* thisPtr, ObType, Process*);

Process *node_GetDomainContext(Node* thisPtr);


bool node_Unprepare(Node* thisPtr, bool zapMe);
    
void node_DoClearThisNode(Node* thisPtr);

INLINE void 
node_ClearThisNode(Node* thisPtr)
{
  objH_MakeObjectDirty(DOWNCAST(thisPtr, ObjectHeader));
  node_DoClearThisNode(thisPtr);
}

void node_SetSlot(Node* thisPtr, int ndx, Node* node /*@ NOT NULL @*/, uint32_t otherSlot);

#endif /* __NODE_H__ */
