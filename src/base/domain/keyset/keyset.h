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


#define KR_KEYBITS     KR_APP(0)
#define KR_OSTREAM     KR_APP(1)
#define KR_SCRATCH     KR_APP(2)
#define KR_ADDRNODE    KR_APP(3)
#define KR_DISCRIM     KR_APP(4)
#define KR_ARG0        KR_ARG(0)
#define KR_SCRATCH2    KR_ARG(1)
#define KR_SETRESUME   KR_APP(6)

struct table_entry {
  uint32_t w[5]; /* 4 words of keybits, 1 word of additional data */
};

int compare(uint32_t *k1, uint32_t *k2);

struct KeySetStat {
  uint32_t startKeyBitsVersion;

  struct table_entry *end_of_table;
  uint32_t numSorted;
  uint32_t numUnsorted;

  enum initState {
    START = 0,
    LOBOTOMIZED,
    ZSBOUGHT,
    RUNNING
  } initstate;
};

#define MAX_UNSORTED (16u) /* maximum number of unsorted before sorting */
			     
/* sort the table */
void
sortTable(struct table_entry *table, uint32_t length);

/* search /table/ -- assumes first /lengthSorted/ items are sorted,
                     next /lengthUnsorted/ items are not.  */

struct table_entry *
findEntry(struct table_entry *table,
	  uint32_t toFind[4],
	  uint32_t lengthSorted,
	  uint32_t lengthUnsorted);

#define FIND(table, toFind, STAT) \
    findEntry(table, toFind, (STAT).numSorted, (STAT).numUnsorted)

/* debugging stuff */
#define dbg_init     0x01
#define dbg_cmds     0x02
#define dbg_add      0x04
#define dbg_contains 0x08
#define dbg_find     0x10
#define dbg_protocol 0x20

#define dbg_all      0x3F

/* Following should be an OR of some of the above */
#define dbg_flags   ( dbg_contains | dbg_add | 0u )

#ifndef NDEBUG
#define DEBUG(x) if (dbg_##x & dbg_flags)
#else
#define DEBUG(x) if (0)
#endif

#ifdef NDEBUG
#define assert(ignore) ((void) 0)
#else

extern int __assert(const char *, const char *, int);

#define assert(expression)  \
  ((void) ((expression) ? 0 : __assert (#expression, __FILE__, __LINE__)))
#endif

