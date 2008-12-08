#ifndef __OBJH_INLINE_H_
#define __OBJH_INLINE_H_
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

#include <kerninc/ObjectHeader.h>
#include <kerninc/Depend.h>
#include <arch-kerninc/Page-inline.h>
#include <idl/capros/Range.h>

INLINE unsigned int
objH_GetBaseType(ObjectHeader * pObj)
{
  if (pObj->obType <= ot_NtLAST_NODE_TYPE)
    return capros_Range_otNode;
  else return capros_Range_otPage;
}

INLINE ObType
BaseTypeToObType(unsigned int baseType)
{
  switch (baseType) {
  default: ;
    assert(false);

  case capros_Range_otNode:
    return ot_NtUnprepared;

  case capros_Range_otPage:
    return ot_PtDataPage;
  }
}

INLINE void
pageH_MakeReadOnly(PageHeader * pageH)
{
  keyR_ProcessAllMaps(&pageH_ToObj(pageH)->keyRing,
                      &KeyDependEntry_MakeRO);
}

#ifdef OPTION_OB_MOD_CHECK
INLINE bool
objH_IsUnwriteable(ObjectHeader * pObj)
{
  return ! objH_IsDirty(pObj)
         || objH_GetFlags(pObj, OFLG_KRO)
         || objH_MD_IsUnwriteable(pObj);
}

INLINE void
node_SetCheck(Node * pNode)
{
  node_ToObj(pNode)->check = node_CalcCheck(pNode);
}

INLINE void
pageH_SetCheck(PageHeader * pageH)
{
  pageH_ToObj(pageH)->check = pageH_CalcCheck(pageH);
}

INLINE void
objH_SetCheck(ObjectHeader * pObj)
{
  pObj->check = objH_CalcCheck(pObj);
}
#endif

// Call this just BEFORE making the object unwriteable.
INLINE void
nodeH_BecomeUnwriteable(Node * pNode)
{ 
#ifdef OPTION_OB_MOD_CHECK
  // if not previously unwriteable:
  if (! objH_IsUnwriteable(node_ToObj(pNode)))
    node_SetCheck(pNode);
#endif
}

// Call this just BEFORE making the object unwriteable.
INLINE void
pageH_BecomeUnwriteable(PageHeader * pageH)
{ 
#ifdef OPTION_OB_MOD_CHECK
  // if not previously unwriteable:
  if (! objH_IsUnwriteable(pageH_ToObj(pageH)))
    pageH_SetCheck(pageH);
#endif
}

#endif // __OBJH_INLINE_H_
