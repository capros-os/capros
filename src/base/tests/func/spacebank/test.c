/*
 * Copyright (C) 2009, Strawberry Development Group
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
#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/ProcCre.h>
#include <idl/capros/Process.h>

#define KR_OSTREAM KR_APP(0)
#define KR_SB      KR_APP(1)
#define KR_SBND    KR_APP(2)
#define KR_SBNL    KR_APP(3)

/* Bypass all the usual initialization. */
const uint32_t __rt_runtime_hook = 0;

#define checkEqual(a,b) \
  if ((a) != (b)) \
    kdprintf(KR_OSTREAM, "Line %d, expecting %s=%d(0x%x), got %d(0x%x)\n", \
             __LINE__, #a, (b), (b), (a), (a));

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is %#x!\n", __LINE__, result); \
  }

void
printLimits(void)
{
  result_t result;
  capros_SpaceBank_limits limits, *lim = &limits;

  result = capros_SpaceBank_getLimits(KR_SBNL, &limits);
  ckOK
  kprintf(KR_OSTREAM, "fl=%6llu\n"
          "ac=%6llu\n"
          "el=%6llu\n"
          "ea=%6llu\n"
          "a0=%6llu\n"
          "a1=%6llu\n"
          "a2=%6llu\n"
          "a3=%6llu\n"
          "r0=%6llu\n"
          "r1=%6llu\n"
          "r2=%6llu\n"
          "r3=%6llu\n",
          lim->frameLimit, lim->allocCount, lim->effFrameLimit,
          lim->effAllocLimit,
          lim->allocs[0], lim->allocs[1], lim->allocs[2], lim->allocs[3],
          lim->reclaims[0], lim->reclaims[1], lim->reclaims[2], lim->reclaims[3]);
}

int
main(void)
{
  result_t result;
  capros_key_type type;
  uint32_t verif;
  /* LIMIT of 4 gives 1 for the bank's own forwarders and 3 for us. */
#define LIMIT 4

  kprintf(KR_OSTREAM, "Starting test.\n");

  result = capros_key_getType(KR_BANK, &type);
  ckOK
  checkEqual(type, IKT_capros_SpaceBank);

  result = capros_SpaceBank_verify(KR_BANK, KR_OSTREAM, &verif);
  ckOK
  checkEqual(verif, 0);	// not a bank

  // Create sub bank.
  kprintf(KR_OSTREAM, "Creating sub bank.\n");
  result = capros_SpaceBank_createSubBank(KR_BANK, KR_SB);
  ckOK

  result = capros_SpaceBank_verify(KR_BANK, KR_SB, &verif);
  ckOK
  checkEqual(verif, 1); // is a bank

  // Test that precludes work.
  result = capros_SpaceBank_reduce(KR_SB, capros_SpaceBank_precludeDestroy,
             KR_SBND);
  ckOK
  result = capros_key_destroy(KR_SBND);
  checkEqual(result, RC_capros_key_UnknownRequest);

  result = capros_SpaceBank_reduce(KR_SB, capros_SpaceBank_precludeSetLimit,
             KR_SBNL);
  ckOK
  result = capros_SpaceBank_setLimit(KR_SBNL, LIMIT);
  checkEqual(result, RC_capros_key_UnknownRequest);

  result = capros_SpaceBank_setLimit(KR_SBND, LIMIT);
  ckOK

  printLimits();

  result = capros_SpaceBank_alloc3(KR_SBNL,
             capros_Range_otPage
             + (capros_Range_otPage << 8)
             + (capros_Range_otPage << 16),
             KR_TEMP0, KR_TEMP1, KR_TEMP2);
  ckOK
  printLimits();

  result = capros_SpaceBank_alloc1(KR_SBNL, capros_Range_otPage, KR_TEMP3);
  checkEqual(result, RC_capros_SpaceBank_LimitReached);

  result = capros_SpaceBank_free3(KR_SBNL,
             KR_TEMP0, KR_TEMP1, KR_TEMP2);
  ckOK
  printLimits();

  result = capros_key_destroy(KR_SBNL);
  ckOK

  // Make a new sub-bank and test destroyBankAndSpace.
  kprintf(KR_OSTREAM, "Creating sub bank 2.\n");
  result = capros_SpaceBank_createSubBank(KR_BANK, KR_SB);
  ckOK

  result = capros_ProcCre_createProcess(KR_CREATOR, KR_SB, KR_TEMP0);
  ckOK
  // Ensure it's prepared.
  result = capros_Process_swapKeyReg(KR_TEMP0, KR_SELF, KR_TEMP0, KR_VOID);
  ckOK

  kprintf(KR_OSTREAM, "Destroying sub bank 2 and space.\n");
  result = capros_SpaceBank_destroyBankAndSpace(KR_SB);
  ckOK

  kprintf(KR_OSTREAM, "Done\n");

  return 0;
}
