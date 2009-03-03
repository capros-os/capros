/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, Strawberry Development Group.
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
#include <kerninc/ObjectSource.h>
#include <arch-kerninc/PTE.h>
#include <kerninc/Ckpt.h>
#include <kerninc/Key-inline.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Range.h>
#include <eros/fls.h>

/* #define THREADDEBUG */
/*#define RESERVE_DEBUG*/

static void act_Enqueue(Activity * t, StallQueue * q);
static bool PrepareCurrentActivity(void);

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

static DEFQUEUE(freeActivityList);
unsigned int numFreeActivities = 0;
uint32_t act_RunQueueMap = 0;

INLINE bool 
act_IsRunnable(Activity * thisPtr)
{
  return (act_HasProcess(thisPtr)
	  && proc_IsRunnable(act_GetProcess(thisPtr))
	  && ! (act_GetProcess(thisPtr)->processFlags
                & capros_Process_PF_FaultToProcessKeeper) );
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
  Activity * act = act_AllocActivity();

  /*t->priority = prio;*/
  Process * p = kproc_Init(name, prio, rq, pc,
                           StackBottom, StackTop);
  act_SetProcess(act, p);

  act->readyQ = rq;

  return act;
}

Activity *
act_AllocActivity(void) 
{
  if (sq_IsEmpty(&freeActivityList))
    fatal("Activitys exhausted\n");
    // FIXME: is it feasible to wait for a free Activity?

  Activity * act = act_DequeueNext(&freeActivityList);
  assert(numFreeActivities);
  numFreeActivities--;

  act->actHazard = actHaz_None;

  return act;
}

void 
act_AssignTo(Activity * act, Process * proc)
{
  act_SetProcess(act, proc);
  if (act == act_Current())
    act_SetCurProcess(proc);
}

void
StartActivity(OID oid, ObCount count, uint8_t haz)
{
  Activity * act = act_AllocActivity();

  act->hasProcess = false;
  act->id.oid = oid;
  act->u.callCount = count;	// used only if haz == actHaz_WakeResume
  act->actHazard = haz;

  /* The process prepare logic will appropriately adjust this priority
     if it is wrong -- this guess only has to be good enough to get
     the activity scheduled. */
  act->readyQ = dispatchQueues[pr_High];
 
  act_Wakeup(act);
}

void
act_AllocActivityTable() 
{
  int i = 0;

  act_ActivityTable = MALLOC(Activity, KTUNE_NACTIVITY);

  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    Activity * act = &act_ActivityTable[i];

    link_Init(&act->q_link);
    act->hasProcess = false;
    act->readyQ = dispatchQueues[pr_Never];	// just for safety
    act_DeleteActivity(act);
  }

  printf("Allocated User Activitys: 0x%x at 0x%08x\n",
	 sizeof(Activity[KTUNE_NACTIVITY]), act_ActivityTable);
}

void
act_DeleteActivity(Activity *t)
{
  // printf("act_DeleteActivity(%#x)\n", t);

  // No Process should point to this Activity:
  assert(! act_HasProcess(t)
         || ! act_GetProcess(t)->curActivity);

  // This Activity should not be queued:
  assert(link_isSingleton(&t->q_link));

  /* dprintf(true, "Deleting activity 0x%08x\n", t); */
  /* not hazarded because activity key */
  t->state = act_Free;
  if (act_Current() == t) {
#if 0
    printf("curactivity 0x%08x is deleted\n", t);
#endif
    act_curActivity = NULL;
    proc_curProcess = NULL;
    /* act_curActivity == 0 ==> deferredWork & dw_reschedule */
    act_ForceResched();
  }

  if (t->readyQ->mask & (1u<<pr_Reserve)) {
    Reserve *r = (Reserve *)t->readyQ->other;

    printf("removing reserve entry %d", r->index);
    printf(" . entry total time used in ms = %U\n",
           mach_TicksToMilliseconds(r->totalTimeAcc));
    res_SetInactive(r->index);
  }
  t->readyQ = dispatchQueues[pr_Never];	// just for safety

  act_Enqueue(t, &freeActivityList);
  numFreeActivities++;
}

Activity *
act_FindByOid(Node * node)
{
  // FIXME: this is an O(n) algorithm.
  OID oid = node_ToObj(node)->oid;
  int i;
  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    Activity * act = &act_ActivityTable[i];
    if (act->state != act_Free) {
      if (! act_HasProcess(act)) {
        if (act->id.oid == oid) {	// Found the oid
          if (act->actHazard == actHaz_WakeResume) {
            if (act->u.callCount != node->callCount) {
              // This activity's call count is stale.
              assert(act != act_Current());
              act_Dequeue(act);
              act_DeleteActivity(act);
              // There can be no other Activity with the same OID.
              return NULL;
            } else {
              act->actHazard = actHaz_WakeRestart;
              return act;
            }
          } else {
            return act;
          }
        }
      }
    }
  }
  return NULL;
}

void
act_UnloadProcess(Activity * act)
{
  Process * proc = act_GetProcess(act);
  assert(proc->curActivity == act);
  assert(act->actHazard == actHaz_None);

  act->id.oid = node_ToObj(proc->procRoot)->oid;
  act->hasProcess = false;
  proc_Deactivate(proc);
  if (act == act_Current()) {
    act_SetCurProcess(NULL);
  }
}

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

// After calling act_SleepOn, the caller must call act_Yield().
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

/* Migrate allocatedActivity to proc. */
void
MigrateAllocatedActivity(Process * proc)
{
  Activity * act = allocatedActivity;	// local copy
  if (act->state == act_Running) {
    // allocatedActivity came from the invoker.
    assert(act == act_Current());
    // The invocation might have caused the invoker to be unloaded.
    if (act_HasProcess(act)) {
      proc_Deactivate(act_GetProcess(act));
    }
    act_AssignToRunnable(act, proc);
  } else {
    // allocatedActivity is newly allocated.
    assert(act->state == act_Free);
    act_AssignToRunnable(act, proc);
    act_Wakeup(act);
  }
}

void
act_DeleteCurrent(void)
{
  Activity * act = act_Current();
  if (act_HasProcess(act)) {
    assert(! proc_StateHasActivity(act_GetProcess(act)));
    proc_Deactivate(act_GetProcess(act));
  }
  act_SetCurProcess(NULL);
  act_DeleteActivity(act);
}

void 
act_ChooseNewCurrentActivity(void)
{    
  int runQueueNdx;
  Reserve *res = 0;

  assert(local_irq_disabled());

  //printf("starting ChooseNew()...\n");

  for ( ;; ) {
    /* idle activitys should always be ready */
    assert(act_RunQueueMap);
    runQueueNdx = fls32(act_RunQueueMap) - 1;

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
  Activity * act = (Activity *)(dispatchQueues[runQueueNdx]->queue.q_head.next);

  // Set it running.
  /* Must be under irq_DISABLE */
  link_Unlink(&act->q_link);
  act_curActivity = act;
  if (act_HasProcess(act))
    act_SetCurProcess(act_GetProcess(act));
  else
    proc_curProcess = NULL;
  act->state = act_Running;

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

// May Yield.
static void 
act_EnsureRunnable(void)
{
  assert(local_irq_disabled());

  for (; ;
       /* Clear all previous user pins. 
          Otherwise this loop could pin an unbounded number of objects. */
       objH_BeginTransaction(),
       act_ChooseNewCurrentActivity() ) {
    
    assert(act_Current()->state == act_Running);
    
    if (! PrepareCurrentActivity()) {
      assert(act_IsUser(act_Current()));
      /* If the activity cannot be successfully prepared, it cannot (ever) run,
       * and should be returned to the free activity list. */

      /* We shouldn't be having this happen YET */
      fatal("Current activity no longer runnable\n");

      assert(link_isSingleton(&act_Current()->q_link));	// because act_Running
      act_DeleteActivity(act_Current());
      continue;
    }

    assert(act_Current()->state == act_Running);

    if (proc_Current()
	&& proc_Current()->processFlags
           & capros_Process_PF_FaultToProcessKeeper ) {
      proc_InvokeProcessKeeper(proc_Current());

      /* Invoking the process keeper either stuck us on a sleep queue, in
       * which case we Yielded(), or migrated the current activity to a
       * new domain, in which case we will keep trying: */
      if (act_Current() == 0 || act_Current()->state != act_Running) {
        continue;
      }
    }
    break;	// act_Current() is now Runnable
  }

  assert (act_Current());
  assert (proc_Current());

  assert (act_Current()->readyQ == act_CurContext()->readyQ);
  assert(local_irq_disabled());

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

/* Set the global variable that will force rescheduling
 * on the next return to user mode. */
void
act_ForceResched(void)
{
  irqFlags_t flags = local_irq_save();
  deferredWork |= dw_reschedule;
  local_irq_restore(flags);
}

void
HandleDeferredWork(void)
{
  if (deferredWork & dw_timer) {
    deferredWork &= ~dw_timer;

    sysT_WakeupAt();
  }

  if (deferredWork & dw_reschedule) {
    deferredWork &= ~dw_reschedule;

    if (act_Current() && act_Current()->state == act_Running)
      // Put current activity back on the run list.
      act_Current()->readyQ->doQuantaTimeout(act_Current()->readyQ,
                                             act_Current());
    act_ChooseNewCurrentActivity();
  }

  act_EnsureRunnable();
}

// May Yield.
void
ExitTheKernel(void)
{
#ifdef OPTION_DDB
  extern bool ddb_activity_uqueue_debug;
  if (ddb_activity_uqueue_debug)
    dprintf(true, "ExitTheKernel\n");
#endif

  assert(local_irq_disabled());

  UpdateTLB();

  if (deferredWork) {
    HandleDeferredWork();
  }
  else if (! act_IsRunnable(act_Current())) {
    act_EnsureRunnable();
  }
  
  assert(act_Current());
  assert(act_CurContext()->runState == RS_Running);

  Process * thisPtr = act_CurContext();

#ifndef NDEBUG
  if (dbg_inttrap)
    dprintf(true, "Resuming proc 0x%08x\n", thisPtr);

  if ( thisPtr->curActivity != act_Current() )
    fatal("Context 0x%08x (%d) not for current activity 0x%08x (%d)\n",
	       thisPtr, thisPtr - proc_ContextCache,
		  act_Current(),
		  act_Current() - act_ActivityTable);
#endif

  // Call architecture-dependent C code for resuming a process.
  ExitTheKernel_MD(thisPtr);

  // Call architecture-dependent assembly code for resuming a process.
  resume_process(thisPtr);	// does not return
}

// May be called from an interrupt.
void 
sq_WakeAll(StallQueue * q)
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

/* If the Activity has a user process, this procedure sets
*oid and *count to its OID and ObCount, and returns true.
Otherwise it returns false. */
bool
act_GetOID(Activity * act, OID * oid)
{
  if (act_HasProcess(act)) {	// process info is in the Process structure
    Process * proc = act_GetProcess(act);
    if (proc_IsKernel(proc))
      return false;
    *oid = node_ToObj(proc->procRoot)->oid;
  } else {
    *oid = act->id.oid;
  }
  return true;
}

#ifndef NDEBUG
void
ValidateAllActivitys(void)
{
  int i, j;

  for (i = 0; i < KTUNE_NACTIVITY; i++) {
    Activity * act = &act_ActivityTable[i];
    if (act->state != act_Free) {
      act_ValidateActivity(act);
      // There should not be more than one Activity per user process:
      // Warning: this is O(n squared).
      OID procOid;
      if (act_GetOID(act, &procOid)) {
        for (j = 0; j < i; j++) {
          Activity * otherAct = &act_ActivityTable[j];
          if (otherAct->state != act_Free) {
            OID otherProcOid;
            if (act_GetOID(otherAct, &otherProcOid)) {
              assert(procOid != otherProcOid);
            }
          }
        }
      }
    }
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

  switch (thisPtr->state) {
  default:
    assert(false);

  case act_Free:
    assert(! act_HasProcess(thisPtr));
    break;

  case act_Running:
    assert(thisPtr == act_Current());
  case act_Ready:
  case act_Stall:
    if (act_HasProcess(thisPtr)
        && ! (act_GetProcess(thisPtr)->hazards & hz_DomRoot)) {
      Process * proc = act_GetProcess(thisPtr);
      switch (thisPtr->actHazard) {
      default:
      case actHaz_WakeResume:
        assert(false);

      case actHaz_None:
        assert(proc->runState == RS_Running
               || (thisPtr->state == act_Running
                   && proc->runState == RS_WaitingU) );
		// RS_WaitingU and act_Running can happen during an invocation
        break;

      case actHaz_WakeOK:
      case actHaz_WakeRestart:
        assert(proc->runState == RS_WaitingK);
        break;
      }
    }
    break;

  case act_Sleeping:
    if (act_HasProcess(thisPtr)
        && ! (act_GetProcess(thisPtr)->hazards & hz_DomRoot)) {
      assert(act_GetProcess(thisPtr)->runState == RS_WaitingK);
    }
  }
}


#endif

const char* 
act_Name(Activity* thisPtr)
{
  static char userActivityName[] = "userXXX\0";
  uint32_t ndx = 0;

  if (! act_IsUser(thisPtr))
    return proc_Name(act_GetProcess(thisPtr));	// kernel process name
    
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

/* May Yield. */
static bool 
PrepareCurrentActivity(void)
{
  Activity * thisPtr = act_Current();

  /* Try to prepare this activity to run: */

  Process * proc;
  if (! act_HasProcess(thisPtr)) {
    ObjectLocator objLoc;

    if (! CheckObjectType(thisPtr->id.oid, &objLoc, capros_Range_otNode)) {
      assert(thisPtr->actHazard == actHaz_WakeResume);
  noProc:
      // Worth noting:
      printf("Rescinded activity %#x!\n", thisPtr);
      return false;
    }

    if (thisPtr->actHazard == actHaz_WakeResume) {
      // We have a call count to be checked.
      ObCount countToCompare = GetObjectCount(thisPtr->id.oid, &objLoc, true);
      if (countToCompare != thisPtr->u.callCount)
        goto noProc;	// call count is stale
      thisPtr->actHazard = actHaz_WakeRestart;
    }

    ObjectHeader * pObj = GetObject(thisPtr->id.oid, &objLoc);
    Node * pNode = objH_ToNode(pObj);
    assert(objC_ValidNodePtr(pNode));
    proc = node_GetProcess(pNode);

    act_AssignTo(thisPtr, proc);
  } else {
    proc = act_GetProcess(thisPtr);
  }

  assert(act_Current()->state == act_Running);

#ifdef ACTIVITYDEBUG
  printf("Preparing Process 0x%08x..\n", proc);
#endif

  if (proc_IsNotRunnable(proc)) {
    proc_DoPrepare(proc);
    // If proc_DoPrepare was unable to make it runnable:
    if (proc_IsNotRunnable(proc)) {
      printf("Process %#x is malformed.\n", proc);
      return false;
    }
  }

  assert(proc_IsRunnable(proc));

  assert(act_Current()->state == act_Running);

  if (thisPtr->actHazard != actHaz_None) {
    switch (thisPtr->actHazard) {
    default:
    case actHaz_WakeResume:
      assert(false);

    case actHaz_WakeOK:
      sysT_procWake(proc, RC_OK);
      thisPtr->actHazard = actHaz_None;
      break;

    case actHaz_WakeRestart:
      sysT_procWake(proc, RC_capros_key_Restart);
      thisPtr->actHazard = actHaz_None;
      break;
    }
  }

  assert(act_Current()->state == act_Running);
  assert(thisPtr->readyQ == proc->readyQ);

  return true;
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
  assert(! InvocationCommitted);
  assert(allocatedActivity == NULL);

#ifndef NDEBUG
  act_ValidateActivity(act_Current());
#endif

  inv_Cleanup(&inv);
  
  act_ForceResched();

  /* At this time, the activity rescheduler logic thinks it must run
     disabled. I am not convinced that it really needs to, but it is
     simpler not to argue with it here. */
  /* At this point we may or may not have done irq_ENABLE().
     Either way, irq_DISABLE() ensures interrupts are disabled. */
  irq_DISABLE();
  
  ExitTheKernel();
}

#ifdef OPTION_DDB
void
db_print_readyQueue(ReadyQueue * rq)
{
  printf("mask=%#x other=%#x wake=%#x, to=%#x\n",
    rq->mask, rq->other, rq->doWakeup, rq->doQuantaTimeout);
  if (!sq_IsEmpty(&rq->queue)) {
    printf("Activitys on queue: ");
    Link * lk;
    for (lk = rq->queue.q_head.next; lk != &rq->queue.q_head; lk = lk->next) {
      printf("%#x ", lk);
    }
    printf("\n");
  }
}

void
db_show_readylist(void)
{
  int runQueueNdx;

  printf("RunQueueMap=%#.8x\n", act_RunQueueMap);
  for (runQueueNdx = pr_High; runQueueNdx >= 0; runQueueNdx--) {
    if (runQueueNdx == pr_Reserve) {
      int i;
      for (i = 0; i < 32; i++) {
        Reserve * res = &res_ReserveTable[i];
        printf("Reserve[%d]: ", i);
        db_print_readyQueue(&res->readyQ);
      }
    } else {
      printf("dispatchQueues[%d]: ", runQueueNdx);
      db_print_readyQueue(dispatchQueues[runQueueNdx]);
    }
  }
}
#endif

