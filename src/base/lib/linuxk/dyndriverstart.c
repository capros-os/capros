/*
 * Copyright (C) 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <domain/CMTEMaps.h>
#include <domain/assert.h>

#define dbg_alloc 0x01

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* This is the first driver code to run in a usb driver process.
   It sets up the .data and .bss sections in a vcsk. */

extern result_t driver_main(void);

/* We run after dyndriverprotospace.
   which leaves KR_TEMP3 as the GPT to our address space,
   and KR_TEMP2 as the GPT to our stacks space.
   The caller of the constructor passed a USBInterface cap
   which is now in KR_ARG(0), and a resume key in KR_RETURN. */
int
cmte_main(void)
{
  result_t result;
  result_t finalResult;

  // Unpack some caps.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_LINUX_EMUL,
    KR_LINUX_EMUL);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_DEVPRIVS, KR_DEVPRIVS);
  assert(result == RC_OK);
  /* Set I/O privileges. */
  result = capros_Process_setIOSpace(KR_SELF, KR_DEVPRIVS);
  assert(result == RC_OK);

  assert(LKSN_APP == LKSN_CMTE);	// otherwise need to allocate
		// range in KR_KEYSTORE

  maps_init();

  // Initialize delayCalibrationConstant.
  result = capros_Sleep_getDelayCalibration(KR_SLEEP,
             &delayCalibrationConstant);
  assert(result == RC_OK);

  finalResult = driver_main();

  ////maps_fini();

  return finalResult;
}
