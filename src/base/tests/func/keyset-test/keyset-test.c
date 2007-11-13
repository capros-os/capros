/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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
#include <idl/capros/Number.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Constructor.h>

#include <domain/KeySetKey.h>
#include <domain/Runtime.h>

#include <domain/domdbg.h>

#define KR_KEYSETC  KR_APP(0)
#define KR_OSTREAM  KR_APP(1)
#define KR_SLEEP    KR_APP(2)

#define KR_EVENSET  KR_APP(3)
#define KR_ODDSET   KR_APP(4)
#define KR_OTHERSET KR_APP(5)
#define KR_ALLSET   KR_APP(6)

#define KR_NODE     KR_APP(7)
#define KR_TMP      KR_TEMP0

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

void
createNumberKey(uint32_t number, uint32_t krOut)
{
  uint32_t result;
  const capros_Number_value val = {{0,0,number}};

  result = capros_Node_writeNumber(KR_NODE, 0, val);
  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "Keyset-test: Error writing number key to node! (0x%08x)\n",
	     result);
  }

  result = capros_Node_getSlot(KR_NODE, 0, krOut);
#if 0
  kdprintf(KR_OSTREAM,
	   "Copied number key from slot 0 in node in %d to key register %d.\n",
	   KR_NODE, krOut);
#endif
  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "Keyset-test: Error copying number key from node! (0x%08x)\n",
	     result);
  }
}

int
main(void)
{
  uint32_t idx;
  uint32_t result;
  uint32_t isDiscrete;

  result = capros_Constructor_isDiscreet(KR_KEYSETC, &isDiscrete);
  if (result == RC_OK && isDiscrete) {
    kprintf(KR_OSTREAM,
	    "KeySet Constructor alleges discretion.\n");
  } else {
    kdprintf(KR_OSTREAM,
	     "KeySet Constructor is not discreet. (%08x)\n",
	     result);
  }

  kprintf(KR_OSTREAM, "Creating All set\n");

  result = capros_Constructor_request(KR_KEYSETC,
			       KR_BANK,
			       KR_SCHED,
			       KR_VOID,
			       KR_ALLSET);

  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "Construction of All KeySet failed. (%08x)\n", result);
  }

  kprintf(KR_OSTREAM, "Creating Even set\n");

  result = capros_Constructor_request(KR_KEYSETC,
			       KR_BANK,
			       KR_SCHED,
			       KR_VOID,
			       KR_EVENSET);

  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "Construction of Even KeySet failed. (%08x)\n", result);
  }

  kprintf(KR_OSTREAM, "Creating odd set\n");

  result = capros_Constructor_request(KR_KEYSETC,
			       KR_BANK,
			       KR_SCHED,
			       KR_VOID,
			       KR_ODDSET);

  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "Construction of Odd KeySet failed. (%08x)\n", result);
  }

  kprintf(KR_OSTREAM, "Creating other set\n");
  
  result = capros_Constructor_request(KR_KEYSETC,
			       KR_BANK,
			       KR_SCHED,
			       KR_VOID,
			       KR_OTHERSET);

  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "Construction of Other KeySet failed. (%08x)\n", result);
  }

  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otNode, KR_NODE);

  if (result != RC_OK) {
    kdprintf(KR_OSTREAM,
	     "Failed to buy node from SpaceBank (%08x)\n", result);
  }

  /* we've now got all the pieces -- let the test begin! */

  /* first, initialize the sets.  EvenSet gets the even numbers,
     OddSet gets the odds, and OtherSet gets the multiples of 3. */
  
#define MAX_NUM 16
  for (idx = 0u; idx < MAX_NUM; idx++) {
    createNumberKey(idx, KR_TMP);

    kprintf(KR_OSTREAM,"Adding NK(0x%08x) to",idx);
    kprintf(KR_OSTREAM," all");
    result = keyset_add_key(KR_ALLSET, KR_TMP, idx, NULL);
    if (result != RC_OK) {
      kdprintf(KR_OSTREAM,
	       "\nKeySet: Error adding NumberKey(0x%08x) to "
	       "all set. (0x%08x)\n",
	       idx,result);
    }
    if ((idx & 0x1u) == 0) {
      /* even */
      kprintf(KR_OSTREAM," even");
      result = keyset_add_key(KR_EVENSET, KR_TMP, idx, NULL);
      if (result != RC_OK) {
	kdprintf(KR_OSTREAM,
		 "\nKeySet: Error adding NumberKey(0x%08x) to "
		 "even set. (0x%08x)\n",
		 idx,result);
      }
    } else {
      /* odd */
      kprintf(KR_OSTREAM," odd");
      result = keyset_add_key(KR_ODDSET, KR_TMP, idx, NULL);
      if (result != RC_OK) {
	kdprintf(KR_OSTREAM,
		 "\nKeySet: Error adding NumberKey(0x%08x) to "
		 "odd set. (0x%08x)\n",
		 idx,result);
      }
    }
    if ((idx % 2) || (idx % 3) == 0) {
      /* odd or evenly divisible by three */
      kprintf(KR_OSTREAM," other");
      result = keyset_add_key(KR_OTHERSET, KR_TMP, idx, NULL);
      if (result != RC_OK) {
	kdprintf(KR_OSTREAM,
		 "\nKeySet: Error adding NumberKey(0x%08x) to "
		 "other set. (0x%08x)\n",
		 idx,result);
      }
    }
    kprintf(KR_OSTREAM,"\n");
  }

  kprintf(KR_OSTREAM, "Done adding keys.  Now testing ContainsKey.\n");
  
  for (idx = 0; idx < MAX_NUM; idx++) {
    uint32_t result;
    createNumberKey(idx, KR_TMP);
    result = keyset_contains_key(KR_ALLSET,KR_TMP, NULL);
    if (result != RC_OK) {
      kdprintf(KR_OSTREAM,
	       "KeySetTest: AllSet does not contain 0x%08x (%08x)\n",
	       idx,
	       result);
    }

    result = keyset_contains_key(KR_EVENSET,KR_TMP, NULL);
    if (result != (idx % 2)?RC_KeySet_KeyNotInSet:RC_OK) {
      kdprintf(KR_OSTREAM,
	       "KeySetTest: EvenSet got wrong result for 0x%08x (%08x)\n",
	       idx,
	       result);
    }

    result = keyset_contains_key(KR_ODDSET,KR_TMP, NULL);
    if (result != (!(idx % 2))?RC_KeySet_KeyNotInSet:RC_OK) {
      kdprintf(KR_OSTREAM,
	       "KeySetTest: OddSet got wrong result for 0x%08x (%08x)\n",
	       idx,
	       result);
    }

    result = keyset_contains_key(KR_OTHERSET,KR_TMP, NULL);
    if (result != ((idx % 2) || (idx % 3) == 0)?RC_OK:RC_KeySet_KeyNotInSet) {
      kdprintf(KR_OSTREAM,
	       "KeySetTest: OtherSet got wrong result for 0x%08x (%08x)\n",
	       idx,
	       result);
    }
    
  }

#if 0 /* DISABLED until we go back to compare_sets */
  /* test that all of the CompareSets relations are woring. */

  kprintf(KR_OSTREAM, "Testing compare_sets\n");

  kprintf(KR_OSTREAM, "Checking:  AllSet contains EvenSet\n");
  result = keyset_compare_sets(KR_ALLSET, KR_EVENSET,0);

  if (result != RC_KeySet_SetContainsOtherSet) {
    kdprintf(KR_OSTREAM,"KeysetTest: AllSet does not contain EvenSet!\n");
  }
  
  kprintf(KR_OSTREAM, "Checking:  AllSet contains OddSet\n");
  result = keyset_compare_sets(KR_ALLSET, KR_ODDSET,0);

  if (result != RC_KeySet_SetContainsOtherSet) {
    kdprintf(KR_OSTREAM,"KeysetTest: AllSet does not contain OddSet!\n");
  }
  
  kprintf(KR_OSTREAM, "Checking:  AllSet contains OtherSet\n");
  result = keyset_compare_sets(KR_ALLSET, KR_OTHERSET,0);

  if (result != RC_KeySet_SetContainsOtherSet) {
    kdprintf(KR_OSTREAM,"KeysetTest: AllSet does not contain OtherSet!\n");
  }
  
  kprintf(KR_OSTREAM, "Checking:  AllSet contains itself\n");
  result = keyset_compare_sets(KR_ALLSET, KR_ALLSET,0);

  if (result != RC_KeySet_SetsEqual) {
    kdprintf(KR_OSTREAM,"KeysetTest: AllSet does not contain itself!\n");
  }

  kprintf(KR_OSTREAM, "Checking: EvenSet does not contain AllSet\n");
  result = keyset_compare_sets(KR_EVENSET, KR_ALLSET,0);

  if (result != RC_KeySet_OtherSetContainsSet) {
    kdprintf(KR_OSTREAM,"KeysetTest: EvenSet contains AllSet!\n");
  }
  
  kprintf(KR_OSTREAM, "Checking: EvenSet does not contian OddSet\n");
  result = keyset_compare_sets(KR_EVENSET, KR_ODDSET,0);

  if (result != RC_KeySet_SetsDisjoint) {
    kdprintf(KR_OSTREAM,"KeysetTest: EvenSet contains OddSet!\n");
  }
  
  kprintf(KR_OSTREAM, "Checking: EvenSet does not contian OtherSet\n");
  result = keyset_compare_sets(KR_EVENSET, KR_OTHERSET,0);

  if (result != RC_KeySet_SetsDifferent) {
    kdprintf(KR_OSTREAM,"KeysetTest: EvenSet contains OtherSet!\n");
  }
  
  kprintf(KR_OSTREAM, "Checking: EvenSet contains itself\n");
  result = keyset_compare_sets(KR_EVENSET, KR_EVENSET,0);

  if (result != RC_KeySet_SetsEqual) {
    kdprintf(KR_OSTREAM,"KeysetTest: EvenSet does not contain itself!\n");
  }
#endif

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

