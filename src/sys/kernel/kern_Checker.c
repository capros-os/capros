/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* Low priority background activity to do consistency checking on kernel
 * data structures.  Runs through the object cache, the activity list,
 * etc. making sure that things look kosher.
 */

#include <kerninc/kernel.h>
/*#include <kerninc/Check.h>*/
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Node.h>
#include <kerninc/IRQ.h>

#ifdef STRESS_TEST
#define DELAY 2ll
#define INVAL_MODULUS 1
#else
#define DELAY 7000ll
#define INVAL_MODULUS 4
#endif

/* Once main() exits and activitys are started, this activity drives the
 * completion of the bootup process.
 */

#define StackSize 1024

extern void check_Consistency(const char *); 

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

void CheckActivity_UnprepareObjects();

/* Unlike all other activities, this one runs once and then exits (or at
 * least sleeps forever).
 */
void
CheckActivity_Start()
{
  int stack;

  printf("Start CheckActivity (activity 0x%x,context 0x%x,stack 0x%x)\n",
	       act_curActivity, act_curActivity->context, &stack);

  for(;;) {
    pass++;
    
    assert(act_curActivity->state == act_Running);

    check_Consistency("chkthrd");
  
    assert(act_curActivity->state == act_Running);
    
    CheckActivity_UnprepareObjects();
    
    assert(act_curActivity->state == act_Running);

    assert( act_CAN_PREEMPT(0) ); /* the parameter is unused in act_CAN_PREEMPT() */

    irq_DISABLE();   
    act_WakeUpIn(act_curActivity, DELAY);

    /* printf("Checkactivity pass %d state %d\n", pass, Activity::state); */
    assert(act_curActivity->state == act_Running);

    act_SleepOn(act_curActivity, &DeepSleepQ);
    
    irq_ENABLE();

    /* is this correct?? */
    act_DirectedYield(act_curActivity, false);
  }

    /* We will never wake up again... */
  act_SleepOn(act_curActivity, &DeepSleepQ);

  /* is this correct?? */
  act_DirectedYield(act_curActivity, false);

}

void
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
      act_DirectedYield(act_curActivity, false);
    }
  }
#endif
  if ((pass % 2) == 1) {
    for (nd = 0; nd < objC_NumCoreNodeFrames(); nd++) {

      Node *pNode = objC_GetCoreNodeFrame(nd);

      if (objH_IsFree(DOWNCAST(pNode, ObjectHeader)))
	continue;

#if 0
      if (pNode->obType <= ObType::NtSegment)
	pNode->DiscardObTableEntry(false);
#endif

      act_DirectedYield(act_curActivity, false);
    }
  }
}

