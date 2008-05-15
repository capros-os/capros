/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
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

#define dbg_init	0x01u   /* debug initialization logic */
#define dbg_heap	0x02u   /* debug heap mgmt logic */
#define dbg_dealloc     0x04u   /* debug deallocation logic */
#define dbg_alloc       0x08u   /* debug allocation logic */
#define dbg_malloc      0x10u   /* low-level mem alloc */
#define dbg_tree        0x20u   /* tree code debugging */
#define dbg_children    0x40u   /* child create/destroy code debugging */
#define dbg_limit       0x80u   /* Bank limits debugging */
#define dbg_realloc    0x100u   /* reallocation rate */
#define dbg_nospace    0x200	/* out of space errors */
#define dbg_cache      0x400	/* caches */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_nospace )

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))
#define DEBUG2(x,y) if (((dbg_##x|dbg_##y) & dbg_flags) == (dbg_##x|dbg_##y))

#ifdef TIMESTAMPS
uint64_t rdtsc(void);
#endif
