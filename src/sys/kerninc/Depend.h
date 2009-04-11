#ifndef __DEPEND_H__
#define __DEPEND_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2009, Strawberry Development Group.
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


/* A depend table entry keeps track of the relationship between a
 * (machine-dependent) mapping table entry and an item that generated
 * that mapping entry.  The purpose of the depend table
 * is to ensure that all of the mapping table entries associated with a
 * particular item can be found and invalidated if the 
 * associated item changes.

   An "item" is a slot of a node that was consulted in building
   the mapping table entry.
   At that time, the key in the slot was write-hazarded.
   Such slots are either in nodes prepared as a segment,
   or the slot ProcAddrSpace of a node prepared as a process root.

   The definition of "mapping table entry" is architecture-dependent. 

   In an architecture with a tree-structured map,
   a mapping table entry is one of the following:
   - The field in the Process structure that points to the top-level
     mapping table. 
   - An entry in a mapping table of any level. 

 * Whenever a page address translation fault occurs, the kernel walks
 * the segment tree, adding depend entries as it proceeds down the
 * tree.  In many (indeed most) cases, the necessary entry will
 * already be present in the depend structure. The
 * depend logic suppresses redundant entries.

   If mapping tables are shared, the corresponding depend entries
   will also be shared. (Actually, the dependency entries take up
 * rather more space than the corresponding mapping entries, so it's
 * more important to share them than the mapping entries.)
 *
 * Suppose you have a 32 bit machine with a two-level tree structured
 * mapping table mechanism with 4K pages.  Each layer of mapping table
 * translates 10 bits of address, and therefore contains 1024 entries.
 * (The example described is the x86 family, but the description
 * generalizes).
 *
 * The interesting case to note is what happens with a slot in an L4
 * segment node.  Such a slot spans 4096 addresses.  In a tree
 * structured system, however, it is sufficient to zap only the
 * corresponding L2 mapping table slots when the node slot in question
 * is zapped.  The slots in the L1 mapping tables are no longer
 * reachable, and therefore do not need to be zapped.  Therefore, ONLY
 * the L2 mapping slots are entered in the depend table.

   HASHING

   Dependency entries are stored with a hash of the key slot address.
   This means that when invalidating the mapping table entries associated
   with a key slot, other entries might also be invalidated.
   This is annoying but harmless.
   The probability of this should be low enough so as to not significantly
   impact performance.

   MERGING

   One key slot can influence several adjacent entries in a mapping
   table. As an optimization, a depend entry has a count of the number
   of adjacent entries associated with that slot. 

   Adjacent entries are recognized when the same slot generates a new
   entry in the same mapping table. More precisely, entries are merged
   iff slots with the same *hash* generate entries in the same table. 
   If two different slots have a hash collision and their entries
   get merged, that is unfortunate, but harmless. 

   INVALIDATING

   Mapping tables are never moved. (This includes the one-entry 
   "mapping table" that is the field in the Process structure
   that points to its top-level mapping table.)
   Consequently, either the memory that a dependency entry points to
   is still the entry that needs to be invalidated, or the entry that
   needed to be invalidated no longer exists. 

   A page containing a mapping table can be stolen and put to other use.
   There may remain dependency entries referring to that memory.
   Consequently, KeyDependEntry_Invalidate must take care not to clobber
   anything that is not in a mapping table. 
   It does this by checking the PageHeader of the page containing the
   entry to see if the memory is currently a mapping table. 
   If a page is stolen and reused for a different mapping table,
   KeyDependEntry_Invalidate will invalidate the entry in the new table,
   which is unfortunate but harmless. 
   If the table entry is in an area of memory that has no PageHeader,
   it is either in a Process structure or in a mapping table that
   was allocated early in system initialization; in neither case is the
   memory ever repurposed, so it is safe to invalidate the entry. 
 *
 * HASH STRUCTURED MAPPING
 *
 * My working theory is that the desired number of active mapping
 * table entries is some function of memory size, memory bus speed
 * (which bounds miss rate) and number of processors.  Whatever the
 * relationship is, there is probably a formula that will allow us to
 * calculate this number at machine startup.
 *
 * On a hash-structured mapping architecture, the machine-dependent
 * initialization simply allocates that many mapping entries at system
 * startup and uses the segment tree as a fallback, in effect viewing
 * the hash entries as a software-managed L2 TLB cache.
 *
 * Depend entries are interpreted a bit differently on a hash
 * architecture, because the mapping tables lacks the density to take
 * advantage of entry adjacency.  Instead, the fields are interpreted
 * as follows:
 *
 *  (mapping table, starting entry, number of entries, slot address)
 *
 *    mapping table     -- pointer to per-process table root, or
 *                         holder for ASID if all processes are
 *                         intermingled. 
 *    starting entry    -- starting kernel page va
 *    number of entries -- number of pages to invalidate
 *    slot address      -- address of segment node slot
 *
 * In order to make efficient use of this information the invalidation
 * algorithm will need to know something about the processor's hashing
 * strategies.  In particular, knowledge of the hashing algorithm
 * details can eliminate redundant walks of the hash buckets.
 *
 * SOFTWARE MANAGED MAPPING
 *
 * While we haven't thought much about the mapping tables we would
 * want for software-managed mapping (a la R3000), or for 64 bit
 * addressing, it seems very likely that our approach would be to
 * build a software-managed N-set L2 TLB cache, with the ability to
 * fall back to a segment tree walk when needed.  For such an
 * approach, the hash table mapping structures would be the ones to
 * use.
 *
 * STEALING
 *
 * On tree-structured architectures, mapping pages are allocated out of the
 * pool controlled by the core table, and are subject to ageing.  This
 * means that a mapping table can be yanked out from under us by
 * the ager. We need a way to find and invalidate all the mapping table
   entries that point to the table. 

   We do this the same way that we invalidate page table entries to a page
   that is stolen or evicted. 
   All such entries resulted from consulting a set of key slots.
   Either that set contains a key to the producer of the stolen object,
   in which case un-write-hazarding all keys to the producer will invalidate
   the entry,
   or the set is empty and the producer also produces a (degenerate) 
   higher-level mapping table, in which case zapping the higher-level
   tables will invalidate the entry.
   Here we consider a page's producer to be itself. 

   In an alternative design, there would be dependency entries
   for pages and mapping tables that
 * allow us to invalidate the mapping table entries that point to
 * them.  Therefore, L(n) mapping table entry that points to an L(n-1)
 * mapping page has an associated depend table entry of the form:
 *
 *   (L(n) tbl*, entry #, 1, L(n-1) tbl*)
 *
 */

struct PTE;	/*  PTE is an OPAQUE type! */

typedef struct KeyDependEntry {
  void * start;		/* First mapping table entry to zap */
			/* zero if this KeyDependEntry is not in use */

  uint32_t pteCount : 12;	/* Number of mapping table entries */
	/* pteCount is zero iff the "start" field really points to
	a Process structure and this entry pertains to the field
	that points to the top-level mapping table. */

  uint32_t slotTag : 20;	/* hash of key address */
} KeyDependEntry;

#define SLOT_TAG(pKey) ( (((unsigned) pKey) >> 4) & 0xfffffu )

void KeyDependEntry_Invalidate(KeyDependEntry * kde);
void KeyDependEntry_MakeRO(KeyDependEntry * kde);
void KeyDependEntry_TrackReferenced(KeyDependEntry * kde);
void KeyDependEntry_TrackDirty(KeyDependEntry * kde);

INLINE bool 
KeyDependEntry_InUse(KeyDependEntry const * kde)
{
  return kde->start != 0;
}

void Depend_AddKey(Key *, void *, int mapLevel);
void Depend_VisitEntries(Key * pKey, void (*func)(KeyDependEntry *));
void Depend_InvalidateKey(Key * key);

void Depend_InitKeyDependTable(uint32_t nNodes);
unsigned long Depend_getSize(void);
unsigned long Depend_getNumBuckets(void);

#if 0
void Depend_MarkAllForCOW();
#endif
#ifdef OPTION_DDB
void Depend_ddb_dump_hist();
void Depend_ddb_dump_bucket(uint32_t bucket);
#endif

#endif /* __DEPEND_H__ */
