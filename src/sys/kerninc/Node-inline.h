#ifndef __NODE_INLINE_H__
#define __NODE_INLINE_H__
/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

#include <kerninc/Node.h>
#include <kerninc/ObjH-inline.h>
#include <idl/capros/Range.h>

/* If the specified slot (which may be hazarded) in the node
 * is a Resume key, return a pointer to the Resume key,
 * otherwise return NULL. */
// Inline because there is only one caller.
INLINE Key *
node_SlotIsResume(Node * node, unsigned int slot)
{
  Key * pKey = node_GetKeyAtSlot(node, slot);
  if (keyBits_IsRdHazard(pKey)) {
    ObjectHeader * pObj = node_ToObj(node);
    switch (pObj->obType) {
    default:
      assert(false);
    case ot_NtProcessRoot:
      // The only read-hazarded slots in the process root are register
      // values, which are always number keys.
      return NULL;
    case ot_NtKeyRegs:
      // The real key has been moved to the Process structure:
      pKey = &pObj->prep_u.context->keyReg[slot];
    }
  }
  if (! keyBits_IsType(pKey, KKT_Resume))
    return NULL;
  return pKey;
}

void node_DoBumpCallCount(Node * node);

INLINE void
node_BumpCallCount(Node * node)
{
  if (objH_GetFlags(node_ToObj(node), OFLG_CallCntUsed)) {
    node_DoBumpCallCount(node);
  }
}

INLINE Node *         
objH_LookupNode(OID oid)
{
  ObjectHeader * pObj = objH_Lookup(oid, 0);
  if (pObj && objH_GetBaseType(pObj) == capros_Range_otNode)
    return objH_ToNode(pObj);
  else return NULL;
}

#endif // __NODE_INLINE_H__
