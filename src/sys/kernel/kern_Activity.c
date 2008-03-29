/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group.
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
#include <arch-kerninc/PTE.h>

/* #define THREADDEBUG */
/*#define RESERVE_DEBUG*/

static void act_Enqueue(Activity * t, StallQueue * q);

const char *act_stateNames[act_NUM_STATES] = {
    "Free",
    "Ready",
    "Running",
    "Stalled",
    "Sleeping",
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

static void
InactivateReserve(Activity * thisPtr)
{
  if (thisPtr->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve * r = (Reserve *)thisPtr->readyQ->other;
    r->lastDesched = sysT_Now();
    res_SetInactive(r->index);
#ifdef RESERVE_DEBUG
    printf("inactive reserve %d - in SleepOn()\n", r->index);
#endif
  }
}

/* do all wakeup work in this function */
/* so activity wakeup just calls the function pointer */
static void 
readyq_GenericWakeup(ReadyQueue *r, Activity *t)
{
  assert(link_isSingleton(&t->q_link));
  assertex(t, (r->mask <= (1u << pr_High)));

  act_RunQueueMap |= r->mask;

  irqFlags_t flags = local_irq_save();

  act_Enqueue(t, &r->queue);

  local_irq_restore(flags);

  t->state = act_Ready;
}

void
readyq_ReserveWakeup(ReadyQueue *r, Activity *t)
{
  Reserve *res = 0;

  assert(link_isSingleton(&t->q_link));
  assertex(t, (r->mask <= (1u << pr_High)));

  irqFlags_t flags = local_irq_save();

  res = (Reserve *)r->other;

  act_RunQueueMap |= r->mask;
  res->timeAcc = 0;
  res->nextDeadline = sysT_Now() + res->period;
  res_SetActive(res->index);

  act_Enqueue(t, &r->queue);

  t->state = act_Ready;

#ifdef RESERVE_DEBUG
  printf("wokeup reserve %d", res->index);
  printf(" next deadline = %d\n", 
         /*mach_TicksToMilliseconds*/(res->nextDeadline));
#endif
  local_irq_restore(flags);
}

void 
readyq_Timeout(ReadyQueue *r, Activity *t)
{
  irqFlags_t flags = local_irq_save();

  //printf("now = %d", sysT_Now());
  //printf(" in generic timeout at %d\n", t->readyQ->mask);
  act_Wakeup(t);

  local_irq_restore(flags);
}

void
readyq_ReserveTimeout(ReadyQueue *r, Activity *t)
{
  Reserve *res = (Reserve *)r->other;

  irqFlags_t flags = local_irq_save();
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

  t->state = act_Ready;

  local_irq_restore(flags);
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

Activity *
kact_InitKernActivity(const char * name, 
		    Priority prio,
                    ReadyQueue *rq,
                    void (*pc)(void), uint32_t *StackBottom, 
                    uint32_t *StackTop)
{
  Activity *t = act_AllocActivity();

  /*t->priority = prio;*/
  Process * p = kproc_Init(name, prio, rq, pc,
                           StackBottom, StackTop);
  act_SetContextNotCurrent(t, p);
  proc_SetActivity(p, t);

  return t;
}

Activity *
act_AllocActivity() 
{
  if (sq_IsEmpty(&freeActivityList))
    fatal("Activitys exhausted\n");

  Activity * t = act_DequeueNext(&freeActivityList);
  /*printf("returning activity with p = %d\n", t->priority);*/

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
    Activity * t = &act_ActivityTable[i];

    link_Init(&t->q_link);
    keyBits_InitToVoid(&t->processKey);
    act_SetContextNotCurrent(t, NULL);
    t->readyQ = dispatchQueues[pr_Never];

    act_DeleteActivity(t);
  }

  printf("Allocated User Activitys: 0x%x at 0x%08x\n",
	 sizeof(Activity[KTUNE_NACTIVITY]), act_ActivityTable);
}

void
act_DeleteActivity(Activity *t)
{
  assert(t->context == NULL);
  assert(link_isSingleton(&t->q_link));

  /* dprintf(true, "Deleting activity 0x%08x\n", t); */
  /* not hazarded because activity key */
  key_NH_SetToVoid(&t->processKey);
  t->state = act_Free;
  if (act_Current() == t) {
#if 0
    printf("curactivity 0x%08x is deleted\n", t);
#endif
    act_curActivity = NULL;
    proc_curProcess = NULL;
    /* act_curActivity == 0 ==> act_yieldState == ys_ShouldYield */
    act_ForceResched();
  }

  if (t->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve *r = (Reserve *)t->readyQ->other;

    printf("removing reserve entry %d", r->index);
    printf(" . entry total time used in ms = %U\n",
           mach_TicksToMilliseconds(r->totalTimeAcc));
    res_SetInactive(r->index);
  }
  t->readyQ = dispatchQueues[pr_Never];	// just in case

  act_Enqueue(t, &freeActivityList);
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

static void
act_Enqueue(Activity * t, StallQueue * q)
{
  assert(link_isSingleton(&t->q_link));

  t->lastq = q;
  
  link_insertBefore(&q->q_head, &t->q_link);
#if 0
  if (t->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve *r = (Reserve *)t->readyQ->other;
    printf("enqueued activity on reserve index %d\n", r->index);
  }
#endif
}

void
act_Dequeue(Activity *t)
{
  if (t->state == act_Sleeping)
    sysT_CancelAlarm(t);
  else {
    irqFlags_t flags = local_irq_save();

    link_Unlink(&t->q_link);

    local_irq_restore(flags);
  }
}
 
Activity *
act_DequeueNext(StallQueue *q)
{
  Activity *t = 0;

  irqFlags_t flags = local_irq_save();

  if (!sq_IsEmpty(q)) {
    t = (Activity *) q->q_head.next;
    link_Unlink(&t->q_link);
  }

  local_irq_restore(flags);

  return t;
}
 
void 
act_SleepOn(StallQueue * q /*@ not null @*/)
{
  Activity * thisPtr = act_Current();
#ifndef NDEBUG
  act_ValidateActivity(thisPtr);
#endif

  irqFlags_t flags = local_irq_save();

#if defined(DBG_WILD_PTR)
  if (dbg_wild_ptr && 0)
    check_Consistency("In Activity::SleepOn()");
#endif

  if (thisPtr->state != act_Running && ! link_isSingleton(&thisPtr->q_link) )
    fatal("Activity 0x%08x (%s) REqueues q=0x%08x lastq=0x%08x state=%d\n",
		  thisPtr, act_Name(thisPtr), &q, thisPtr->lastq, thisPtr->state);

  act_Enqueue(thisPtr, q);

  thisPtr->state = act_Stall;
  InactivateReserve(thisPtr);
  
#ifdef OPTION_DDB
  {
    extern bool ddb_activity_uqueue_debug;
    if (act_IsUser(thisPtr) && ddb_activity_uqueue_debug)
      dprintf(true, "Activity 0x%08x sleeps on queue 0x%08x\n",
		      thisPtr, q);
  }
#endif
  local_irq_restore(flags);
}

void 
act_SleepUntilTick(Activity * thisPtr, uint64_t tick) 
{  
  irqFlags_t flags = local_irq_save();

  sysT_AddSleeper(thisPtr, tick);

  thisPtr->state = act_Sleeping;
  InactivateReserve(thisPtr);

  local_irq_restore(flags);
}

void 
act_Wakeup(Activity* thisPtr)
{
  irqFlags_t flags = local_irq_save();
  
  thisPtr->readyQ->doWakeup(thisPtr->readyQ, thisPtr);

  /* Do not set activityShouldYield until the first activity runs!
   * There may not be a curactivity if we are being woken up because
   * the previous activity died.
   * assert(curActivity);
   */
  /* change from thisPtr->priority to thisPtr->readyQ->mask */
  if (act_Current() && thisPtr->readyQ->mask > act_Current()->readyQ->mask) {
#if 0
    printf("Wake 0x%08x Cur activity should yield. canPreempt=%c\n",
		   this, canPreempt ? 'y' : 'n');
#endif
 
    act_ForceResched();
  }
  local_irq_restore(flags);

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("In Activity::Wakeup()");
#endif

}

void
act_DeleteCurrent(void)
{
  Activity * thisPtr = act_Current();
  if (thisPtr->context)
    proc_Deactivate(thisPtr->context);
  act_SetContextCurrent(thisPtr, 0);
  act_DeleteActivity(thisPtr);
}

inline void 
act_ChooseNewCurrentActivity()
{    
  int runQueueNdx;
  Reserve *res = 0;

  assert(local_irq_disabled());

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

  assert(local_irq_disabled());
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
  assert(local_irq_disabled());

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr && 0)
    check_Consistency("In DoReschedule()");
#endif

  /* On the way out of an invocation trap there may be no current
   * activity, in which case we may need to choose a new one:
   */
  if (act_Current() == 0) {
    //printf("in no current Activity case...calling ChooseNew\n");
    act_ChooseNewCurrentActivity();
    act_yieldState = 0;
  }
  else if (act_yieldState == ys_ShouldYield) {
    /* Current activity may be stalled or dead; if so, don't stick it
     * back on the run list!
     */
    if ( act_Current()->state == act_Running ) {
      act_Current()->readyQ->doQuantaTimeout(act_Current()->readyQ,
                                             act_Current());
#ifdef ACTIVITYDEBUG
      if ( act_Current()->IsUser() )
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
    printf("schedloop: curActivity 0x%08x proc 0x%08x fc 0x%x\n",
		   act_Current(), proc_Current(),
		   proc_Current() ? proc_Current()->faultCode : 0);
#endif

    assert (act_Current());
    
    /* If activity cannot be successfully prepared, it cannot (ever) run,
     * and should be returned to the free activity list.  Do this even if
     * we are rescheduling, since we want the activity entry back promptly
     * and it doesn't take that long to test.
     */
    if ( act_Current() && act_Prepare(act_Current()) == false ) {
      assert( act_IsUser(act_Current()) );
      

      /* We shouldn't be having this happen YET */
      fatal("Current activity no longer runnable\n");

      act_DeleteActivity(act_Current());
      assert (act_Current() == 0);
      act_ChooseNewCurrentActivity();
    }

    assert (act_Current());

    /* Activity might have gone to sleep as a result of context being prepared. */
    /* But in that case, wouldn't we have gone to act_Yield, not here? CRL */
    if (act_Current()->state != act_Running)
      act_ChooseNewCurrentActivity();

    if (proc_Current()
	&& proc_Current()->processFlags & capros_Process_PF_FaultToProcessKeeper) {
      proc_InvokeProcessKeeper(proc_Current());

      /* Invoking the process keeper either stuck us on a sleep queue, in
       * which case we Yielded(), or migrated the current activity to a
       * new domain, in which case we will keep trying: */
      if (act_Current() == 0 || act_Current()->state != act_Running)
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

  } while (! act_IsRunnable(act_Current()));

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Bottom DoReschedule()");
#endif

  assert (act_Current());
  assert (proc_Current());

  assert (act_Current()->readyQ == act_CurContext()->readyQ);
  assert(local_irq_disabled());

  act_yieldState = 0;		/* until proven otherwise */

  if (act_Current()->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve * r = (Reserve *)act_Current()->readyQ->other;

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

  assert(local_irq_disabled());
}

void
HandleDeferredWork(void)
{
  if (timerWork) {
    timerWork = false;

    // FIXME: check that all variables are properly locked by irq
    sysT_WakeupAt();
  }
  act_DoReschedule();
}

// May Yield.
void
ExitTheKernel(void)
{
  assert(local_irq_disabled());

  UpdateTLB();

  if (act_yieldState) {
    HandleDeferredWork();
  }
  else if (! act_IsRunnable(act_Current())) {
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

  // Call architecture-dependent C code for resuming a process.
  ExitTheKernel_MD(thisPtr);

  // Call architecture-dependent assembly code for resuming a process.
  resume_process(thisPtr);	// does not return
}

// May be called from an interrupt.
void 
sq_WakeAll(StallQueue* q, bool b/* verbose*/)
{
  Activity *t;

  irqFlags_t flags = local_irq_save();

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

  local_irq_restore(flags);
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

  assert(thisPtr == act_Current());
  
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
  
    Process * proc = thisPtr->context;
    if (! proc) {
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

      proc = thisPtr->processKey.u.gk.pContext;
      if (! proc)
	return false;
    
      act_SetContextCurrent(thisPtr, proc);
      proc_SetActivity(proc, thisPtr);
    }

#ifdef ACTIVITYDEBUG
    printf("Preparing Process 0x%08x..\n", proc);
#endif

    proc_Prepare(proc);

    if ( proc_IsRunnable(proc) ) {
#if CONV
      assert (thisPtr->priority == proc->priority);
#else
      assert (thisPtr->readyQ = proc->readyQ);
#endif
#ifdef DBG_WILD_PTR
      if (dbg_wild_ptr && 0)
	check_Consistency("After ThrdPrepare()");
#endif
      return true;
    }

    /* The Process could not be prepared. */
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
  assert(allocatedActivity == 0);

#ifndef NDEBUG
  act_ValidateActivity(act_Current());
#endif

  inv_Cleanup(&inv);
  
  act_ForceResched();

  assertex(act_CurContext(), act_CurContext()->runState == RS_Running);

  /* At this time, the activity rescheduler logic thinks it must run
     disabled. I am not convinced that it really needs to, but it is
     simpler not to argue with it here. */
  /* At this point we may or may not have done irq_ENABLE().
     Either way, irq_DISABLE() ensures interrupts are disabled. */
  irq_DISABLE();
  
  ExitTheKernel();
}
