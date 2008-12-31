#ifndef __LSYNC_H
#define __LSYNC_H
/*
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

/* lsync.h -- Synchronization process for Linux kernel emulation.
*/

#include <domain/cmte.h>

/* Key registers: */
#define KR_LINUX_EMUL KR_CMTE(0) // Node of keys for Linux driver environment
#define KR_DEVPRIVS   KR_CMTE(1)
#define KR_APP2(i)    KR_CMTE(2+(i)) // first available key reg for driver

/* Slots in the node in KR_LINUX_EMUL: */
#define LE_CLOCKS 0
#define LE_IOMEM 1
/* LE_IOMEM has a key to an extended node containing, beginning with slot 0:
- A number key containing the number of pages 
  and the starting phys addr (uint64_t)
- The keys to the physical pages
This pattern repeats for as many physical ranges are available to
the process (usually one). */
//// Combine clocks, iomem, etc. into one extended node to save space?
// (at the cost of time)

/* Slots in the supernode KR_KEYSTORE: */
#define LKSN_APP                 LKSN_CMTE // available for driver

/* Constituents of driver constructors: */
#define KC_LINUX_EMUL KC_CMTE(0)
#define KC_DEVPRIVS   KC_CMTE(1)
#define KC_APP2(n)    KC_CMTE(2+(n))

#ifndef __ASSEMBLER__
#include <stdint.h>
extern uint32_t delayCalibrationConstant;
#endif

#endif // __LSYNC_H
