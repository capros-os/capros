#ifndef __LOWVOLUME_H__
#define __LOWVOLUME_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2008, 2009, Strawberry Development Group.
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
#include <disk/ErosTypes.h>

/* Low-level portion of the CapROS volume structure.
 * 
 * A CapROS volume (partition) contains:
 * 
 * 	1. A page containing a VolHdr	(Page 0: must be error free)
 * 	2. A page containing an array of NDIVENT Divisions (must be error free)
 * 	3. Other divisions as defined by the user
 */

#define EROS_PARTITION_TYPE 0x95

enum DivType {

#define __DIVDECL(x) dt_##x,
#include <disk/DivTypes.h>
#undef __DIVDECL

  dt_NUMDIV,			/* highest numbered division type */
};
typedef enum DivType DivType;

struct Division {
  // Beware of alignment: this structure is used on both the host and target.
  OID_s startOid;
  OID_s endOid;

  uint32_t start;	// sector # within this volume
  uint32_t end;
  
  uint8_t type;		/* see division type enum, above */
  uint8_t flags;	// at the moment there are no flags
  uint16_t unused1;	// Pad, to avoid alignment differences between
  uint32_t unused2;	//   target and host.
};
typedef struct Division Division;

#ifdef __cplusplus
extern "C" {
#endif
extern const char *div_TypeName(uint8_t ty);
#ifdef __cplusplus
}
#endif

INLINE bool
div_contains(const struct Division *d, const OID oid)
{
  if (get_target_u64(&d->startOid) > oid)
    return false;
  
  if (get_target_u64(&d->endOid) <= oid)
    return false;

  return true;
}

enum {
  NDIVENT = 64
};

#define VOLHDR_VERSION 1

/* Bits in BootFlags: */
enum VolHdrFlags {
  VF_BOOT       = 0x1,
};
typedef enum VolHdrFlags VolHdrFlags;

struct VolHdr {
  // Beware of alignment: this structure is used on both the host and target.
  char        code[8];		/* leading jump instr */
  uint32_t    HdrVersion;	/* contains VOLHDR_VERSION */
  uint32_t    PageSize;		/* contains EROS_PAGE_SIZE */
  uint32_t    DivTable;		/* sector of first division table */
  uint32_t    AltDivTable;	/* sector of alternate division table */
  uint32_t    BootFlags;
  uint32_t    BootSectors;	/* Boot block size */
  uint32_t    VolSectors;	/* number of sectors actually written
				 * to this volume by the formatter.
				 */
  uint32_t    zipLen;		/* unused */
  target_u64  iplSysId;		/* Unique system identifier */

  uint8_t     signature[4];	/* 'E' 'R' 'O' 'S' */
} ;
typedef struct VolHdr VolHdr;

#endif /* __LOWVOLUME_H__ */
