#ifndef __GENERATIONHDR_H__
#define __GENERATIONHDR_H__
/*
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
#include <disk/ErosTypes.h>

struct GenDirHdr {
  LID firstDirFrame;
  uint32_t nDirFrames;
  uint32_t nDescriptors;
};

typedef struct DiskGenerationHdr {
  // Beware of alignment: this structure is used on both the host and target.
  uint32_t versionNumber;
  uint32_t nHeaders;
  uint64_t generationNumber;
  uint64_t migratedGenNumber;
  LID firstLid;
  LID lastLid;
  struct GenDirHdr processDir;
  struct GenDirHdr objectDir;
} DiskGenerationHdr;

// struct DiskProcessDescriptor is packed because
// (1) it needs to be the same on the host and target, and
// (2) it saves space on disk.
struct DiskProcessDescriptor {
  OID oid;
  ObCount allocCount;
} __attribute__ ((packed));

// struct DiskObjectDescriptor is packed because
// (1) it needs to be the same on the host and target, and
// (2) it saves space on disk.
struct DiskObjectDescriptor {
  OID oid;
  ObCount allocCount;
  ObCount callCount;
  LID lid;
  uint8_t allocCountUsed : 1;
  uint8_t callCountUsed : 1;
  uint8_t type : 6;
} __attribute__ ((packed));

#endif // __GENERATIONHDR_H__
