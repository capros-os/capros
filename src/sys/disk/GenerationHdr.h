#ifndef __GENERATIONHDR_H__
#define __GENERATIONHDR_H__
/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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
#include <disk/ErosTypes.h>
#include <string.h>

struct GenDirHdr {
  LID_s firstDirFrame;
  uint32_t nDirFrames;
  uint32_t nDescriptors;	// number of descriptors in the header frame
};

typedef struct DiskGenerationHdr {
  // Beware of alignment: this structure is used on both the host and target.
  uint32_t versionNumber;
  //// uint32_t unused;	// for alignment
  target_u64 generationNumber;
  LID_s firstLid;
  LID_s lastLid;
  struct GenDirHdr objectDir;

  /* The following could be in the CkptRoot, but putting them here helps
  if we ever support restarting from generations older than the newest. */

  struct GenDirHdr processDir;

  target_u64 persistentTimeOfDemarc;	// in units of nanoseconds
  unsigned long RTCOfDemarc;	// see RTC.idl for units
} DiskGenerationHdr;

// struct DiskProcessDescriptor is packed because
// (1) it needs to be the same on the host and target, and
// (2) it saves space on disk.
struct DiskProcessDescriptor {
  OID_s oid;
  ObCount callCount;
  uint8_t actHazard;
} __attribute__ ((packed));

INLINE OID 
GetDiskProcessDescriptorOid(const struct DiskProcessDescriptor * dpd)
{
  // Must use memcpy since DiskProcessDescriptor is packed.
  OID_s oids;
  memcpy(&oids, &dpd->oid, sizeof(oids));
  return get_target_oid(&oids);
}

#endif // __GENERATIONHDR_H__
