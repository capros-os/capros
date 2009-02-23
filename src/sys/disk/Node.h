#ifndef __DISK_NODE_H__
#define __DISK_NODE_H__
/*
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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
// NOTE: proc_IsWaiting() and proc_StateHasActivity() depend
// on these values!
#define RS_Available 0	/* The Process is Available, and there is no
			Resume key to it. There is no Activity. */
#define RS_Running   1	/* The Process is Running, and there is no
			Resume key to it. There is an Activity
			in state act_Ready, act_Running, or act_Stall. */
#define RS_WaitingU  2	/* The Process is Waiting, and there may be a
			Resume key to it. There is no Activity. */
#define RS_WaitingK  3	/* The Process is Waiting, and there may be a
			Resume key to it. There is an Activity
			in state act_Ready or act_Sleeping. */

/* For a node, the first byte of nodeData contains: */
#define NODE_L2V_MASK 0x3f	// same as GPT_L2V_MASK
#define NODE_BLOCKED 0x40
#define NODE_KEEPER  0x80 

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

#ifndef __ASSEMBLER__

INLINE uint8_t * 
node_l2vField(uint16_t * nodeDatap)
{
  return (uint8_t *) nodeDatap;
}

#endif // __ASSEMBLER__

#endif /* __DISK_NODE_H__ */
