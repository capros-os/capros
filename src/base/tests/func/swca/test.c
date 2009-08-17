/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

/* SWCA test.
*/

#include <stdint.h>
#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/fls.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/SWCA.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM  KR_APP(1)
#define KR_SLEEP    KR_APP(2)
#define KR_SWCA     KR_APP(3)
#define KR_LALOG    KR_APP(4)


const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

int
main(void)
{
  result_t result;

  kprintf(KR_OSTREAM, "Starting.\n");

  capros_key_type typ;
  result = capros_key_getType(KR_SWCA, &typ);
  ckOK
  if (typ != IKT_capros_SWCA)
    kdprintf(KR_OSTREAM, "Line %d type is 0x%08x!\n", __LINE__, typ);

  short value;

  // Get generator mode.
  // capros_SWCA_GenMode genModeValue;
  int16_t genModeValue;	//// workaround
  result = capros_SWCA_getGeneratorMode(KR_SWCA, 1, &genModeValue);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d gen mode = %d\n",
          0+1, genModeValue);

#if 1////
  // Set generator mode Off.
  result = capros_SWCA_setGeneratorMode(KR_SWCA, 1, capros_SWCA_GenMode_Off);
  ckOK

  // Get generator mode.
  result = capros_SWCA_getGeneratorMode(KR_SWCA, 1, &genModeValue);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d gen mode = %d\n",
          0+1, genModeValue);

#if 1////
  // Set generator mode to Auto.
  result = capros_SWCA_setGeneratorMode(KR_SWCA, 1, capros_SWCA_GenMode_Auto);
  ckOK

  // Get generator mode.
  result = capros_SWCA_getGeneratorMode(KR_SWCA, 1, &genModeValue);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d gen mode = %d\n",
          0+1, genModeValue);
#endif
#endif


  // Get bulk volts.
  result = capros_SWCA_getBulkVolts(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d bulk volts*10=%d\n",
          0+1, value);
  short originalBV = value;

  // Set bulk volts.
  result = capros_SWCA_setBulkVolts(KR_SWCA, 1, originalBV-4);
  ckOK

  // Get bulk volts.
  result = capros_SWCA_getBulkVolts(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d bulk volts*10=%d\n",
          0+1, value);

  // Set bulk volts back.
  result = capros_SWCA_setBulkVolts(KR_SWCA, 1, originalBV);
  ckOK

  // Get bulk volts.
  result = capros_SWCA_getBulkVolts(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d bulk volts*10=%d\n",
          0+1, value);


  result = capros_SWCA_getEqualizeTime(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d equalize time =%d\n",
          0+1, value);

  result = capros_SWCA_getLBCOVolts(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d LBCO volts*10=%d\n",
          0+1, value);

  result = capros_SWCA_getMaxChargeAmps(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d max charge amps =%d\n",
          0+1, value);

  result = capros_SWCA_getGenAmps(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d gen amps =%d\n",
          0+1, value);

  result = capros_SWCA_get15MinStartVolts(KR_SWCA, 1, &value);
  ckOK
  kprintf(KR_OSTREAM, "Inverter %d 15 min start volts*10=%d\n",
          0+1, value);

  result = capros_SWCA_getLoadAmpsLogfile(KR_SWCA, 0, KR_LALOG);
  ckOK

  kprintf(KR_OSTREAM, "Done.\n");

  return 0;
}

