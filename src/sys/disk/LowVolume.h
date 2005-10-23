#ifndef __LOWVOLUME_HXX__
#define __LOWVOLUME_HXX__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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

#include <eros/target.h>
#include <disk/KeyStruct.h>

/* Low-level portion of the EROS volume structure.
 * This header file is included by things like boot code.  Statically
 * allocated objects in this header or anything it includes are a
 * profoundly bad idea.
 * 
 * EROS volume structure.  EROS volumes contain:
 * 
 * 	1. A Boot Page		(Page 0: must be error free)
 * 	2. A Division Table	(Page 1: must be error free)
 * 	3. A spare sector region
 * 	3. Other divisions as defined by the user
 */

/* In principle, these classes probably ought to live in distinct
 * header files.  In practice, there is essentially nothing you can do
 * with a volume unless you have the division table and the bad map
 * table.  Rather than keep the relationships in multiple files, it
 * seemed better to put them all in one place.
 */

enum DivType {

#define __DIVDECL(x) dt_##x,
#include <disk/DivTypes.h>
#undef __DIVDECL

  dt_NUMDIV,			/* highest numbered division type */
};
typedef enum DivType DivType;

#ifdef __KERNEL__
#define BOOT_DISK_DIVISION 0xffff
#endif

#define DF_PRELOAD  0x1		/* range should be preloaded */

struct Division {
  uint32_t start;
  uint32_t end;
  
  OID startOid;
  OID endOid;

  uint8_t type;		/* see division type enum, above */
  uint8_t flags;
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
  if (d->startOid > oid)
    return false;
  
  if (d->endOid <= oid)
    return false;

  return true;
}

#if 0
/* Bad sector mapping info. */
struct BadEnt {
  unsigned long badSec;	/* the sector being replaced */
  unsigned long goodSec;	/* the sector we replaced it with */

  BadEnt()
  { badSec = 0; goodSec = 0; }
} ;
#endif

enum {
  NDIVENT = 64,
#if 0
  NBADENT = ((EROS_PAGE_SIZE - (64*sizeof(Division))) /
	     sizeof(BadEnt))
#endif
};

#if defined(i386) || defined(i486)
#define VOLHDR_VERSION 1

/* Bits in BootFlags: */
enum VolHdrFlags {
  VF_BOOT       = 0x1,
  VF_DEBUG      = 0x80000000,	/* Debugging boot */
};
typedef enum VolHdrFlags VolHdrFlags;

struct VolHdr {
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
  KeyBits     iplKey;		/* unique singleton process to start */
  uint64_t    iplSysId;		/* Unique system identifier */

  uint8_t     signature[4];	/* 'E' 'R' 'O' 'S' */
} ;
typedef struct VolHdr VolHdr;
#endif

#endif /* __VOLUME_HXX__ */
