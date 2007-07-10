/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* TimeOfDay key -- returns current time of day and/or current tick. */

#include <kerninc/kernel.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>
#include <eros/TimeOfDay.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Activity.h>
#include <eros/Invoke.h>
#include <eros/StdKeyType.h>

#ifdef KKT_TimeOfDay

void
TimeOfDayKey(Invocation& inv)
{
  switch(inv.entry.code) {
  case OC_TimeOfDay_Now:	/* get current time of day. */
    {
      TimeOfDay tod;
      Machine::GetHardwareTimeOfDay(tod);

      inv.invokee->SetupExitString(inv, sizeof(tod));
      
      COMMIT_POINT();

      (void) inv.CopyOut(sizeof(tod), &tod);
      inv.exit.code = RC_OK;

      /* Make sure we do not commit redundantly! */
      return;
    }
  case OC_TimeOfDay_UpTime:	/* get ms since startup. */
    {
      uint64_t curTick = SysTimer::Now();
      uint64_t ms = Machine::TicksToMilliseconds(curTick);
      
      inv.exit.w1 = ms;
      inv.exit.w2 = (ms>>32);
      inv.exit.code = RC_OK;
      break;
    }
  case OC_capros_key_getType:
    inv.exit.code = RC_OK;
    inv.exit.w1 = AKT_TimeOfDay;
    break;
  default:
    inv.exit.code = RC_capros_key_UnknownRequest;
    break;
  }

  COMMIT_POINT();
}

#endif /* KKT_TimeOfDay */
