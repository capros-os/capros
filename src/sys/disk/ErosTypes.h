#ifndef __EROSTYPES_H__
#define __EROSTYPES_H__
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

#include <eros/target.h>

typedef uint32_t ObCount;

typedef uint64_t OID;
typedef OID frame_t;	// (64 - 8) bits suffice

// OIDs between 0 and FIRST_PERSISTENT_OID are for non-persistent objects.
#define FIRST_PERSISTENT_OID   0x0100000000000000ull

// The first persistent object is the persistent volsize node,
// which has a well-known OID:
#define PVOLSIZE_OID FIRST_PERSISTENT_OID

// OIDs between FIRST_PERSISTENT_OID and OID_RESERVED_PHYSRANGE
// are for persistent objects.
#define OID_RESERVED_PHYSRANGE 0xff00000000000000ull
// OIDS from OID_RESERVED_PHYSRANGE are for physical pages.

#define EROS_FRAME_FROM_OID(oid) (oid & ~(EROS_OBJECTS_PER_FRAME-1))

INLINE unsigned int
OIDToObIndex(OID oid)
{
  return oid % EROS_OBJECTS_PER_FRAME;
}

INLINE frame_t
OIDToFrame(OID oid)
{
  return oid / EROS_OBJECTS_PER_FRAME;
}

INLINE OID
FrameToOID(frame_t frame)
{
  return frame * EROS_OBJECTS_PER_FRAME;
}

INLINE OID
FrameObIndexToOID(frame_t frame, unsigned int obindex)
{
  return FrameToOID(frame) + obindex;
}

// An identifier of a location in the log:
typedef uint64_t LID;

#endif /* ErosTypes.h */
