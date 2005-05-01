#ifndef __KEYRING_H__
#define __KEYRING_H__
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

/* KeyRing is an overlay on both the KeyBits structure and the
 * ObjectHeader structure.
 * The uint32_t filler is a kludge: it allows the first word of the
 * KeyBits structure to be used for the key type, etc.
 * The key type is more frequently accessed, and on some machines
 * the first word can be more efficiently addressed. 
 * The filler can be unioned with a one-word object
 * to avoid wasting the space of the first field.
 */
#include <kerninc/Link.h>

typedef struct Link KeyRing;

/* Former member functions of KeyRing */

/* Resume key zap does not require mustUnprepare, because the kernel
 * guarantees that prepared resume keys only reside in dirty objects.
 */

void keyR_RescindAll(KeyRing *thisPtr, bool mustUnprepare);
void keyR_ZapResumeKeys(KeyRing *thisPtr);
void keyR_UnprepareAll(KeyRing *thisPtr);
bool keyR_HasResumeKeys(const KeyRing *thisPtr);
struct ObjectHeader;
void keyR_ObjectMoved(KeyRing *thisPtr, struct ObjectHeader *);

#ifndef NDEBUG
bool keyR_IsValid(const KeyRing *thisPtr, const void *);
#endif

INLINE bool 
keyR_IsEmpty(const KeyRing *thisPtr)
{
  return (thisPtr->next == thisPtr) ? true : false;
}
  
INLINE void 
keyR_ResetRing(KeyRing *thisPtr)
{
  link_Init(thisPtr);
}

#endif /* __KEYRING_H__ */
