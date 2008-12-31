#ifndef __LINK_H__
#define __LINK_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
};

#define link_Initializer(link) { .next = &(link), .prev = &(link) }

INLINE void
link_Init(Link * thisPtr)
{
  thisPtr->prev = thisPtr;
  thisPtr->next = thisPtr;
}

// Leaves thisPtr->prev and next undefined.
INLINE void 
link_UnlinkUnsafe(Link * thisPtr) 
{
  Link * nxt = thisPtr->next;
  Link * prv = thisPtr->prev;
  nxt->prev = prv;
  prv->next = nxt;
#ifndef NDEBUG
  thisPtr->next = NULL;
  thisPtr->prev = NULL;
#endif
}

INLINE void 
link_Unlink(Link * thisPtr) 
{
  link_UnlinkUnsafe(thisPtr);
  link_Init(thisPtr);
}

// This is faster when you have both pointers.
INLINE void
link_insertBetween(Link * thisPtr, Link * p, Link * n)
{
  p->next = n->prev = thisPtr;
  thisPtr->prev = p;
  thisPtr->next = n;
}

// Insert thisPtr after head p.
INLINE void
link_insertAfter(Link * p, Link * thisPtr)
{
  link_insertBetween(thisPtr, p, p->next);
}

// Insert thisPtr before head n.
INLINE void
link_insertBefore(Link * n, Link * thisPtr)
{
  link_insertBetween(thisPtr, n->prev, n);
}

INLINE bool
link_isSingleton(const Link * l)
{
  return (l->next == l);
}

#endif /* __LINK_H__ */
