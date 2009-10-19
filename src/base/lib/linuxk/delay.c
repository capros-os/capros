/*
 * Copyright (C) 2007, 2009, Strawberry Development Group.
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

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <eros/Invoke.h>	// get RC_OK
#include <idl/capros/Sleep.h>
#include <domain/assert.h>

// Delay in a spin loop for xloops/0x10c7 microseconds.
void
__const_udelay(unsigned long xloops)
{
  result_t result = capros_Sleep_delayMicroseconds(0, xloops/0x10c7,
                      delayCalibrationConstant);
  assert(result == RC_OK);
}

// Delay in a spin loop for usecs microseconds.
void
__udelay(unsigned long usecs)
{
  result_t result = capros_Sleep_delayMicroseconds(0, usecs,
                      delayCalibrationConstant);
  assert(result == RC_OK);
}
