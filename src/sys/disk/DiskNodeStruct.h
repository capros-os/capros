#ifndef __DISK_DISKNODESTRUCT_HXX__
#define __DISK_DISKNODESTRUCT_HXX__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

// Values for the Process runState:
#define RS_Available 0
#define RS_Waiting   1
#define RS_Running   2

#ifndef __ASSEMBLER__
/* This structure defines the *layout* of the disk node structure so
 * that various elements of the kernel can fetch things from ROM and
 * RAM nodes. 
 *
 * For the moment, the kernel still makes the assumption that nodes on
 * disk are gathered into node pots -- even in ROM and RAM
 * images. This should perhaps be reconsidered, so as to isolate the
 * kernel from unnecessary knowledge.
 */

#include "KeyStruct.h"

typedef struct DiskNodeStruct DiskNodeStruct;
struct DiskNodeStruct {
  ObCount allocCount;
  ObCount callCount;
  OID oid;

  uint16_t nodeData;
  KeyBits slot[EROS_NODE_SIZE];
} ;

/* For a GPT, the first byte of nodeData contains: */
#define GPT_L2V_MASK 0x3f
#define GPT_BACKGROUND 0x40
#define GPT_KEEPER 0x80
INLINE uint8_t * 
gpt_l2vField(uint16_t * nodeDatap)
{
  return (uint8_t *) nodeDatap;
}

/* For a node, the first byte of nodeData contains: */
#define NODE_L2V_MASK GPT_L2V_MASK
#define NODE_BLOCKED 0x40
#define NODE_KEEPER  0x80 
INLINE uint8_t * 
node_l2vField(uint16_t * nodeDatap)
{
  return gpt_l2vField(nodeDatap);
}

INLINE uint8_t *
proc_runStateField(DiskNodeStruct * dn)
{
  /* N.B. This must match the location in the LAYOUT file. */
  return ((uint8_t *) &dn->slot[8].u.nk.value) + 8;
}

#endif // __ASSEMBLER__

/* Slots of a process root. Changes here should be matched in the
 * architecture-dependent layout files and also in the mkimage grammar
 * restriction checking logic. */
#define ProcSched             0
#define ProcKeeper            1
#define ProcAddrSpace         2
#define ProcGenKeys           3
#define ProcIoSpace           4
#define ProcSymSpace          5
#define ProcBrand             6
/*			      7    unused */
// 8 has fault code, fault info, and runState.
#define ProcPCandSP           9
#define ProcFirstRootRegSlot  8
#define ProcLastRootRegSlot   31

#define DISK_NODES_PER_PAGE (EROS_PAGE_SIZE / sizeof(DiskNodeStruct))

#endif /* __DISK_DISKNODESTRUCT_HXX__ */
