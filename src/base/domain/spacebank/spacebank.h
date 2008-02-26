/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

#ifndef SPACEBANK_H
#define SPACEBANK_H

#include <idl/capros/SpaceBank.h>

// #define NEW_DESTROY_LOGIC

#ifndef NEW_DESTROY_LOGIC
#ifdef NDEBUG
#define TREE_NO_TYPES
#endif
#endif

/* Invalid type for internal use */
#define SBOT_INVALID 0xff

#define NUM_BASE_TYPES 2	// just nodes and pages

extern const char *type_name(int t);
extern bool valid_type(int t);
extern uint32_t objects_per_frame[capros_Range_otNUM_TYPES];
extern uint32_t objects_map_mask[capros_Range_otNUM_TYPES];
extern uint8_t typeToBaseType[capros_Range_otNUM_TYPES];
extern int ffs(int);

#define kpanic kdprintf

bool heap_insert_page(uint32_t addr, uint32_t pageKR);

#define MAX_RANGES 256

/* Layout of memory. */
/* Following MUST agree with values in primebank.map: */
#define STACK_TOP           0x100000
#define SRM_BASE            0x400000
#define SRM_TOP            0x1400000
#define HEAP_BASE          SRM_TOP
#if defined(EROS_TARGET_arm)
#define HEAP_TOP           0x2000000	// to fit in a small space
#define HEAP_TOP_LG	25
#else
#define HEAP_TOP           0xc000000
#define HEAP_TOP_LG	32
#endif

#define SB_BRAND_KEYDATA       65535

/* Key registers */
#define KR_WALK0      KR_APP(0)	/* used internally by malloc */
#define KR_WALK1      KR_APP(1)	/* used internally by malloc */
#define KR_SRANGE     KR_APP(2)   /* Super Range key -- what fun! */

/* The following definitions MUST match those in primebank.map */
#define KR_PRIMEBANK  KR_APP(3)
#define KR_VERIFIER   KR_APP(4)

#define KR_VOLSIZE    KR_APP(5)
#define KR_TMP        KR_APP(6)
#define KR_TMP2       KR_APP(7)
#define KR_TMP3       KR_APP(8)
#define KR_OSTREAM    KR_APP(9)	/* only used for debugging */

#define KR_ARG0       KR_ARG(0)
#define KR_ARG1       KR_ARG(1)
#define KR_ARG2       KR_ARG(2)


/* NOTE: On startup, KR_ARG0 holds a node key to the prime space bank
   key's node. */

/* kprintf for uint64_ts convienience */
#define DW_HEX "%08x%08x" /* hexadecimal format string -- 16 chars */
#define DW_HEX_ARG(x) (uint32_t)(x >> 32),(uint32_t)(x) /* hex. arg. string*/

#endif /* SPACEBANK_H */

