#ifndef __ACTIVITY_H__
#define __ACTIVITY_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* EROS activity list management.  For every process that is runnable
 * or stalled, an entry exists in the activity table list.  When one
 * process invokes another, it hands its entry to the invoked process.
 * When a process issues a SEND, it allocates a new activity.
 * Activities will block if their process performs a SEND and no free
 * Activity is available.  When a process returns to the kernel, or
 * when it becomes malformed in certain ways, its occupying Activity
 * is returned to the Activity free list.
 */

#include <kerninc/Link.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/SysTimer.h>
#include <kerninc/ReadyQueue.h>
#include <eros/ProcessState.h>

struct CpuReserve;

/* Prototypes for former member functions of Activity */
void act_DoReschedule();

enum State {
  act_Free,			/* free activity */
  act_Ready,			/* can be scheduled */
  act_Running,			/* active on a processor */
  act_Stall,			/* blocked on an event, not expecting
				   result(s) */

  act_NUM_STATES,
};
 
enum {
  ys_ShouldYield = 0x1
};

#define AF_RETRYLIK 0x1u

/* The activity structure captures the portion of a process's state
 * that MUST remain in core while the process is logically running or
 * blocked.  An idle process is free to go out of core.
 *
 * When one process invokes another, it's activity table slot is
 * transferred, but it's VCPU entry is not.
 */

typedef struct Activity Activity;
struct Activity {
  Link q_link;

  Process *context;	/* zero if no context cache entry. */

  StallQueue *lastq;

  uint8_t state;
  uint8_t flags;

  Key processKey;		/* type==KtProcess */

  
  /* The wakeInfo field is a bit perverse. I initially thought that it
   * ought to be considered a per-process register. Eventually I
   * realized that this was wrong. If the process is running, then the
   * definitive version of the WakeInfo field *must* reside in the
   * activity, because the context can be swapped out. The wakeInfo
   * field is altogether meaningless if the process is available or
   * waiting.
   *
   * The only time we need to consider the wakeInfo field, then, is
   * when we consider the implementation of the decongester. I'm
   * taking the architectural position that decongestion is a rare
   * concern, and that it is okay to allow nominally "blocked" activities
   * to retry when they are restarted after being decongested.
   *
   * The wakeInfo register is CLEARED on every successful invocation
   * in any case, except for the RETRY invocation, which sets it
   * explicitly.
   */

  uint32_t wakeInfo;


  struct ReadyQueue *readyQ;	/* readyQ info for this activity */  

  /* Support for the per-activity timer structure.  The only user
   * activities that will use this are the ones invoking primary system
   * timer keys.
   */

  uint64_t wakeTime; /* if asleep, when to wake up, in milliseconds */
  Activity *nextTimedActivity;
} ;

extern const char *act_stateNames[act_NUM_STATES]; 

extern uint32_t act_yieldState;
extern Activity* act_curActivity;

extern Activity *act_ActivityTable;

void act_InitActivity(Activity *thisPtr);

/* Prototypes for former member functions of Activity */

Activity * kact_InitKernActivity(const char * name, 
			     Priority prio,
                             struct ReadyQueue *rq,
			     void (*pc)(void), 
			     uint32_t *StackBottom, uint32_t *StackTop);

INLINE void 
act_Resume(Activity* thisPtr) 
{
#if 0
  printf("Resume activity 0x%08x\n", thisPtr);
#endif
  assert (thisPtr->context);
  proc_Resume(thisPtr->context);
}

INLINE Activity* 
act_Current() 
{
  return act_curActivity;
}

void act_WakeUpAtTick(Activity* thisPtr, uint64_t ms);

void act_Enqueue(Activity *t, StallQueue *);
void act_Dequeue(Activity *t);
Activity *act_DequeueNext(StallQueue *q);
void act_SleepOn(Activity* thisPtr, StallQueue*);

/* This relies on the fact that the context will overwrite our process
 * key slot if it is unloaded!
 */
INLINE void 
act_ZapContext(Activity *thisPtr) 
{
  thisPtr->context = 0;
}

/* Called by the activity when it wishes to yield the processor: */
INLINE void 
act_Yield(Activity* thisPtr) 
{
  __asm__ __volatile__ ("jmp LowYield");
}

INLINE void 
act_Preempt(Activity* thisPtr)
{
  act_yieldState |= ys_ShouldYield;
}

INLINE void 
act_SetCurActivity(Activity* thisPtr)
{
  act_curActivity = thisPtr;
}

INLINE Process* 
act_CurContext()	/* retrieves the current context */
{
  assert(act_curActivity->context);
  return act_curActivity->context;
}

INLINE bool 
act_IsUser(Activity* thisPtr)
{
  /* hack alert! */
  return ( thisPtr->context == 0 || proc_IsUser(thisPtr->context) );
}
  
INLINE bool 
act_IsKernel(Activity* thisPtr)
{
  /* hack alert! */
  return ( thisPtr->context && proc_IsKernel(thisPtr->context) );
}

/* If prepare returns false, the activity is dead and should be placed
 * on the free list.  This can happen if the process root has been
 * rescinded.
 */
bool act_Prepare(Activity* thisPtr);

void act_Wakeup(Activity* thisPtr);

void act_InvokeMyKeeper(Activity* thisPtr);

INLINE bool 
act_ShouldYield(Activity* thisPtr)
{
  return (act_yieldState & ys_ShouldYield);
}

INLINE void 
act_ForceResched(Activity* thisPtr)
{
  act_yieldState = ys_ShouldYield;
}


INLINE bool 
act_IsRunnable(Activity* thisPtr)
{
  return (thisPtr->context
	  && proc_IsRunnable(thisPtr->context)
	  && (thisPtr->context->processFlags & PF_Faulted) == 0);
}

INLINE void 
act_Reschedule(void) 
{
  //printf("in initial Reschedule()...%d\n", act_curActivity->readyQ->mask);
  if (act_curActivity && (act_yieldState != ys_ShouldYield)
      && act_IsRunnable(act_curActivity)) {
    //printf("not rescheduling...%d\n", act_curActivity->readyQ->mask);
    return;
  }
  //printf("doing actual Reschedule()...%d\n", act_curActivity->readyQ->mask);
  act_DoReschedule();
}


#ifndef NDEBUG
void act_ValidateActivity(Activity* thisPtr);
#endif

const char* act_Name(Activity* thisPtr);

void act_DeleteActivity(Activity* t);

/* This relies on the fact that the context will overwrite our process
 * key slot if it is unloaded!
 * 
 * It proves that the only activity migrations that occur in EROS are to
 * processs that are runnable.  If a process is runnable, we know that
 * it has a proper schedule key.  We can therefore assume that the
 * reserve slot of the destination context is populated, and we can
 * simply pick it up and go with it.
 */
INLINE void 
act_MigrateTo(Activity* thisPtr, Process *dc)
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
      //act_Preempt(0);
    }    
  }
  else {
    act_DeleteActivity(thisPtr); /* migrate to 0 context => kill curActivity */
  }
}

void act_HandleYieldEntry(Activity* thisPtr) NORETURN;

void act_WakeUpIn(Activity* thisPtr, uint64_t ms);

#ifndef NDEBUG
bool act_ValidActivityKey(Activity* thisPtr, const Key *pKey);
#endif

INLINE Activity* 
act_ContainingActivity(void *vp)
{
  uint32_t wvp = (uint32_t) vp;
  wvp -= (uint32_t) act_ActivityTable;
  wvp /= sizeof(Activity);
  return act_ActivityTable + wvp;
}

INLINE bool 
act_IsActivityKey(const Key *pKey)
{
  /* This isn't quite right, as it will return TRUE for any random
   * pointer in to the activity table, but that's good enough for all
   * the places that we use it.
   */
    
  if ( ((uint32_t) pKey >= (uint32_t) act_ActivityTable) &&
       ((uint32_t) pKey < (uint32_t) &act_ActivityTable[KTUNE_NACTIVITY]) ) {
    return true;
  }
  return false;
}

Activity * act_AllocActivity();
void act_AllocActivityTable();
void act_Unprepare(Activity * thisPtr);

#endif /* __ACTIVITY_H__ */
