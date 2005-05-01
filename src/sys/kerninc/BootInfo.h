#ifndef __BOOTINFO_H__
#define __BOOTINFO_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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
#include <disk/ErosTypes.h>
#include <disk/KeyStruct.h>

/* The SysInfo structure is fabricated by the bootstrap code and
 * passed to the newly loaded kernel. It is currently
 * machine-independent, though at some point I am certain that we will
 * need to pass machine-specific information from the bootstrap/bios
 * to the kernel.
 *
 * However, with the advent of the embedded-style kernel, the majority
 * of the information that the kernel actually needs is in fact
 * machine-independent. It needs to know where physical memory went,
 * and it MAY need to know what drive we booted from so that this
 * information can be passed to the startup code, but that should be
 * about it.
 */

#define MI_UNUSED     0		/* unused entry */
#define MI_MEMORY     1		/* allocatable */
#define MI_RESERVED   2		/* architecturally reserved regions */
#define MI_BOOT       3		/* used by boot logic */
#define MI_PRELOAD    4		/* a preloaded range */
#define MI_DEVICEMEM  5		/* device memory region */
#define MI_BOOTROM    6		/* System ROM area. */
#define MI_RAMDISK  254		/* temporary placeholder while we are
				 * still using the ramdisk boot logic. */
#define MI_UNKNOWN  255		/* unknown region */


struct MemInfo {
  kpa_t     base;
  kpa_t     bound;
  uint32_t  type;		/* address type of this range */
} ;
typedef struct MemInfo MemInfo;
#define MAX_MEMINFO 128

struct DivisionInfo {
  OID startOid;
  OID endOid;

  kpa_t   where;

  /* for meaning of fields below, see LowVolume.hxx */
  uint8_t type;
  uint8_t flags;
};
typedef struct DivisionInfo DivisionInfo;
/* MAX_PRELOAD should be considerably less than MAX_MEMINFO, because
   each preload occupies a MemInfo structure as well. */ 
#define MAX_PRELOAD	64

struct Geometry {		/* as reported by machine-specific BIOS! */
  uint32_t heads;
  uint32_t cylinders;
  uint32_t sectors;
};
typedef struct Geometry Geometry;

/* Structure to describe layout/info of the console frame buffer, if
 * any. If new fields are added to this structure, they should be
 * appended! 
 *
 * I'm not really convinced that this structure is complete yet.
 */
struct ConsoleInfo {
  uint32_t  len;		/* length of ConsoleInfo, in bytes
				 * (for versioning) */
  uint32_t  videoMode;		/* machine specific value */
  kpa_t     frameBuffer;	/* frame buffer pointer */
  uint32_t  Xlimit;		/* X dimension, in bytes */
  uint32_t  Ylimit;		/* Y dimension, in bytes */
  uint32_t  winSize;		/* size of window, in bytes */
  uint32_t  bytesPerScanLine;	/* number of bytes (horizontal) per
				 * scan line */

  uint8_t   isBanked;		/* true iff banked mode, else linear */
  uint8_t   bpp;		/* bits per pixel */
  uint8_t   redMask;		/* number of red bits */
  uint8_t   blueMask;		/* number of blue bits */
  uint8_t   greenMask;		/* number of green bits */
  uint8_t   redShift;		/* shift to red bit pos */
  uint8_t   blueShift;		/* shift to blue bit pos */
  uint8_t   greenShift;		/* shift to green bit pos */
};
typedef struct ConsoleInfo ConsoleInfo;

struct BootInfo {
  uint32_t  volFlags;		/* defined in LowVolume.h */

  Geometry  bootGeom;		/* boot geometry as seen by the bootstrap */
  uint32_t  bootDrive;		/* boot drive as seen by the bootstrap */
  uint32_t  bootStartSec;	/* sector offset of boot image
				 * relative to start of original boot
				 * media. */
  bool_t    isRamImage;		/* was the boot a ram image (including
				   ramDisk) */

  MemInfo  *memInfo;		/* pointer to the bootstrap memInfo array */
  uint32_t nMemInfo;

  DivisionInfo *divInfo;	/* pointer to preloaded divisions */
  uint32_t nDivInfo;

  ConsoleInfo *consInfo;	/* Graphics framebuffer, if present */
  bool_t   useGraphicsFB;	/* true iff should use console FB */

  uint64_t iplSysId;		/* Unique system identifier */
  KeyBits iplKey;
} ;

typedef struct BootInfo BootInfo;

#ifdef __KERNEL__
#ifdef __cplusplus
extern "C" {
#endif
extern BootInfo * BootInfoPtr;
#ifdef __cplusplus
}
#endif
#endif

#endif /* __BOOTINFO_H__ */
