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


#ifndef OBJSPACE_H
#define OBJSPACE_H

struct Bank;

extern void ob_init(void);
/* ob_init():
 *     Initializes master frame space.  Called once on startup.  Calls
 *   Bank_init().
 */

#if 0
void ob_MarkFrameAllocated(OID oid);
/* ob_MarkAllocated:
 *     Marks the frame identified by /oid/ as allocated, updating
 *     subrange and range maps accordingly.
 */
#endif

/* JA NOTE: this replaces cache_GrabFrame().  The idea is that both
   the grab and the release should be done through the same interface,
   and the allocation cache is an internal design issue in the object
   manager. This is a change from the design as we laid it out on
   paper. */
#if 0
uint32_t ob_AllocPageFrame(struct Bank *, OID* oid);
uint32_t ob_AllocNodeFrame(struct Bank *, OID* oid);
#endif
uint32_t ob_AllocFrame(struct Bank *, OID* oid, bool wantNode);
#define ob_AllocPageFrame(bank, oid) ob_AllocFrame(bank, oid, false)
#define ob_AllocNodeFrame(bank, oid) ob_AllocFrame(bank, oid, true)
/* ob_AllocXxxFrame:
 *     Allocates a new frame, whose OID is returned in /oid/, updating
 *     all maps as necessary.
 *
 *     Use AllocNodeFrame when allocating for nodes, AllocPageFrame
 *     when allocating for page-sized objects.
 *
 *     Returns 1 on success, 0 on failure.
 */

void ob_ReleaseFrame(struct Bank*, OID oid, bool isNode);
#define ob_ReleasePageFrame(bank, oid) ob_ReleaseFrame(bank, oid, false)
#define ob_ReleaseNodeFrame(bank, oid) ob_ReleaseFrame(bank, oid, true)
/* ob_FreeFrame:
 *     Marks the frame identified by /oid/ as free, updating subrange
 *     and range maps accordingly.
 */

#if 0
/* This is internal only. */
bool ob_FindNonEmptySubrange(OID*);
/* ob_FindNonEmptySubrange:
 *     Finds a subrange with available space and returns its OID.
 */
#endif

#endif /* OBJSPACE_H */
