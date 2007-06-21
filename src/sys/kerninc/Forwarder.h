#ifndef __FORWARDER_H__
#define __FORWARDER_H__
/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

// Slots of a Forwarder node:

/* Slots 0 through eros_Forwarder_maxSlot can be used by the user.
eros_Forwarder_maxSlot is 14 not 29, for compatibility with 16-slot nodes */

/* ForwarderDataSlot always has a number key. Its value[0] is the word
that is optionally transmitted. */
#define ForwarderDataSlot    30

/* ForwarderTargetSlot is the key to which invocations are forwarded.
It must be a gate key. */
#define ForwarderTargetSlot  31

// Bits in a Forwarder's nodeData:
#define ForwarderBlocked 0x1

#endif /* __FORWARDER_H__ */
