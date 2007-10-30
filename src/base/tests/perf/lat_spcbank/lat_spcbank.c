/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2007, Strawberry Development Group
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Sleep.h>
#include <eros/KeyConst.h>
#include <domain/domdbg.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>

#define KR_VOID 0
#define KR_SELF 4

#define KR_BANK 7
#define KR_PGTREE 8
#define KR_SLEEP 9
#define KR_OSTREAM 10

#define KR_WALK 15
#define KR_PAGE 16
#define KR_SEG     17

const uint32_t __rt_stack_pages = 0;
const uint32_t __rt_stack_pointer = 0x20000;

#define NPASS  5
//#define NPAGES 2048		/* 8 Mbytes */
#define NPAGES 256

void
extendedSetSlot(unsigned int i, uint32_t kr)
{
  capros_GPT_getSlot(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
  capros_GPT_getSlot(KR_WALK,
                     (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
  capros_GPT_setSlot(KR_WALK, i & EROS_NODE_SLOT_MASK, kr);
}

void
extendedGetSlot(unsigned int i, uint32_t kr)
{
  capros_GPT_getSlot(KR_PGTREE, i >> (EROS_NODE_LGSIZE*2), KR_WALK);
  capros_GPT_getSlot(KR_WALK,
                     (i >> EROS_NODE_LGSIZE) & EROS_NODE_SLOT_MASK, KR_WALK);
  capros_GPT_getSlot(KR_WALK, i & EROS_NODE_SLOT_MASK, kr);
}

void
NullProc(void)
{
}

void
BuyPageProc(void)
{
  uint32_t result = capros_SpaceBank_alloc1(KR_BANK,
                          capros_Range_otPage, KR_PAGE);
  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Page purchase failed\n");
}

void
BuyNodeProc(void)
{
  uint32_t result = capros_SpaceBank_alloc1(KR_BANK,
                          capros_Range_otNode, KR_PAGE);
  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Node purchase failed\n");
}

void
SellPageProc(void)
{
  uint32_t result = capros_SpaceBank_free1(KR_BANK, KR_PAGE);
  if (result != RC_OK)
    kdprintf(KR_OSTREAM, "Return failed 0x%08x\n", result);
}

void
DoTest(void (*allocProc)(void), void (*freeProc)(void))
{
  unsigned int i, pass;
  uint64_t startTime, endTime;
  uint32_t result;

  /* Warm up the space bank for tests: */

  /* Buy needed pages: */
  for (i = 0; i < NPAGES; i++) {
    (*allocProc)();
    extendedSetSlot(i, KR_PAGE);
  }
  
  /* Now sell them back: */
  for (i = 0; i < NPAGES; i++) {
    extendedGetSlot(i, KR_PAGE);
    (*freeProc)();
  }
  
  kprintf(KR_OSTREAM, "Warmed up\n");

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &startTime);
  if (result != RC_OK)
    kprintf(KR_OSTREAM, "getTime returned %d(0x%x)\n", result);

  for (pass = 0; pass < NPASS; pass++) {
    /* Buy needed pages: */
    for (i = 0; i < NPAGES; i++) {
      (*allocProc)();
      extendedSetSlot(i, KR_PAGE);
    }
  
    /* Now sell them back: */
    for (i = 0; i < NPAGES; i++) {
      extendedGetSlot(i, KR_PAGE);
      (*freeProc)();
    }
  }

  result = capros_Sleep_getTimeMonotonic(KR_SLEEP, &endTime);

  kprintf(KR_OSTREAM, "%10u us per object\n",
	  (uint32_t) ((endTime - startTime)/(NPASS*NPAGES*1000)) );
}

int
main()
{
  capros_Sleep_sleep(KR_SLEEP, 1000);

  BuyPageProc();	// just to get a page key in KR_Page

  /* Run a calibration pass: */
  kprintf(KR_OSTREAM, "Following is cost of %d tree manipulations:\n",
	  NPAGES);

  DoTest(&NullProc, &NullProc);

  SellPageProc();
  
  kprintf(KR_OSTREAM, "Starting %d page measurements\n",
	  NPAGES);

  DoTest(&BuyPageProc, &SellPageProc);
  
  kprintf(KR_OSTREAM, "Starting node measurements\n");

  DoTest(&BuyNodeProc, &SellPageProc);

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}
