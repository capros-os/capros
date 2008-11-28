#ifndef __NPODESCR_H__
#define __NPODESCR_H__
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

#ifndef __ASSEMBLER__
#include <disk/ErosTypes.h>
#endif // __ASSEMBLER__

/* This structure conveys to the kernel information about the initial
preloaded objects. */

/* Preloaded objects occupy a range of OIDs beginning with OIDBase.
That range contains the following frames in order:
  numNodeFrames of nodes allocated in the image file
    (we do not distinguish null nodes).
  numNonzeroPageFrames of nonzero pages allocated in the image file.
  some number of zero pages allocated in the image file.
  frames reserved for submap pages for the space bank.
  unallocated frames.

The first node (the one at OIDBase) is the "volsize" node. It contains:
  Slots 0 and 1: number caps containing the number of pages and nodes
    respectively that were allocated in the image file
    (including zero and nonzero objects).
    These are stored here by mkimage.
  If we define other frame types, their information will follow slot 1.
 */

/* Slot volsize_range: a Range cap for the range of objects.
    This range begins at OIDBase and contains all the frames
    enumerated above. 
    This cap is stored here by npgen. */
/* Slots volsize_range+1 through volsize_range_last inclusive may contain
   Range capabilities for additional ranges for the space bank.
   For now there are no tools to put them there. */
#define volsize_range 6
#define volsize_range_last 12

/* Slot volsize_nplinkKey contains the start cap to the nplink process,
 * which receives non-persistent caps at boot time.
 * This cap is stored here by an assignment in the mkimage file
 * for the persistent objects. */
#define volsize_nplinkCap 14

/* Slot volsize_pvolsize contains a node cap to the persistent volsize node,
 * which always has OID PVOLSIZE_OID.
 */
#define volsize_pvolsize 15

#ifndef __ASSEMBLER__

struct NPObjectsDescriptor {
  uint32_t numFrames; // equals numNonzeroPages
		// + (numNodes + DISK_NODES_PER_PAGE - 1) / DISK_NODES_PER_PAGE
		// This saves a computation in the boot code.
  uint32_t numFramesInRange;	// includes unallocated frames
  OID OIDBase;		// The first OID of the non-persistent objects
  OID IPLOID;		// The OID of the IPL process
  uint32_t numNodes;
  uint32_t numNonzeroPages;
  uint32_t numZeroPages;	// zero pages allocated in mkimage
  uint32_t numSubmaps;		// pages reserved for space bank submaps
  uint8_t numPreloadImages;	// 1 (if NP only) or 2 (NP and P)
};

#ifdef __KERNEL__
extern struct NPObjectsDescriptor * NPObDescr;
#endif

#endif // __ASSEMBLER__

#endif /* __NPODESCR_H__ */
