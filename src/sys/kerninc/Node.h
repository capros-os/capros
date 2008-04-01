#ifndef __NODE_H__
#define __NODE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

#include "ObjectHeader.h"
#include "Key.h"
#include <disk/DiskNodeStruct.h>

struct DiskNodeStruct;
struct Invocation;

/*typedef struct Node Node;*/

struct Node {
  ObjectHeader node_ObjHdr;
  ObCount callCount;

  uint8_t objAge;
  uint8_t kernPin;

  uint16_t nodeData;
  
  Key slot[EROS_NODE_SIZE];
};

INLINE Node *
objH_ToNode(ObjectHeader * pObj)
{
  return container_of(pObj, Node, node_ObjHdr);
}

INLINE const Node *
objH_ToNodeConst(const ObjectHeader * pObj)
{
  return container_of(pObj, const Node, node_ObjHdr);
}

INLINE ObjectHeader *
node_ToObj(Node * pNode)
{
  return &pNode->node_ObjHdr;
}

INLINE Node *         
objH_LookupNode(OID oid)
{
  return objH_ToNode(objH_Lookup(ot_NtUnprepared, oid));
}

INLINE bool
node_IsKernelPinned(Node * thisPtr)
{
  return (thisPtr->kernPin != 0);
}

INLINE void
node_MakeDirty(Node * pNode)
{
  objH_MakeObjectDirty(node_ToObj(pNode));
  pNode->objAge = age_NewBorn;
}

INLINE Key *
node_GetKeyAtSlot(Node *thisPtr, int n)
{
  return & thisPtr->slot[n];
}

INLINE unsigned int
node_GetL2vField(Node * node)
{
  return * node_l2vField(& node->nodeData);
}

INLINE void
node_SetL2vField(Node * node, unsigned int f)
{
  * node_l2vField(& node->nodeData) = f;
}

void node_SetEqualTo(Node *thisPtr, const struct DiskNodeStruct *);

bool node_Validate(Node* thisPtr);

void node_ClearHazard(Node* thisPtr, uint32_t ndx);
void node_ClearAllHazards(Node * thisPtr);

/* Prepare node under various sorts of contracts.  In unprepare, set
 * zapMe to true if the *calling* domain should be deprepared in
 * order to satisfy the request.  It shouldn't to satisfy segment
 * walks or to prepare a domain, but it should to set a key in a
 * node slot.
 */
bool node_PrepAsSegment(Node* thisPtr);

Process * node_GetProcess(Node * thisPtr);

void node_Unprepare(Node * thisPtr);
    
void node_DoClearThisNode(Node* thisPtr);

void node_SetSlot(Node * thisPtr, uint32_t slot, struct Invocation * inv);

void NodeClone(Node * toNode, Key * fromKey);

// The following is architecture-specific.
void node_ClearGPTHazard(Node * thisPtr, uint32_t ndx);

#endif /* __NODE_H__ */
