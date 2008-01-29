/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

/* Low priority background activity to do consistency checking on kernel
 * data structures.  Runs through the object cache, the activity list,
 * etc. making sure that things look kosher.
 */

/* NOTE: this is no longer used.
   It did not work, because kernel activities are preemptible
   and checking must not be preempted. 
   The checking loop should be moved to a preloaded process
   that invokes a (new) miscellaneous capability to do checking.
 */

#include <kerninc/kernel.h>
#include <kerninc/Check.h>
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/IRQ.h>

#ifdef STRESS_TEST
#define DELAY 2ll
#else
#define DELAY 7000ll
#endif

#define StackSize 1024

void CheckActivity_Start();

void 
StartCheckActivity()
{
  fixreg_t *stack = MALLOC(fixreg_t, StackSize);

  Activity *checkActivity = 
    kact_InitKernActivity("Checker", pr_Idle, dispatchQueues[pr_Idle], 
			&CheckActivity_Start, stack, &stack[StackSize]);

  checkActivity->readyQ = dispatchQueues[pr_Idle];
  act_Wakeup(checkActivity);	/* let it initialize itself... */

  printf("Checker...\n");

}

static uint32_t pass = 0;

static void CheckActivity_UnprepareObjects();

void
CheckActivity_Start()
{
  int stack;

  printf("Start CheckActivity (activity 0x%x,proc 0x%x,stack 0x%x)\n",
	       act_Current(), proc_Current(), &stack);

  for(;;) {
    pass++;
    
    assert(act_Current()->state == act_Running);

    check_Consistency("chkthrd");
  
    assert(act_Current()->state == act_Running);
    
    CheckActivity_UnprepareObjects();
    
    assert(act_Current()->state == act_Running);

    /* printf("Checkactivity pass %d state %d\n", pass, Activity::state); */
    
    // Sleep for DELAY ticks here ...
  }
}

static void
CheckActivity_UnprepareObjects()
{
  uint32_t nd = 0;
#if 0
  if ((pass % 2) == 0) {
    for (uint32_t pg = 0; pg < ObjectCache::NumCorePageFrames(); pg++) {
      ObjectHeader *pPage = ObjectCache::GetCorePageFrame(pg);
      if (pPage->flags.free)
	continue;

      pPage->DiscardObTableEntry(false);
      act_Yield();
    }
  }
#endif
  if ((pass % 2) == 1) {
    for (nd = 0; nd < objC_NumCoreNodeFrames(); nd++) {

      Node *pNode = objC_GetCoreNodeFrame(nd);

      if (pNode->node_ObjHdr.obType == ot_NtFreeFrame)
	continue;

#if 0
      if (pNode->obType <= ObType::NtSegment)
	pNode->DiscardObTableEntry(false);
#endif

      act_Yield();
    }
  }
}

