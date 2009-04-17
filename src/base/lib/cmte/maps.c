/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Thread-safe procedures for the maps area of our address space.
*/

#include <domain/CMTESemaphore.h>
#include <domain/CMTEMaps.h>

static CMTEMutex_DECLARE_Unlocked(mapsLock);

void
maps_liberate(unsigned long pgOffset, unsigned long numPages)
{
  CMTEMutex_lock(&mapsLock);

  maps_liberate_locked(pgOffset, numPages);

  CMTEMutex_unlock(&mapsLock);
}

// Returns page offset within maps area, or -1 if can't allocate.
long
maps_reserve(unsigned long numPages)
{
  CMTEMutex_lock(&mapsLock);

  long block = maps_reserve_locked(numPages);

  CMTEMutex_unlock(&mapsLock);

  return block;
}

// Uses KR_TEMP0 and KR_TEMP1.
result_t
maps_mapPage(unsigned long pgOffset, cap_t pageCap)
{
  CMTEMutex_lock(&mapsLock);

  result_t result = maps_mapPage_locked(pgOffset, pageCap);

  CMTEMutex_unlock(&mapsLock);

  return result;
}
