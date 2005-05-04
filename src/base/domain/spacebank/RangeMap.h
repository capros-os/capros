/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
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


#ifndef RM_H
#define RM_H

#define RM_WLEN (EROS_PAGE_SIZE / 4)
#define RM_SIZE (EROS_PAGE_SIZE * 8)

typedef struct {
  uint64_t startOID;
  uint64_t endOID;
  uint32_t *base;			/* pointer to the bits */
  uint32_t curPos;			/* current position in search */
  uint32_t nAvail;			/* number of available frames this map */
} RangeMap;

void rm_Reset(RangeMap *map, OID start, OID end, uint32_t * newMap);
/* rm_reset:
 *     Sets /base/ to value of newMap, startOID to /start/, endOID to
 *     /end/, resets curPos, and runs through the new map to recompute
 *     the number of subranges with available space.
 */

void rm_MarkAllocated(RangeMap *map, OID oid);
/* rm_MarkAllocated:
 *     Marks the subrange identified by /oid/ as fully allocated.
 *     Decrements nAvail.
 *
 *     It is a fatal error if OID is out of range, or if the subrange
 *     map is already fully allocated.
 */

void rm_MarkFree(RangeMap *map, OID oid);
/* rm_MarkFree:
 *     Marks the subrange identified by /oid/ as partially free.
 *     Increments nAvail.
 *
 *     It is a fatal error if OID is out of range, or if the subrange
 *     map is already partially free.
 */

bool
rm_HasFreeSpace(RangeMap *map);
/* rm_hasFreeSpace:
 *     Returns /true/ if there is any allocatable space in /map/,
 *   false otherwise.
 */
#define rm_HasFreeSpace(map) ((map)->nAvail) ? true : false)

OID rm_FindAvailableSubrange(RangeMap *map);
/* rm_Allocate:
 *     Finds available subrange and returns it.
 *
 *     It is a fatal error if the subrange has no available space.
 */

#endif /* RM_H */
