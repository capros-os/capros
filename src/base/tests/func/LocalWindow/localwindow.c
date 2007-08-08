/*
 * Copyright (C) 2007, Strawberry Development Group
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
#include <eros/ProcessKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/GPT.h>

#include "constituents.h"

#define KR_OSTREAM   KR_APP(0)
#define KR_NEWNODE   KR_APP(1)
#define KR_MYSPACE   KR_APP(2)
#define KR_ALTSPACE  KR_APP(3)

unsigned long value = 0;

/* Local window is a small program. We test it by injecting a node
 * above the initial address space node, inserting a local window key
 * into that new node, and referencing one of our variables via the
 * now-aliased address. 
 */

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1 - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
}

int
main(void)
{
  unsigned long *pValue = &value;
  unsigned long *npValue;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  capros_SpaceBank_alloc2(KR_BANK, capros_Range_otGPT + (capros_Range_otGPT<<8),
                          KR_NEWNODE, KR_ALTSPACE);

  capros_GPT_setL2v(KR_NEWNODE, BlssToL2v(EROS_PAGE_BLSS + 2));
  capros_GPT_setL2v(KR_ALTSPACE, BlssToL2v(EROS_PAGE_BLSS + 2));

  /* Fetch our address space and stick it in the node. */
  process_copy(KR_SELF, ProcAddrSpace, KR_MYSPACE);
  capros_GPT_setSlot(KR_NEWNODE, 0, KR_MYSPACE);
  capros_GPT_setSlot(KR_ALTSPACE, 2, KR_MYSPACE);
  capros_GPT_setSlot(KR_ALTSPACE, 0, KR_MYSPACE);
  capros_GPT_setSlot(KR_NEWNODE, 1, KR_ALTSPACE);
  process_swap(KR_SELF, ProcAddrSpace, KR_NEWNODE, KR_VOID);

  /* Insert the first window: */
  capros_GPT_setWindow(KR_NEWNODE, 2, 0, 0, 0ull);

  kprintf(KR_OSTREAM, "Value of ul through normal address 0x%08x: %d\n",
	  pValue, *pValue);

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (1 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through alt address 0x%08x: %d\n",
	  npValue, *npValue);

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (2 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through window address 0x%08x: %d\n",
	  npValue, *npValue);

  /* Insert the second window: */
  capros_GPT_setWindow(KR_NEWNODE, 3, 1, 0, 0ull);

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (3 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through 2nd window address 0x%08x: %d\n",
	  npValue, *npValue);

  /* Insert the third window: */
  capros_GPT_setWindow(KR_NEWNODE, 4, 1, 0,
                       2ull << (EROS_NODE_LGSIZE + EROS_PAGE_LGSIZE));

  {
    unsigned char *cpValue = (unsigned char *) pValue;
    cpValue += (4 * EROS_NODE_SIZE * EROS_PAGE_SIZE);
    npValue = (unsigned long *)cpValue;
  }

  kprintf(KR_OSTREAM, "Value of ul through 3rd window address 0x%08x: %d\n",
	  npValue, *npValue);

  kprintf(KR_OSTREAM, "localwindow completes\n");

  return 0;
}
