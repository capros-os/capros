#ifndef __LINK_H__
#define __LINK_H__
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

/* This is a common base class for all of the various linked
 * lists. Some objects, including UnitIoReq's, live on multiple lists.
 * 
 * CAUTION! If this representation changes in such a way as to become
 * larger than 2 Words, it will break the representation pun in the
 * ok/gk portions of the key data structure union.
 */

typedef struct Link Link;

struct Link {
  Link *next;
  Link *prev;
} ;

INLINE void
link_Init(Link *thisPtr)
{
  thisPtr->prev = thisPtr;
  thisPtr->next = thisPtr;
}

INLINE void 
link_Unlink(Link * thisPtr) 
{
  Link * nxt = thisPtr->next;
  Link * prv = thisPtr->prev;
  nxt->prev = prv;
  prv->next = nxt;
  link_Init(thisPtr);
}

INLINE void
link_insertAfter(Link *inList, Link *newItem)
{
  newItem->next = inList->next;
  newItem->prev = inList;
  inList->next->prev = newItem;
  inList->next = newItem;
}

INLINE void
link_insertBefore(Link *inList, Link *newItem)
{
  link_insertAfter(inList->prev, newItem);
}

INLINE bool
link_isSingleton(Link *l)
{
  return (l->next == l);
}

#endif /* __LINK_H__ */
