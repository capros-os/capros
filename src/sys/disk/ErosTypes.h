#ifndef __EROSTYPES_H__
#define __EROSTYPES_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

#include <eros/target.h>

/* The target and host programs such as sysgen need to agree on the
alignment and endianness of uint64_t.
Use target_u64 when declaring uint64_t in a structure.
The following works for targets that align uint64_t the same as uint32_t. */

#ifdef __KERNEL__	// if compiling for target

typedef uint64_t target_u64;

INLINE uint64_t
get_target_u64(const target_u64 * p)
{
  return *p;
}

INLINE void
put_target_u64(target_u64 * p, uint64_t v)
{
  *p = v;
}

#else			// compiling for host

#include <eros/endian.h>

typedef struct {
  uint32_t words[2];
} target_u64;

INLINE uint64_t
get_target_u64(const target_u64 * p)
{
  return ((uint64_t)p->words[_QUAD_HIGHWORD] << 32) + p->words[_QUAD_LOWWORD];
}

INLINE void
put_target_u64(target_u64 * p, uint64_t v)
{
  p->words[_QUAD_LOWWORD] = (uint32_t)v;
  p->words[_QUAD_HIGHWORD] = v >> 32;
}

#endif

typedef uint32_t ObCount;

typedef uint64_t OID;
typedef target_u64 OID_s;	// use this in structures
#define get_target_oid(p) get_target_u64(p)
#define put_target_oid(p, v) put_target_u64(p, v)

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

// oid must not be >= OID_RESERVED_PHYSRANGE.
INLINE bool
OIDIsPersistent(OID oid)
{
  return oid >= FIRST_PERSISTENT_OID;
}

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
typedef target_u64 LID_s;	// use this in structures
#define get_target_lid(p) get_target_u64(p)
#define put_target_lid(p, v) put_target_u64(p, v)
#define UNUSED_LID ((LID)0)
#define CONTENT_LID(x) ((x) != UNUSED_LID)
	// LID is the location of contents

// The log begins with the two CkptRoots.
#define MAIN_LOG_START FrameToOID(2)

typedef uint32_t GenNum;	// a generation number

#endif /* ErosTypes.h */
