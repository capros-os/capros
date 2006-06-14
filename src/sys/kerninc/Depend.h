#ifndef __DEPEND_H__
#define __DEPEND_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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


#if 0
/* A depend table entry keeps track of the relationship between a
 * machine-dependent mapping table slot and the slot(s) that generated
 * that mapping entry.  Originally, the purpose of the depend table
 * was to ensure that all of the mapping entries associated with a
 * particular slot can be found and invalidated if the value in the
 * associated slot changes.
 *
 * Note that the current mechanism makes no provision for variable
 * page sizes such as the 4M pages of the Pentium family or the
 * R3000. Our basic view of these features is that until we understand
 * how to properly use them for user-level software it is best to
 * reserve them for OS use.
 *
 * The simplest way to think of all this is that every involved slot
 * controls an associated range of virtual addresses in each mapping
 * table that it impacts.  The best way to think about this is that we
 * need to remember a set of quadruples of the form:
 *
 *    (mapping table, starting entry, number of entries, slot address)
 *
 * The starting entry is relative to the start of the mapping table.
 *
 * Whenever a page address translation fault occurs, the kernel walks
 * the segment tree, adding depend entries as it proceeds down the
 * tree.  In many (indeed most) cases, the necessary entry will
 * already be present in the depend structure, but the kernel has no
 * way to know that any existing depend entries exist to cover the
 * current process.  Adding them redundantly does no harm, since the
 * depend logic suppresses redundant entries.
 *
 * TREE STRUCTURED OPTIMIZATIONS
 *
 * Usage Optimizations are possible for machines with tree structured
 * address mapping.  On such machines, object mappings are often
 * shared, and it is convenient if the dependency management entries
 * can be shared as well.  Actually, the dependency entries take up
 * rather more space than the corresponding mapping entries, so it's
 * more important to share them than the mapping entries.  To do this,
 * the mapping table field points to either an L1 or L2 mapping table
 * page, and the offsets and counts are relative to that mapping page.
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
 * NUMBER OF MAPPING ENTRIES
 *
 * While the number of active mapping entries needed should, in
 * principle, be relatively independent of the mapping mechanism's
 * implementation, there is an ugly multiplier effect introduced in
 * tree structured architectures.  While the NUMBER of active entries
 * doesn't vary, their distribution among mapping table pages WILL.
 * The need to allocate pages rather than entries therefore has fairly
 * bad implications for main memory allocation.
 *
 * On such architectures, mapping pages are allocated out of the pool
 * controlled by the core table, and are subject to ageing.  This
 * means that a mapping table page can be yanked out from under us by
 * the ager, so there must be dependency entries for such pages that
 * allow us to invalidate the mapping table entries that point to
 * them.  Therefore, L(n) mapping table entry that points to an L(n-1)
 * mapping page has an associated depend table entry of the form:
 *
 *   (L(n) tbl*, entry #, 1, L(n-1) tbl*)
 *
 * Similary, every root page table pointer in a context structure has
 * an associated depend entry of the form:
 *
 *   (root page table ptr*, 0, 0, L3 tbl*)
 *
 * Note the 0 in the "number of entries field", which tips off the
 * zapper logic that this entry needs to be specially handled.
 *
 * FUTURE OPTIMIZATION FOR TREE STRUCTURED MAPPPING ARCHITECTURES:
 *
 * On a machine with a tree-structured memory mapping architecture,
 * the starting entry field is unnecessary.  The maping table pointer
 * can simply point directly to the first entry impacted by this
 * depend table entry.  If this is done, the starting entry field is
 * always zero and can therefore be eliminated.  Since this
 * optimization can be introduced without changing the interface, I
 * plan to defer it for now.  At some point I'll add it as a #define
 * in target.h
 *
 * Given this optimization, the L(n)/L(n-1) entry becomes
 *
 *   (L(n) tbl entry*, 1, L(n-1) tbl*)
 *
 */
#endif

/*  PTE is an OPAQUE type! */

typedef struct KeyDependEntry {
  struct PTE  *start;		/* First entry to zap */
  uint32_t     pteCount : 12;	/* Number of entries */
  uint32_t     slotTag : 20;	/* hash of key address */
} KeyDependEntry;

#define SLOT_TAG(pKey) ( (((unsigned) pKey) >> 4) & 0xfffffu )

void KeyDependEntry_Invalidate(KeyDependEntry * );

INLINE bool 
KeyDependEntry_InUse(KeyDependEntry const * kde)
{
  return kde->start != 0;
}

#if 0
struct PageDependEntry {
  struct PTE  *entry;
  void Invalidate();
  void Invalidate(kva_t);
} ;
#endif

void Depend_AddKey(Key*, struct PTE*, bool allowMerge);
void Depend_InvalidateKey(Key *key);
  
#if 0
void Depend_AddPage(struct ObjectHeader*, struct PTE*);
void Depend_InvalidatePage(struct ObjectHeader *page);
#endif

  /* Machine dependent -- generally accompanies the page fault
   * handling code
   */
void Depend_InvalidateProduct(MapTabHeader * page);

void Depend_InitKeyDependTable(uint32_t nNodes);
#if 0
void Depend_InitPageDependTable(uint32_t nPages);
#endif

#if 0
void Depend_MarkAllForCOW();
#endif
#ifdef OPTION_DDB
void Depend_ddb_dump_hist();
void Depend_ddb_dump_bucket(uint32_t bucket);
#endif

#endif /* __DEPEND_H__ */
