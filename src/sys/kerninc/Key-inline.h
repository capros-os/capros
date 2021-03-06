#ifndef __KERNINC_KEYINLINE_H__
#define __KERNINC_KEYINLINE_H__
/*
 * Copyright (C) 2008, Strawberry Development Group.
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

#include <kerninc/Key.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Invocation.h>
#include <kerninc/Node.h>
#include <disk/KeyStruct.h>
#include <disk/Key-inline.h>

/* Prepare this key.
 * Also lock the designated object for this transaction.
 * key_Prepare may Yield. */
INLINE void 
key_Prepare(Key * thisPtr)
{
  assert(! InvocationCommitted);
  assert(thisPtr);

  if (keyBits_IsUnprepared(thisPtr))
    key_DoPrepare(thisPtr);
  
  if (keyBits_IsObjectKey(thisPtr)) {
    // The following works for keys that designate a process too,
    // due to a representation pun.
    ObjectHeader * pObj = thisPtr->u.ok.pObj;
      // or proc_ToObj(thisPtr->u.gk.pContext)
    objH_TransLock(pObj);
  }
}

/* Validate this key to be invoked.
 * Also lock the designated object for this transaction.
 * This checks that the key is not rescinded, but does not
 * fetch the object. */
/* If the key is voided, true is returned. Do no further processing. */
// May Yield.
INLINE bool
key_ValidateForInv(Key * thisPtr)
{
  assert(! InvocationCommitted);
  assert(thisPtr);

  if (keyBits_IsUnprepared(thisPtr)) {
    if (key_DoValidate(thisPtr)) {
      VoidKey(&inv);
      return true;
    }
  }
  return false;
}

/* Prepare this key to be invoked.
 * Also lock the designated object for this transaction. */
/* If the key is voided, true is returned. Do no further processing. */
// May Yield.
INLINE bool
key_PrepareForInv(Key * thisPtr)
{
  assert(! InvocationCommitted);
  assert(thisPtr);

  if (keyBits_IsUnprepared(thisPtr)) {
    if (key_DoPrepare(thisPtr)) {
      VoidKey(&inv);
      return true;
    }
  }
  
  if (keyBits_IsObjectKey(thisPtr)) {
    // The following works for keys that designate a process too,
    // due to a representation pun.
    ObjectHeader * pObj = thisPtr->u.ok.pObj;
      // or proc_ToObj(thisPtr->u.gk.pContext)
    objH_TransLock(pObj);
  }
  return false;
}

INLINE void
key_ClearHazard(Key * pKey)
{
  assert(node_ValidNodeKeyPtr(pKey));
  Node * pNode = node_ContainingNode(pKey);
  node_ClearHazard(pNode, pKey - pNode->slot);
}

#endif /* __KERNINC_KEYINLINE_H__ */
