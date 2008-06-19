#ifndef __KEYRING_H__
#define __KEYRING_H__
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

/* KeyRing is an overlay on both the KeyBits structure and the
 * ObjectHeader structure.
 */
#include <eros/Link.h>

typedef struct Link KeyRing;

void keyR_ClearWriteHazard(KeyRing * thisPtr);
void keyR_RescindAll(KeyRing * thisPtr);
void keyR_ZapResumeKeys(KeyRing *thisPtr);
void keyR_UnprepareAll(KeyRing *thisPtr);
void keyR_UnmapAll(KeyRing * thisPtr);
void keyR_TrackReferenced(KeyRing * thisPtr);
void keyR_TrackDirty(KeyRing * thisPtr);
bool keyR_HasResumeKeys(const KeyRing *thisPtr);
struct ObjectHeader;
void keyR_ObjectMoved(KeyRing *thisPtr, struct ObjectHeader *);

#ifndef NDEBUG
bool keyR_IsValid(const KeyRing *thisPtr, const void *);
#endif

INLINE bool 
keyR_IsEmpty(const KeyRing *thisPtr)
{
  return (thisPtr->next == thisPtr);
}
  
INLINE void 
keyR_ResetRing(KeyRing *thisPtr)
{
  link_Init(thisPtr);
}

#endif /* __KEYRING_H__ */
