#ifndef __KERNTUNE_H__
#define __KERNTUNE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

/* Kernel parameters. These are used to tune the sizes of
 * various internal tables according to the machine.
 */

/* THE FOLLOWING ARE USEFULLY TUNABLE */

/* Number of activities.  EROS is extremely activity-intensive, and
   activities don't take up any space worth measuring (like 32 bytes
   each).  Most activities are asleep at any given moment.

   At present, if activities get exhausted, the SEND operation will
   block.  This is easy to fix; I just haven't had time to deal with
   it.

   CAUTION: If you reduce this, save a new kernel, and try to restart
   from a checkpoint area that has more than this number of activities,
   DO NOT expect things to work! */
#define KTUNE_NACTIVITY      2048

/* Number of cached contexts.  This is the number of simultaneously
   active process contexts EROS can support.  When a context cache
   entry is reclaimed, its state is written back to its nodes. The
   number of actual processes is only limited by disk space.

   Making this bigger may alter performance in unusual circumstances;
   it will not alter correctness unless it gets below 4. */
#define KTUNE_NCONTEXT     512

/* Number of entries in the object range table.
 * This is the maximum number of object ranges CapROS can support.
 * Each disk object range takes one entry,
 * and each range of RAM takes one entry.
 * An object range table entry takes about 36 bytes. */
#define KTUNE_NRNGTBLENTS 14

/* Number of entries in the log range table.
 * This is the maximum number of log ranges CapROS can support.
 * A log range table entry takes about 36 bytes. */
#define KTUNE_NLOGTBLENTS 4

/* Amount of mappable physical card memory (i.e. sum over ALL cards)
 * IN MEGABYTES that the kernel should be prepared to map. This means
 * things like video memory, shared buffers on network cards, and the
 * like. For every 316 or so megabytes of mappable board memory we
 * need to add an additional 4 Mbytes to the heap limit so we can
 * allocate the required object headers. Note that this doesn't cause
 * the heap space itself to be claimed, only the mapping tables for
 * that heap space. This means that if you allow (e.g.) 630 Mbytes of
 * card memory and you only use 16 mbytes, you're wasting a total of
 * one page. Be generous here -- recompiles are a pain in the neck.
 */

#define KTUNE_MAX_CARDMEM 512

/* Maximum size IN MEGABYTES of the checkpoint log area that can be
   supported by this kernel.  This determines the size of the submap
   array.  This number is probably as large as you want to go, since
   above this point you are starting to lose locality on current
   generation drives.

   CAUTION: If you reduce this, and your actual checkpoint area holds
   more than this amount of stuff, YOUR SYSTEM WILL NOT RECOVER.

   FIX: At present this is not actually checked by the restart logic.
   It definitely should be. */ 
#define KTUNE_MAXCKLOGSIZE  256	 /* In MEGABYTES. */

/* Max number of block disk controllers on the system.  This is a
   plausible number for a PC, but probably not for, say, a large
   database machine.  Can be changed if you like; the disk structures
   do not take up much space. */
#define KTUNE_NBLKDEV       4

/* Max number of network interfaces.  This is actually a bit high, but
   hey, we're in a networking lab and I actually had a PC configured
   this high for a while */
#define KTUNE_NNETDEV       4

/* Number of simultaneous outstanding object I/O requests.  You want
   this to be high, as higher numbers facilitate checkpoint batching.
   A good choice is to pick a number large enough to saturate several
   modern disk tracks and then a few more.  EROS write traffic is
   bursty and sorted, so I/O's don't have the same impact that they
   might have on, say, UNIX (pbbbbt). */
#define KTUNE_NBLKIO        128

/* Maximum number of duplexes the kernel is expected to support for
   any given range.  2 is the largest pragmatically useful number.  3
   is used here because it's useful to be able to mount a third when
   you are rearranging your disks.

   This is the number of per-disk request structures that will be
   reserved before getting a duplexed I/O structure.  If you err, err
   on the high side.

   In short order I will re-implement the disk logic to be more
   similar to the BSD style so that I can steal drivers, at which
   point this tunable will go away. */
#define KTUNE_MAXDUPLEX     3

/* Maximum number of I/O requests that the checkpoint logic will
   attempt to batch together.  You may want it lower, but it should
   not be more than 1/4 of KTUNE_NBLKIO or checkpoint activity will
   dominate the I/O pattern.
 */
#define KTUNE_CKBATCH      (KTUNE_NBLKIO/4)

/* Controls for various internally hashed tables: */
#define KTUNE_NOBBUCKETS   1024 /* number of OID hash buckets */
#define KTUNE_NOBSLEEPQ    64	 /* number of object sleep queues */

/* Number of I/O address space regions you can allocate.  This is
   probably high enough. */
#define KTUNE_NIOREGION     32	 /* Number of reservable I/O address
				    space regions */

/* Max number of partitions we need to remember across all disks.
   Might need to go up if you have a farm.

   Tune with relative impunity. */
#define KTUNE_NPARTITION    64	 /* Number of partitions across all
				    disks */

/* Max number of EROS divisions within those partitions that we might
   find out there.  This is actually a bit tight, but it's good enough
   for 16 divisions per disk on a 4 drive system.  Now that node and
   page divisions are the same maybe it's enough.

   Tune with relative impunity. */
#define KTUNE_NCOREDIV      64	 /* Number of core division table entries */


#if 0
#define KTUNE_NDISKUNITS    8	 /* number of drives attached */
#define KTUNE_NEROSVOLS     64	 /* max EROS partitions across all disks */
#define KTUNE_BOUNCEBUFS    4	 /* bounce buffer pages */


#define KTUNE_NDEVINFO      32	 /* number of device information
				    structures (max number of attached
				    devices) */
#endif

/* Following are computed or recomputed on the basis of the values
   above.  Do not mess with these unless you know what you are about;
   they check vital kernel sanity conditions. */


#define KTUNE_MAXCKLOGSIZE  256	 /* In MEGABYTES. */


#define CKPT_MAP_BYTES_PER_MBYTE ((1024*1024)/EROS_PAGE_SIZE)
#define CKPT_MAP_BYTES  (KTUNE_MAXCKLOGSIZE * CKPT_MAP_BYTES_PER_MBYTE)

/* CKPT_MAXFREESUBMAP is the number of submap structures that should
   be allocated for use as checkpoint frame allocation counrs.  One
   byte is needed for each page in the checkpoint log area. */
#define CKPT_MAXSUBMAP \
     ((CKPT_MAP_BYTES + EROS_PAGE_SIZE - 1) / EROS_PAGE_SIZE)

#endif /* __KERNTUNE_H__ */
