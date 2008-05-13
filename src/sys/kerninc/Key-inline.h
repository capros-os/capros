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
  
  // FIXME: Why do gate keys not need pinning?
  if (keyBits_IsObjectKey(thisPtr) && !keyBits_IsGateKey(thisPtr))
    objH_TransLock(key_GetObjectPtr(thisPtr));
}

INLINE void
key_ClearHazard(Key * pKey)
{
  Node * pNode = objC_ContainingNode(pKey);
  node_ClearHazard(pNode, pKey - pNode->slot);
}

#endif /* __KERNINC_KEYINLINE_H__ */
