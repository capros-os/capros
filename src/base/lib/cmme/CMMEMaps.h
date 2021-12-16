#ifndef __CMMEMAPS_H
#define __CMMEMAPS_H
/*
 * Copyright (C) 2007-2010, Strawberry Development Group.
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
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <eros/target.h>	// get result_t
#include <idl/capros/Range.h>

result_t maps_init(void);
void maps_fini(void);
/* *_locked procedures are for single-threaded programs
 * or while holding a lock that assures mutual exclusion. */
long maps_reserve_locked(unsigned long pageSize /* size in pages */ );
void maps_liberate_locked(unsigned long pgOffset,
                          unsigned long pageSize /* size in pages */ );
void * maps_pgOffsetToAddr(unsigned long pgOffset);
unsigned long maps_addrToPgOffset(unsigned long addr);
result_t maps_mapPage_locked(unsigned long pgOffset, cap_t pageCap);
long maps_reserveAndMapRange_locked(cap_t rangeCap,
  capros_Range_off_t firstPageOfs, unsigned int nPages, bool readOnly);
long maps_reserveAndMapBlock_locked(cap_t blockPageCap, unsigned int nPages);
void maps_getCap(unsigned long pgOffset, cap_t pageCap);

#endif // __CMMEMAPS_H
