/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
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

/* design assumptions at end */

#include <string.h>
#include <eros/target.h>
#include <eros/ProcessKey.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <domain/Runtime.h>
#include <eros/Invoke.h>
#include <domain/domdbg.h>

#include "debug.h"
#include "assert.h"
#include "malloc.h"
#include "misc.h"
#include "spacebank.h"
#include "Bank.h"

/* static uint32_t base = HEAP_BASE; */
static uint32_t top = HEAP_BASE;
static uint32_t bound = HEAP_BASE;	// heap is grown up to here

static bool grow_heap(void);

void *
malloc(size_t sz)
{
  uint32_t ptr;
  
  DEBUG(malloc)
    kdprintf(KR_OSTREAM, "Call to malloc for %d, top=0x%08x, bound=0x%08x\n",
	     sz, top, bound);
  
  while (bound - top < sz) {
    if (!grow_heap())
      return 0;
  }
  
  ptr = top;
  top += (  ( sz + sizeof(uint32_t) - 1 ) & ~((unsigned)sizeof(uint32_t) - 1u) );

  return (void*) ptr;
}

/* Note the use of a sleazy trick here -- rather than using a kernel
   call to swap key registers, we keep the active register numbers in
   variables and swap those around instead. */
static bool
grow_heap()
{
  assert( (bound & EROS_PAGE_MASK) == 0 );

  if (bound >= HEAP_TOP)
    return false;	// no more address space for the heap

  if (BankAllocObjects(&bank0, eros_Range_otPage, 1, KR_TMP) != RC_OK)
    return false;

  if (heap_insert_page((uint32_t) bound, KR_TMP) == false) {
    BankDeallocObjects(&bank0, eros_Range_otPage, 1, KR_TMP);
    return false;
  }

  bound = bound + EROS_PAGE_SIZE;

  return true;
}

/* spacebank malloc support
 *
 * The basic assumption of this code is that the malloc heap sits
 * after everything else, and can freely be permitted to grow upward
 * until it hits HEAP_TOP.  This is a VERY
 * SIMPLE implementation -- the working assumption is that STORAGE
 * WILL NEVER BE FREED.  This means that the allocator returns exactly
 * as much as you asked for, and no more.  It is the responsibility of
 * a higher-level allocation manager to deal with free lists.
 *
 * When a malloc occurs that cannot be satisfied from the available
 * free list, the heap is grown by calling grow_heap().  If that
 * fails, malloc() returns 0.
 *
 * grow_heap simply attempts to add a page to the end of the space
 * bank heap by obtaining nodes/pages via bank0 and doing the
 * necessary modifications to the address space tree of the space bank
 * itself.  grow_heap assumes that the address space of the space bank
 * is a node of suitable size, and that the address space key is a
 * node key.
 */

