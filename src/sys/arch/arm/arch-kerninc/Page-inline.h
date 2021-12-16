#ifndef __MACHINE_PAGE_INLINE_H__
#define __MACHINE_PAGE_INLINE_H__
/*
 * Copyright (C) 2007, Strawberry Development Group.
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

INLINE void
pageH_MDInitDataPage(PageHeader * pageH)
{
  // A newly-allocated page is not mapped anywhere.
  pageH->kt_u.ob.cacheAddr = CACHEADDR_NONE;
}

INLINE void
pageH_MDInitDevicePage(PageHeader * pageH)
{
  // A device page is considered writeable by the hardware.
  pageH->kt_u.ob.cacheAddr = CACHEADDR_UNCACHED;
}

#ifdef OPTION_OB_MOD_CHECK
INLINE bool
objH_MD_IsUnwriteable(ObjectHeader * pObj)
{
  if (objH_isNodeType(pObj)
      || pObj->obType > ot_PtLAST_OBJECT_TYPE)
    return false;
  // It's an object page.
  // Check the cache class.
  switch (objH_ToPage(pObj)->kt_u.ob.cacheAddr & EROS_PAGE_MASK) {
  default:
    return false;

  case CACHEADDR_NONE:
  case CACHEADDR_ONEREADER:
  case CACHEADDR_READERS:
    return true;
  }
}
#endif

#endif // __MACHINE_PAGE_INLINE_H__
