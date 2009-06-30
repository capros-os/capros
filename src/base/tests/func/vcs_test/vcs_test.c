/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2009, Strawberry Development Group.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Process.h>
#include <idl/capros/GPT.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/VCS.h>
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM KR_APP(0)
#define KR_ZSF     KR_APP(1)
#define KR_SEG     KR_APP(2)
#define KR_AddrSpace KR_APP(3)

#define dbg_init    0x1
#define dbg_test    0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

#define TEST_ADDR  0x400000

/* This is truly sleazy -- it turns into one of:

   (kprintf) (args... )
   (kdprintf) (args... )

   according to the test result */

#define KPRINTF(x) ( (dbg_##x & dbg_flags) ? kdprintf : kprintf )

const uint32_t __rt_stack_pointer = 0x10000;
/* Flag as unkept so bootstrap code doesn't clobber KR_APP(0). */
const uint32_t __rt_unkept = 1;

void
PrintBankSpace(void)
{
  result_t result;
  int i;
  capros_SpaceBank_limits limits;

  result = capros_SpaceBank_getLimits(KR_BANK, &limits);
  assert(result == RC_OK);
  kprintf(KR_OSTREAM,
          "SpaceBank: %llu frames available\n",
          limits.effAllocLimit );
  static char * typeName[capros_Range_otNUM_TYPES] = {
    "page", "node", "forwarder", "GPT"
  };
  for (i = 0; i < capros_Range_otNUM_TYPES; i++) {
    kprintf(KR_OSTREAM,
            "  %9s: %6llu allocs, %6llu reclaims\n",
            typeName[i], limits.allocs[i], limits.reclaims[i]);
  }
}

int
main()
{
  uint32_t value;
  uint32_t addr = TEST_ADDR;
  result_t result;
  capros_key_type keyType;
  
  KPRINTF(init)(KR_OSTREAM, "About to buy new root seg:\n");

  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, KR_AddrSpace);
  ckOK

  KPRINTF(init)(KR_OSTREAM, "Set l2v:\n");

  capros_GPT_setL2v(KR_AddrSpace, 22);
  
  KPRINTF(init)(KR_OSTREAM, "Fetch current space:\n");

  capros_Process_getAddrSpace(KR_SELF, KR_TEMP0);

  KPRINTF(init)(KR_OSTREAM, "Insert it in new node:\n");

  capros_GPT_setSlot(KR_AddrSpace, 0, KR_TEMP0);

  KPRINTF(init)(KR_OSTREAM, "Make new GPT be my address space:\n");

  capros_Process_swapAddrSpace(KR_SELF, KR_AddrSpace, KR_VOID);
  PrintBankSpace();


  KPRINTF(test)(KR_OSTREAM, "Build new zero segment:\n");

  result = capros_Constructor_request(KR_ZSF, KR_BANK, KR_SCHED, KR_VOID,
			 KR_SEG);
  ckOK

  KPRINTF(test)(KR_OSTREAM, "Destroy it:\n");
  result = capros_key_destroy(KR_SEG);
  ckOK
  PrintBankSpace();

  KPRINTF(test)(KR_OSTREAM, "Build new zero segment:\n");

  result = capros_Constructor_request(KR_ZSF, KR_BANK, KR_SCHED, KR_VOID,
			 KR_SEG);
  ckOK

  result = capros_key_getType(KR_SEG, &keyType);
  ckOK
  assert(keyType == IKT_capros_VCS);

  result = capros_Memory_reduce(KR_SEG, capros_Memory_readOnly, KR_TEMP0);
  ckOK
  result = capros_key_getType(KR_TEMP0, &keyType);
  ckOK
  if (keyType != IKT_capros_GPT) {
    kdprintf(KR_OSTREAM, "%s:%d: keyType = %#x\n",
             __FILE__, __LINE__, keyType);
  }

  capros_GPT_setSlot(KR_AddrSpace, 1, KR_SEG);
  
  KPRINTF(test)(KR_OSTREAM, "About to read word from VCS:\n");

  value = *((uint32_t *) addr);
  
  if (value != 0)
    KPRINTF(test)(KR_OSTREAM, "Result was 0x%x\n", value);

  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr); 

  *((uint32_t *) addr) = 1;
  
  value = *((uint32_t *) addr);
  
  if (value != 1)
    kdprintf(KR_OSTREAM, "Reread value is: 0x%08x\n", value);


  addr = TEST_ADDR + EROS_PAGE_SIZE;

  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr); 

  *((uint32_t *) addr) = 2;
  
  value = *((uint32_t *) addr);
  
  if (value != 2)
    kdprintf(KR_OSTREAM, "Reread value is: 0x%08x\n", value);


  /* following tests traversal suppression */
  addr = TEST_ADDR + 2 * EROS_PAGE_SIZE;

  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr); 

  *((uint32_t *) addr) = 3;
  
  KPRINTF(test)(KR_OSTREAM, "Reread word from VCS:\n");

  value = *((uint32_t *) addr);

  if (value != 3)
    kdprintf(KR_OSTREAM, "Reread value is: 0x%08x\n", value);


  addr = TEST_ADDR + (EROS_PAGE_SIZE * EROS_NODE_SIZE);


#if 1 
  KPRINTF(test)(KR_OSTREAM, "About to write word to VCS at 0x%08x:\n",
		addr);

  *((uint32_t *) addr) = 4;
  
  value = *((uint32_t *) addr);
  
  if (value != 4)
    KPRINTF(test)(KR_OSTREAM, "Reread value is: 0x%08x\n", value);
#endif
  

  KPRINTF(test)(KR_OSTREAM, "About to destroy VCS:\n");
  result = capros_key_destroy(KR_SEG);
  ckOK
  PrintBankSpace();

  KPRINTF(test)(KR_OSTREAM, "Test PASSED\n");
  return 0;
}
