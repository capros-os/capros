/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
 *
 * This file is part of the EROS Operating System.
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

#include <kerninc/kernel.h>
#include <kerninc/Check.h>
#include <kerninc/Activity.h>
/*#include <kerninc/util.h>*/
#include <kerninc/ObjectHeader.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/IRQ.h>
#include <kerninc/Process.h>
#include <kerninc/Invocation.h>
#include "lostart.h"

/* #define THREAD386DEBUG */

extern void kernel_thread_yield();

void 
act_HandleYieldEntry(void)
{
  /* This routine is really another kernel entry point.  When called,
     the current process is giving up the processor, and is most
     likely (but not always) asleep on a stall queue.

     We do a few cleanups that are useful in some or all of the
     various Yield() paths, call Reschedule, and then resume the
     activity that is current following reschedule.

  */     
  extern Invocation inv;
  extern Activity *activityToRelease;
  
  assert (act_Current());

  /* If we yielded from within the IPC path, we better not be yielding
     after the call to COMMIT_POINT(): */
  assert (InvocationCommitted == false);

#if defined(OPTION_DDB)
  {
    extern bool ddb_uyield_debug;
    if ( ddb_uyield_debug )
      dprintf(true, "Activity 0x%08x yields\n",
		      act_Current()); 
  }
#endif

#ifndef NDEBUG
  act_ValidateActivity(act_Current());
#endif

  objH_ReleasePinnedObjects();


  inv_Cleanup(&inv);

  /* If we were in a SEND invocation, release the activity: */

  if (activityToRelease)
    act_MigrateTo(activityToRelease, 0);
  
  act_ForceResched();

  if (act_Current()->context) {
    act_CurContext()->runState = RS_Running;
  }

  /* At this time, the activity rescheduler logic thinks it must run
     disabled. I am not convinced that it really needs to, but it is
     simpler not to argue with it here.  Do this check to avoid
     disabling interrupts recursively forever.  Also, this check has
     the right effect whether or not interrupts were enabled in
     OnKeyInvocationTrap(). */
  
  if (irq_DISABLE_DEPTH() == 0)
    irq_DISABLE();
  
  assert( irq_DISABLE_DEPTH() == 1 );
  //printf("about to call act_Reschedule...%d\n", act_curActivity->readyQ->mask);
  act_Reschedule();
  act_Resume(act_Current());
}

