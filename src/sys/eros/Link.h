#ifndef __LINK_H__
#define __LINK_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
