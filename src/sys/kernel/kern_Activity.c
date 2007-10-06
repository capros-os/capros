/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Check.h>
#include <kerninc/Invocation.h>
#include <kerninc/IRQ.h>
#include <kerninc/Machine.h>
#include <kerninc/SysTimer.h>
#include <arch-kerninc/KernTune.h>
#include <kerninc/CPU.h>
#include <kerninc/Node.h>
#include <kerninc/Process.h>
#include <kerninc/PhysMem.h>
#include <kerninc/CpuReserve.h>
#include <kerninc/util.h>
#ifndef NDEBUG
#include <disk/DiskNodeStruct.h>
#endif

/* #define THREADDEBUG */
/*#define RESERVE_DEBUG*/

const char *act_stateNames[act_NUM_STATES] = {
    "Free",
    "Ready",
    "Running",
    "Stalled",
};

Activity *act_ActivityTable = 0;

/* During an invocation, we may allocate an Activity for later use: */
Activity * allocatedActivity = 0;

DEFQUEUE(freeActivityList);
uint32_t act_RunQueueMap = 0;

INLINE bool 
act_IsRunnable(Activity * thisPtr)
{
  return (thisPtr->context
	  && proc_IsRunnable(thisPtr->context)
	  && (thisPtr->context->processFlags & capros_Process_PF_FaultToProcessKeeper) == 0);
}

/* do all wakeup work in this function */
/* so activity wakeup just calls the function pointer */
static void 
readyq_GenericWakeup(ReadyQueue *r, Activity *t)
{
  assert(link_isSingleton(&t->q_link));

  if (t->wakeTime) {
    assert ( t->state != act_Running );
    sysT_CancelAlarm(t);
    t->wakeTime = 0;
  }

  assertex(t, (r->mask <= (1u << pr_High)));

  act_RunQueueMap |= r->mask;

  act_Enqueue(t, &r->queue);

  t->lastq = &r->queue;

  t->state = act_Ready;
}

void
readyq_ReserveWakeup(ReadyQueue *r, Activity *t)
{
  Reserve *res = 0;

  irq_DISABLE();
  assert(link_isSingleton(&t->q_link));

  if (t->wakeTime) {
    assert ( t->state != act_Running );
    sysT_CancelAlarm(t);
    t->wakeTime = 0;
  }

  assertex(t, (r->mask <= (1u << pr_High)));
  res = (Reserve *)r->other;

  act_RunQueueMap |= r->mask;
  res->timeAcc = 0;
  res->nextDeadline = sysT_Now() + res->period;
  res_SetActive(res->index);

  act_Enqueue(t, &r->queue);

  t->lastq = &r->queue;

  t->state = act_Ready;

#ifdef RESERVE_DEBUG
  printf("wokeup reserve %d", res->index);
  printf(" next deadline = %d\n", 
         /*mach_TicksToMilliseconds*/(res->nextDeadline));
#endif
  irq_ENABLE();
}

void 
readyq_Timeout(ReadyQueue *r, Activity *t)
{
  irq_DISABLE();

  //printf("now = %d", sysT_Now());
  //printf(" in generic timeout at %d\n", t->readyQ->mask);
  act_Wakeup(t);

  irq_ENABLE();
}

void
readyq_ReserveTimeout(ReadyQueue *r, Activity *t)
{
  Reserve *res = (Reserve *)r->other;

  irq_DISABLE();
#ifdef RESERVE_DEBUG
  printf("reserve %d in timeout...", res->index);
  printf(" time left = %d", 
         res->timeLeft);
  printf(" deadline = %d", /*mach_TicksToMilliseconds*/(res->nextDeadline));
  printf(" p = %d", res->period);
  printf(" d = %d", res->duration);
  printf(" active = %d\n", res->isActive);
#endif
  
  /* here, enqueue the activity back on its readyQ. we don't want to call wakeup
     here because that will replenish the reserve. 
  */
  assert(link_isSingleton(&t->q_link));

  if (t->wakeTime) {
    assert ( t->state != act_Running );
    sysT_CancelAlarm(t);
    t->wakeTime = 0;
  }

  assertex(t, (r->mask <= (1u << pr_High)));

  act_RunQueueMap |= r->mask;
  if (res->isActive == false) {
#ifdef RESERVE_DEBUG
    printf("calling deplenish on reserve index %d\n", res->index);
#endif
    res_DeplenishReserve(res);
  }

  //res_SetActive(res->index);

  //res->timeLeft = 0;

  act_Enqueue(t, &r->queue);

  t->lastq = &r->queue;

  t->state = act_Ready;

  irq_ENABLE();
}

#define pr_Seven 7
ReadyQueue prioQueues[pr_High+1] = {
  {{INITQUEUE(prioQueues[0].queue)}, (1u<<pr_Never), 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[1].queue)}, (1u<<pr_Idle), 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[2].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[3].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[4].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[5].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[6].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[7].queue)}, (1u<<pr_Seven), 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[8].queue)}, (1u<<pr_Normal), 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[9].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[10].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[11].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[12].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[13].queue)}, 0, 0, 
   readyq_GenericWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[14].queue)}, (1u<<pr_Reserve), 0, 
   readyq_ReserveWakeup, readyq_Timeout},
  {{INITQUEUE(prioQueues[15].queue)}, (1u<<pr_High), 0, 
   readyq_GenericWakeup, readyq_Timeout},
};

ReadyQueue *dispatchQueues[pr_High+1] = {
  &prioQueues[0],
  &prioQueues[1],
  &prioQueues[2],
  &prioQueues[3],
  &prioQueues[4],
  &prioQueues[5],
  &prioQueues[6],
  &prioQueues[7],
  &prioQueues[8],
  &prioQueues[9],
  &prioQueues[10],
  &prioQueues[11],
  &prioQueues[12],
  &prioQueues[13],
  &prioQueues[14],
  &prioQueues[15]
};

/* This constructor used when fabricating new activitys in IT_Send.
 * Reserve field will get populated when activity migrates to receiving
 * context. 
 */

void
act_InitActivity(Activity *thisPtr)
{
  link_Init(&thisPtr->q_link);
  keyBits_InitToVoid(&thisPtr->processKey);
  thisPtr->context = 0;
  thisPtr->state = act_Stall;
  thisPtr->readyQ = dispatchQueues[pr_Never];
}

Activity *
kact_InitKernActivity(const char * name, 
		    Priority prio,
                    ReadyQueue *rq,
                    void (*pc)(void), uint32_t *StackBottom, 
                    uint32_t *StackTop)
{
  Activity *t = act_AllocActivity();

  act_InitActivity(t);		/* why?? */
  /*t->priority = prio;*/
  t->context = kproc_Init(name, t,
			  prio, rq, pc,
			  StackBottom, StackTop);

  return t;
}

Activity *
act_AllocActivity() 
{
  Activity *t = 0;

  irq_DISABLE();

  if (sq_IsEmpty(&freeActivityList))
    fatal("Activitys exhausted\n");

  t = act_DequeueNext(&freeActivityList);
  /*printf("returning activity with p = %d\n", t->priority);*/

  irq_ENABLE();

  return t;
}

/* Note -- at one point I tried to make all activitys have domain keys,
 * and inoperative activitys have domain keys with bad values.  This
 * DOES NOT WORK, because if the root of the process is rescinded with
 * a prepared key in an outstanding activity that key will get zeroed.
 * Yuck!.
 */
void
act_AllocActivityTable() 
{
  int i = 0;

  act_ActivityTable = MALLOC(Activity, KTUNE_NACTIVITY);

  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    act_InitActivity(&act_ActivityTable[i]);
    act_DeleteActivity(&act_ActivityTable[i]);
  }

  printf("Allocated User Activitys: 0x%x at 0x%08x\n",
	 sizeof(Activity[KTUNE_NACTIVITY]), act_ActivityTable);
}

void
act_DeleteActivity(Activity *t)
{
  /* dprintf(true, "Deleting activity 0x%08x\n", t); */
  /* not hazarded because activity key */
  key_NH_SetToVoid(&t->processKey);
  t->state = act_Free;
  if (act_curActivity == t) {
#if 0
    printf("curactivity 0x%08x is deleted\n", t);
#endif
    act_curActivity = 0;
    /* act_curActivity == 0 ==> act_yieldState == ys_ShouldYield */
    act_yieldState = ys_ShouldYield;
  }

  if (t->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve *r = (Reserve *)t->readyQ->other;

    printf("removing reserve entry %d", r->index);
    printf(" . entry total time used in ms = %U\n",
           mach_TicksToMilliseconds(r->totalTimeAcc));
    res_SetInactive(r->index);
  }

  irq_DISABLE();

  act_Enqueue(t, &freeActivityList);

  irq_ENABLE();
}

#ifndef NDEBUG

bool 
act_ValidActivityKey(Activity* thisPtr, const Key* pKey)
{
  int i;
  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    if ( &act_ActivityTable[i].processKey == pKey )
      return true;
  }

  return false;
}

#endif

void
act_Enqueue(Activity *t, StallQueue *q)
{
  irq_DISABLE();

  assert(link_isSingleton(&t->q_link));

  t->lastq = q;
  
  link_insertBefore(&q->q_head, &t->q_link);
#if 0
  if (t->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve *r = (Reserve *)t->readyQ->other;
    printf("enqueued activity on reserve index %d\n", r->index);
  }
#endif
  irq_ENABLE();
}

void
act_Dequeue(Activity *t)
{
  irq_DISABLE();

  link_Unlink(&t->q_link);

  irq_ENABLE();
}
 
Activity *
act_DequeueNext(StallQueue *q)
{
  Activity *t = 0;

  irq_DISABLE();

  if (!sq_IsEmpty(q)) {
    t = (Activity *) q->q_head.next;
    link_Unlink(&t->q_link);
  }

  irq_ENABLE();

  return t;
}
 
void 
act_SleepOn(StallQueue * q /*@ not null @*/)
{
  Activity * thisPtr = act_Current();
#ifndef NDEBUG
  act_ValidateActivity(thisPtr);
#endif

  irq_DISABLE();

#if defined(DBG_WILD_PTR)
  if (dbg_wild_ptr && 0)
    check_Consistency("In Activity::SleepOn()");
#endif

  if (thisPtr->state != act_Running && ! link_isSingleton(&thisPtr->q_link) )
    fatal("Activity 0x%08x (%s) REqueues q=0x%08x lastq=0x%08x state=%d\n",
		  thisPtr, act_Name(thisPtr), &q, thisPtr->lastq, thisPtr->state);

  thisPtr->lastq = q;
  
  act_Enqueue(thisPtr, q);

  thisPtr->state = act_Stall;

  if (thisPtr->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve *r = (Reserve *)thisPtr->readyQ->other;
    r->lastDesched = sysT_Now();
    res_SetInactive(r->index);
#ifdef RESERVE_DEBUG
    printf("inactive reserve %d - in SleepOn()\n", r->index);
#endif
  }
  
#ifdef OPTION_DDB
  {
    extern bool ddb_activity_uqueue_debug;
    if (act_IsUser(thisPtr) && ddb_activity_uqueue_debug)
      dprintf(true, "Activity 0x%08x sleeps on queue 0x%08x\n",
		      thisPtr, q);
  }
#endif
  irq_ENABLE();
}

/* Activitys only use the timer system when they are about to yield,
 * sleep, so do not preempt them once they set a timer.
 */
void 
act_WakeUpIn(Activity* thisPtr, uint64_t ms)
{
  assert (thisPtr->state == act_Running);


  thisPtr->wakeTime = sysT_Now() + mach_MillisecondsToTicks(ms);

  /* see above */
  assert(thisPtr->state == act_Running);

  sysT_AddSleeper(thisPtr);

  assert (thisPtr->state == act_Running);
}


void 
act_WakeUpAtTick(Activity* thisPtr, uint64_t tick) 
{  
  thisPtr->wakeTime = tick;
  sysT_AddSleeper(thisPtr);
}

void 
act_Wakeup(Activity* thisPtr)
{
  irq_DISABLE();
  
  thisPtr->readyQ->doWakeup(thisPtr->readyQ, thisPtr);

  /* Do not set activityShouldYield until the first activity runs!
   * There may not be a curactivity if we are being woken up because
   * the previous activity died.
   * assert(curActivity);
   */
  /* change from thisPtr->priority to thisPtr->readyQ->mask */
  if (act_curActivity && thisPtr->readyQ->mask > act_curActivity->readyQ->mask) {
#if 0
    printf("Wake 0x%08x Cur activity should yield. canPreempt=%c\n",
		   this, canPreempt ? 'y' : 'n');
#endif
 
    act_ForceResched();
  }
  irq_ENABLE();

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("In Activity::Wakeup()");
#endif

}

/* This relies on the fact that the context will overwrite our process
 * key slot if it is unloaded!
 * 
 * It proves that the only activity migrations that occur in EROS are to
 * processs that are runnable.  If a process is runnable, we know that
 * it has a proper schedule key.  We can therefore assume that the
 * reserve slot of the destination context is populated, and we can
 * simply pick it up and go with it.
 */
void 
act_MigrateTo(Activity * thisPtr, Process * dc)
{

  if (thisPtr->context)
    proc_Deactivate(thisPtr->context);
    
  thisPtr->context = dc;
  if (dc) {
    proc_SetActivity(dc, thisPtr);
    assert (proc_IsRunnable(dc));

    /* FIX: Check for preemption! */
    if (thisPtr->readyQ == dispatchQueues[pr_Never]) {
      thisPtr->readyQ = dc->readyQ;
    }
    else {
      thisPtr->readyQ = dc->readyQ;
      //if (thisPtr->readyQ->mask & (1u<<pr_Reserve))
       
      //if (thisPtr->readyQ->mask < dc->readyQ->mask)
      //act_ForceResched();
    }    
  }
  else {
    act_DeleteActivity(thisPtr); /* migrate to 0 context => kill curActivity */
  }
}

void 
sysT_ActivityTimeout()
{
  //irq_DISABLE();
  
  //printf("start QuantaExpired...%d\n", sysT_Now());
  
  if (act_curActivity && act_curActivity->readyQ->other) {
    Reserve *r = (Reserve *)act_curActivity->readyQ->other;
    
    if (r->isActive) {
      r->lastDesched = sysT_Now();
#ifdef RESERVE_DEBUG
      printf("old time left = %d", r->timeLeft);
#endif
      r->timeAcc += r->lastDesched - r->lastSched;
      r->totalTimeAcc += r->timeAcc;
#ifdef RESERVE_DEBUG
      printf(" reserve index = %d", r->index);
      printf(" time left = %u", r->timeLeft);
      printf(" duration = %u\n", r->duration);
#endif
#if 0
      printf(" active = %d", r->isActive);
      printf(" table active = %d\n", res_ReserveTable[r->index].isActive);
#endif
      if (r->timeAcc >= r->duration) {
#ifdef RESERVE_DEBUG
        printf("reserve exhausted: %d\n", r->timeLeft);
#endif
        res_SetInactive(r->index);
      }
#ifdef RESERVE_DEBUG
      printf("done QuantaExpired for reserve...\n");
#endif
    }
  }

  DoNeedReplenish();
  act_ForceResched();
}

inline void 
act_ChooseNewCurrentActivity()
{    
  int runQueueNdx;
  Reserve *res = 0;

  assert( irq_DISABLE_DEPTH() == 1 );

  irq_DISABLE();
  //printf("starting ChooseNew()...\n");
  /* idle activitys should always be ready */
  assert(act_RunQueueMap);

  for ( ;; ) {
    runQueueNdx = fmsb(act_RunQueueMap);

    //printf("run queue ndx = %d\n", runQueueNdx);
    if (runQueueNdx == pr_Reserve) {
      /* make the dispatchQ for index pr_Reserve
         point to the reserve entry readyQ */

      //printf("ChooseNew() says there is a reserve...\n");
      res = res_GetEarliestReserve();
      if (res == 0) {
        //printf("GetEarliest says there is no reserve...\n");
        act_RunQueueMap &= ~(1u << runQueueNdx);
        continue;
      }
      else {
        dispatchQueues[pr_Reserve] = &res->readyQ;
#ifdef RESERVE_DEBUG
        printf("reserve chosen in choosenew() is index %d", res->index);
        printf(" active = %d", res->isActive);
        printf(" now = %d\n", sysT_Now());
#endif
      }
    }

    if (!sq_IsEmpty(&dispatchQueues[runQueueNdx]->queue))
      break;

    /* if (runQueueNdx == pr_Reserve)
       break;*/

    dprintf(true, "Run queue %d is empty\n", runQueueNdx);

    /* RunQueueMap is advisory. If there was nothing in the queue,
       turn off the bit and try again. */

    act_RunQueueMap &= ~ (1u << runQueueNdx);
  }

  /* If we dispatch pr_Never, something is really wrong: */
  assert(runQueueNdx > pr_Never);

  /* Now know we have the least non-empty queue. Yank the activity. */
  act_SetRunning((Activity *)(dispatchQueues[runQueueNdx]->queue.q_head.next));

  if (sq_IsEmpty(&dispatchQueues[runQueueNdx]->queue)) {
    /* watch out for the case where a activity from a reserve is chosen */
    /* that case is handled above */
    if (runQueueNdx != pr_Reserve)
      act_RunQueueMap &= ~(1u << runQueueNdx);
    //if (runQueueNdx == pr_Reserve)
    //  res_SetInactive(res->index);
  }

  irq_ENABLE();

  assert( irq_DISABLE_DEPTH() == 1 );
}

/* DoReschedule() is called for a number of reasons:
 * 
 *     1. No current activity
 *     2. Current activity preempted
 *     3. Current activity not prepared
 *     4. Current activity's context not prepared
 *     5. Current activity has fault code (keeper invocation needed)
 * 
 * In the old logic, only activity prepare could Yield().  In the new
 * logic, the keeper invocation may also yield.  For this reason, both
 * the prepare logic and the keeper invocation logic are in separate
 * functions for now.  I am seriously contemplating integrating them
 * into the main code body and setting up a recovery block here.
 */

/* FIX: Somewhere in here the context pins are not getting updated
 * correctly.  It's not important until we do SMP.
 */
// May Yield.
void 
act_DoReschedule(void)
{
#ifndef NDEBUG
  if (irq_DISABLE_DEPTH() != 1) {
    printf("irq_DISABLE_DEPTH = %d ", irq_DISABLE_DEPTH());
    assert(irq_DISABLE_DEPTH() == 1);
  }
#endif

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr && 0)
    check_Consistency("In DoReschedule()");
#endif

  /* On the way out of an invocation trap there may be no current
   * activity, in which case we may need to choose a new one:
   */
  if (act_curActivity == 0) {
    //printf("in no cur_Activity case...calling ChooseNew\n");
    act_ChooseNewCurrentActivity();
    act_yieldState = 0;
  }
  else if (act_yieldState == ys_ShouldYield) {
    /* Current activity may be stalled or dead; if so, don't stick it
     * back on the run list!
     */
    if ( act_curActivity->state == act_Running ) {
      act_curActivity->readyQ->doQuantaTimeout(act_curActivity->readyQ,
                                             act_curActivity);
#ifdef ACTIVITYDEBUG
      if ( act_curActivity->IsUser() )
	printf("Active activity goes to end of run queue\n");
#endif
    }

    act_yieldState = 0;
    act_ChooseNewCurrentActivity();
  }
  
  do {
    /* Clear all previous user pins. 
       Otherwise this loop could pin an unbounded number of objects. */
    objH_BeginTransaction();
    
#ifdef DBG_WILD_PTR
    if (dbg_wild_ptr && 0)
      check_Consistency("In DoReschedule() loop");
#endif
#if 0
    printf("schedloop: curActivity 0x%08x ctxt 0x%08x fc 0x%x\n",
		   Activity::curActivity, Activity::curActivity->context,
		   Activity::curActivity->context ? Activity::curActivity->context->faultCode
		   : 0);
#endif

    assert (act_curActivity);
    
    /* If activity cannot be successfully prepared, it cannot (ever) run,
     * and should be returned to the free activity list.  Do this even if
     * we are rescheduling, since we want the activity entry back promptly
     * and it doesn't take that long to test.
     */
    if ( act_curActivity && act_Prepare(act_curActivity) == false ) {
      assert( act_IsUser(act_curActivity) );
      

      /* We shouldn't be having this happen YET */
      fatal("Current activity no longer runnable\n");

      act_DeleteActivity(act_curActivity);
      assert (act_curActivity == 0);
      act_ChooseNewCurrentActivity();
    }

    assert (act_curActivity);

    /* Activity might have gone to sleep as a result of context being prepared. */
    /* But in that case, wouldn't we have gone to act_Yield, not here? CRL */
    if (act_curActivity->state != act_Running)
      act_ChooseNewCurrentActivity();

    if (act_curActivity->context
	&& act_curActivity->context->processFlags & capros_Process_PF_FaultToProcessKeeper) {
      proc_InvokeProcessKeeper(act_curActivity->context);

      /* Invoking the process keeper either stuck us on a sleep queue, in
       * which case we Yielded(), or migrated the current activity to a
       * new domain, in which case we will keep trying: */
      if (act_curActivity == 0 || act_curActivity->state != act_Running)
        act_ChooseNewCurrentActivity();
    }

#if 0
    static count = 0;
    count++;
    if (count % 100 == 0) {
      count = 0;
      printf("schedloop: curActivity 0x%08x ctxt 0x%08x (%s) run?"
		     " %c st %d fc 0x%x\n",
		     curActivity,
		     curActivity->context,
		     curActivity->context->Name(),
		     curActivity->context->IsRunnable() ? 'y' : 'n',
		     curActivity->state,
		     curActivity->context->faultCode);
    }
#endif

  } while (act_IsRunnable(act_curActivity) == false);

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom DoReschedule()");
#endif

  assert (act_curActivity);

  assert (act_curActivity->context);

  assert (act_curActivity->readyQ == act_CurContext()->readyQ);

  assert( irq_DISABLE_DEPTH() == 1 );
  act_yieldState = 0;		/* until proven otherwise */

  if (act_curActivity->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve *r = (Reserve *)act_curActivity->readyQ->other;

#ifdef RESERVE_DEBUG
    printf("dispatching reserve...deadline = %u", 
           /*mach_TicksToMilliseconds*/(r->nextDeadline));
    printf(" index = %d", r->index);
    printf(" active = %d", r->isActive);
    printf(" time left = %d\n", r->timeLeft);
#endif
    r->lastSched = sysT_Now();
    cpu->preemptTime = NextTimeInterrupt(r);
  }
  else {
    cpu->preemptTime = NextTimeInterrupt(0);
  }

  sysT_ResetWakeTime();

  assert( irq_DISABLE_DEPTH() == 1 );
}

// May Yield.
void
ExitTheKernel(void)
{
  UpdateTLB();
  
  if ((act_yieldState != 0)
      || ! act_IsRunnable(act_curActivity) ) {
    act_DoReschedule();
  }
  assert(act_Current());

  Process * thisPtr = act_CurContext();

#ifndef NDEBUG
  if (dbg_inttrap)
    dprintf(true, "Resuming proc 0x%08x\n", thisPtr);

  if ( thisPtr->curActivity != act_Current() )
    fatal("Context 0x%08x (%d) not for current activity 0x%08x (%d)\n",
	       thisPtr, thisPtr - proc_ContextCache,
		  act_Current(),
		  act_Current() - act_ActivityTable);

  if ( act_CurContext() != thisPtr )
    fatal("Activity context 0x%08x not me 0x%08x\n",
	       act_CurContext(), thisPtr);

#endif

  /* IRQ must be disabled here. */
  assert( irq_DISABLE_DEPTH() == 1 );

  /* While a process is running, irq_DisableDepth is zero.
     After all, IRQ is enabled. 
     irq_DisableDepth is incremented again on an exception. 
     But no one should be looking at that, except maybe privileged
     processes. */

  // Call architecture-dependent C code for resuming a process.
  ExitTheKernel_MD(thisPtr);

  // Call architecture-dependent assembly code for resuming a process.
  resume_process(thisPtr);	// does not return
}

void 
sq_WakeAll(StallQueue* q, bool b/* verbose*/)
{
  Activity *t;

  irq_DISABLE();

  while ((t = act_DequeueNext(q))) {
#ifdef OPTION_DDB
    {
	extern bool ddb_activity_uqueue_debug;
        
	if (ddb_activity_uqueue_debug && act_IsUser(t) )
	  dprintf(true, "Waking up activity 0x%08x (%s)\n",
		  t, act_Name(t));
    }
#endif

#ifndef NDEBUG
    act_ValidateActivity(t);
#endif

    act_Wakeup(t);
  }

  irq_ENABLE();
}

bool 
sq_IsEmpty(StallQueue* q)
{
  bool result = false;
  
  irq_DISABLE();

  if (link_isSingleton(&q->q_head))
    result = true;
  
  irq_ENABLE();

  return result;
}

#ifndef NDEBUG
void
ValidateAllActivitys()
{
  int i = 0;

  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    if ( act_ActivityTable[i].state != act_Free )
      act_ValidateActivity(&act_ActivityTable[i]);
  }
}

void 
act_ValidateActivity(Activity* thisPtr)
{
  uint32_t wthis = (uint32_t) thisPtr;
  uint32_t wtbl = (uint32_t) act_ActivityTable;
  
  if (wthis < wtbl)
    fatal("Activity 0x%x pointer too low\n",(uint32_t)thisPtr);

  wthis -= wtbl;

  if (wthis % sizeof(Activity))
    fatal("Activity 'this' pointer not valid.\n");

  if ((wthis / sizeof(Activity)) >= KTUNE_NACTIVITY)
    fatal("Activity 'this' pointer too high.\n");

  if (!thisPtr->context && keyBits_GetType(&thisPtr->processKey) != KKT_Process  &&
      ! keyBits_IsVoidKey(&thisPtr->processKey))
    fatal("Activity 0x%08x has bad key type.\n", thisPtr);
}


#endif

const char* 
act_Name(Activity* thisPtr)
{
  static char userActivityName[] = "userXXX\0";
  uint32_t ndx = 0;

  if ( !act_IsUser(thisPtr) )
    return proc_Name(thisPtr->context);
    
  ndx = thisPtr - &act_ActivityTable[0];

  if (ndx >= 100) {
    userActivityName[4] = (ndx / 100) + '0';
    ndx = ndx % 100;
    userActivityName[5] = (ndx / 10) + '0';
    ndx = ndx % 10;
    userActivityName[6] = ndx + '0';
    userActivityName[7] = 0;
  }
  else if (ndx >= 10) {
    userActivityName[4] = (ndx / 10) + '0';
    ndx = ndx % 10;
    userActivityName[5] = ndx + '0';
    userActivityName[6] = 0;
  }
  else {
    userActivityName[4] = ndx + '0';
    userActivityName[5] = 0;
  }

  return userActivityName;
}



/* ALERT
 * 
 * There are four places to which a user activity Yield() can return:
 * 
 *    Activity::Prepare() -- called from the scheduler
 *    Activity::InvokeMyKeeper() -- called from the scheduler
 *    The page fault handler
 *    The gate jump path.
 * 
 * We would prefer a design in which there was only one recovery
 * point, but if Activity::Prepare() is being called, it is possible
 * likely that the activity's Context entry is gone, and even possible
 * that the domain root has gone out of memory.
 * 
 * Note that a activity's prepare routine must always be called by that
 * activity, and not by any other.  Activitys prepare themselves.
 * Contexts, by contrast, can be prepared by anyone.
 * 
 * That said, here's the good news: it is not possible for the uses to
 * conflict.  Any action taken as a result of a gate jump that causes
 * the activity to Yield() will have put the activity to sleep, in which
 * case it's not being prepared.
 */

/* May Yield. */
bool 
act_Prepare(Activity* thisPtr)
{
  static uint32_t count;
  
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr && 0)
    check_Consistency("Before ThrdPrepare()");
#endif

  /* In the absence of the PTE zap case, which is now handled
   * separately, it is not clear that we still need the for(;;)
   * loop...
   */
  
  for (count = 0; count < 20;count++) {
    /* If we have a context, the activity ain't dead yet! */
    
    if ( thisPtr->context && proc_IsRunnable(thisPtr->context) ) {
#if CONV
      assert (thisPtr->priority == thisPtr->context->priority);
#else
      assert (thisPtr->readyQ == thisPtr->context->readyQ);
#endif
      
      return true;
    }
    
    assert (thisPtr->state != act_Free); /* is the old code right?? */
    
    /* Probably not necessary, but it doesn't do any harm: */
    assert( act_Current() == thisPtr );

    /* Try to prepare this activity to run: */
#ifdef ACTIVITYDEBUG
    printf("Preparing user activity\n");
#endif
  
    if ( thisPtr->context == 0 ) {
      /* Domain root may have been rescinded.... */

#if 0
      printf("Prepping dom key ");
      key_Print(&thisPtr->processKey);
#endif
    
      key_Prepare(&thisPtr->processKey);

      if (keyBits_IsType(&thisPtr->processKey, KKT_Process) == false) {
	fatal("Rescinded activity!\n");
	return false;
      }
  
      assert( keyBits_IsHazard(&thisPtr->processKey) == false );
      assert( keyBits_IsPrepared(&thisPtr->processKey) );

      thisPtr->context = node_GetDomainContext((Node *) thisPtr->processKey.u.ok.pObj);
      if (!thisPtr->context)
	return false;
    
      proc_SetActivity(thisPtr->context, thisPtr);
    }

#ifdef ACTIVITYDEBUG
    printf("Preparing context 0x%08x..\n", thisPtr->context);
#endif

    proc_Prepare(thisPtr->context);

    if ( proc_IsRunnable(thisPtr->context) ) {
#if CONV
      assert (thisPtr->priority == thisPtr->context->priority);
#else
      assert (thisPtr->readyQ = thisPtr->context->readyQ);
#endif
#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr && 0)
	check_Consistency("After ThrdPrepare()");
#endif
      return true;
    }

    /* The context could not be prepared. */
  }

  dprintf(true, "Activity start loop exceeded\n");
  return false;
}

void // does not return
act_HandleYieldEntry(void)
{
  /* This routine is really another kernel entry point.  When called,
     the current process is giving up the processor, and is most
     likely (but not always) asleep on a stall queue.

     We do a few cleanups that are useful in some or all of the
     various Yield() paths, call Reschedule, and then resume the
     activity that is current following reschedule.
  */     

  assert (act_Current());

  /* If we yielded from within the IPC path, we better not be yielding
     after the call to COMMIT_POINT(): */
  assert (InvocationCommitted == false);

#ifndef NDEBUG
  act_ValidateActivity(act_Current());
#endif

  inv_Cleanup(&inv);

  // If we allocated an Activity, we no longer need it:
  if (allocatedActivity) {
    assert(! allocatedActivity->context);	// it shouldn't have been used
    act_DeleteActivity(allocatedActivity);
    allocatedActivity = 0;
  }
  
  act_ForceResched();

  assertex(act_CurContext(), act_CurContext()->runState == RS_Running);

  /* At this time, the activity rescheduler logic thinks it must run
     disabled. I am not convinced that it really needs to, but it is
     simpler not to argue with it here.  Do this check to avoid
     disabling interrupts recursively forever.  Also, this check has
     the right effect whether or not interrupts were enabled in
     OnKeyInvocationTrap(). */
  
  if (irq_DISABLE_DEPTH() == 0)
    irq_DISABLE();
  
  assert( irq_DISABLE_DEPTH() == 1 );
  ExitTheKernel();
}
