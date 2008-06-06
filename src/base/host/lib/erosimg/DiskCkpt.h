#ifndef __DISKCKPT_H__
#define __DISKCKPT_H__
/*
 * Copyright (C) 1998, 1999, 2002, Jonathan S. Shapiro.
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

/* On-disk structures for the checkpoint log.  Assumes that the
 * LogLoc's are 32 bits, which is sufficient for 17,592,186,044,416
 * pages :-)  The analogous in-core checkpoint directory restricts
 * LogLoc's to 28 bits (it steals four bits for various flags), so
 * the realizable checkpoint log size in this implementation is actually
 * only 1,099,511,627,776.  Since current generation bus technology
 * can only move 41,838,182,400 bytes within the 5 minute checkpoint
 * interval, I don't anticipate that this will be a serious
 * shortcoming for a while.  By that point main memory will be cheap
 * enough to justify a larger in-core ckpt directory structure.
 */

#include <disk/ErosTypes.h>
#include <eros/Reserve.h>

/* Checkpoint directory pages either hold page descriptors or cgroup
 * descriptors, but not both.
 */

// FIXME this is wrong, but at least it compiles:
typedef uint32_t lid_t;
#define ZERO_LID 0
#define UNDEF_LID 1
#define CONTENT_LID(x) (x >= EROS_OBJECTS_PER_FRAME)

enum {
 ck_MaxLidValue = 0x0fffffffu,
};

typedef struct CkptDirent CkptDirent;
struct CkptDirent {
  OID       oid;
  ObCount   count;
  lid_t     lid : 28;	/* NULL_LOGLOC == zero page */
  uint8_t   type : 4;

} ;

/* Each directory page contains a type (page or node) a count
 * of the valid entries (starting from zero), and some number of page
 * or node directory entries.  The derivation hack is just to make
 * computing the array sizes simpler.
 */

typedef struct DirPageHdr DirPageHdr;
struct DirPageHdr {
  uint32_t       nDirent;
};

enum {
  ckdp_maxDirEnt = (EROS_PAGE_SIZE - sizeof(DirPageHdr)) / sizeof(CkptDirent),
};

typedef struct CkptDirPage CkptDirPage;
struct CkptDirPage {
  DirPageHdr hdr;
  CkptDirent entry[ckdp_maxDirEnt];
};

/* Note that while we use the CpuReserveInfo structure, we do NOT
 * convert the times back to milliseconds at the moment.  This may
 * create a problem later when rebooting on a machine with a different
 * HW clock.
 */
enum {
  rdp_maxDirEnt = (EROS_PAGE_SIZE - sizeof(DirPageHdr)) / sizeof(CpuReserveInfo),
};

typedef struct ReserveDirPage ReserveDirPage;
struct ReserveDirPage {
  DirPageHdr     hdr;
  CpuReserveInfo entry[rdp_maxDirEnt];
};

/* Each thread has an entry in the thread directory in ADDITION to a
 * directory entry for the thread's current domain.  Since threads can
 * sleep a long time, there is no guarantee that a sleeping thread's
 * domain root is included in a given checkpoint.  Separating the
 * thread table simplifies the bookkeeping a lot.
 */

typedef struct ThreadDirent ThreadDirent;
struct ThreadDirent {
  OID      oid;
  ObCount  allocCount;
  uint16_t schedNdx;		/* index into CPU reserve table */
};

enum {
  tdp_maxDirEnt = (EROS_PAGE_SIZE - sizeof(DirPageHdr)) / sizeof(ThreadDirent),
};

typedef struct ThreadDirPage ThreadDirPage;
struct ThreadDirPage {
  DirPageHdr   hdr;
  ThreadDirent entry[tdp_maxDirEnt];
};

/* The checkpoint header page.  Note that if we implement journaling
 * by revising the checkpoint header, the sequence number may someday
 * need to be more than 64 bits.
 */

typedef struct DiskCheckpointHdr DiskCheckpointHdr;
struct DiskCheckpointHdr {
  uint64_t   sequenceNumber;	/* monotonically increasing ckpt seq no. */
  bool       hasMigrated;	/* true iff migration has completed */
  lid_t      maxLogLid;		/* lets us verify that the entire ckpt
				 * log has been mounted.
				 */
  uint32_t   nDirPage;		/* number of object directory pages */
  uint32_t   nThreadPage;	/* number of thread directory pages */
  uint32_t   nRsrvPage;		/* number of reserve pages */
};

/* The rule is that the leading entries in the dirPage array are the
 * log locations of the object directory pages, and the subsequent
 * entries are the log locations of the thread directory pages.
 */

enum {
  dckpt_maxDirPage = (EROS_PAGE_SIZE - sizeof(DiskCheckpointHdr)) / sizeof(lid_t),
};

typedef struct DiskCheckpoint DiskCheckpoint;
struct DiskCheckpoint {
  DiskCheckpointHdr hdr;
  lid_t             dirPage[dckpt_maxDirPage];
};

#endif /* __DISKCKPT_H__ */
