/*
 * Copyright (C) 2007, 2008, 2009, Strawberry Development Group.
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
#include <idl/capros/GPT.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <domain/CMTEMaps.h>
#include <domain/assert.h>

/* This is the code to initialize a driver process. */

extern void driver_main(void);
/* driver_main should be NORETURN, but don't declare it so,
 * so we can detect whether it returns. */

int
cmte_main(void)
{
  result_t result;

  assert(LKSN_APP == LKSN_CMTE);	// otherwise need to allocate
		// range in KR_KEYSTORE

  maps_init();

  // Initialize delayCalibrationConstant.
  result = capros_Sleep_getDelayCalibration(KR_SLEEP,
             &delayCalibrationConstant);
  assert(result == RC_OK);

  driver_main();
  assert(false);	// driver_main should not return
  return 0;
}
