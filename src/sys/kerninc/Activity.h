#ifndef __ACTIVITY_H__
#define __ACTIVITY_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* EROS activity list management.  For every process that is runnable
 * or stalled, an entry exists in the activity table list.  When one
 * process invokes another, it hands its entry to the invoked process.
 * When a process issues a SEND, it allocates a new activity.
 * Activities will block if their process performs a SEND and no free
 * Activity is available.  When a process returns to the kernel, or
 * when it becomes malformed in certain ways, its occupying Activity
 * is returned to the Activity free list.
 */

#include <eros/Link.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/SysTimer.h>
#include <kerninc/ReadyQueue.h>

/* Values for Activity.state:

act_Free: free, on the FreeActivityList.

act_Ready: on a ReadyQueue.
  If it has a Process, the Process's runState is RS_Running.

act_Running: active on a processor, not on any queue.
  The activity is act_curActivity.
  If it has a Process, the Process's runState is RS_Running.

act_Stall: blocked on an event, on a StallQueue.
  If it has a Process, the Process's runState is RS_Running.
  When the event occurs, the process will be restarted.

act_Sleeping: blocked on a timer, on the SleepQueue.
  If it has a Process, the Process's runState is RS_Waiting.

act_Stall is used to block before an operation is committed
(therefore the operation can be restarted when the block is removed).
act_Sleeping is used to block after an operation is commited.
It is logically equivalent to the kernel holding a Resume key to the
process, and when the event occurs, using that Resume key to return
to the process.
The difference is seen when an invoker does a SEND to a key such as Sleep
and passes a Resume key to another process (the returnee aka invokee)
to be returned to when the operation completes.
If the invoker blocks, we use act_Stall. If the returnee is blocked,
we use act_Sleep.

act_Stall is used for short waits (such as for I/O) or for some closely
held keys (migrator tool, I/O interrupt).
act_Sleep is used for the Sleep key, where the wait could be long.
act_Sleep gives a more accurate implementation (SEND to Sleep works the same
whether Sleep is implemented in or out of the kernel),
but is more difficult to implement in the kernel,
because it requires saving information about when and how to return.
In particular, on a checkpoint we do not save timer information.
It is particularly important to implement the Sleep key this way,
because otherwise it is difficult to implement "wait for n minutes";
if you restart the invoker after n minutes, it will simply wait for
another n minutes.
*/
enum {
  act_Free,
  act_Ready,
  act_Running,
  act_Stall,
  act_Sleeping,

  act_NUM_STATES
};

// Values for actHazard:
enum {actHaz_None,
      actHaz_WakeOK,
      actHaz_WakeRestart,
      actHaz_END};
 
/* The activity structure captures the portion of a process's state
 * that MUST remain in core while the process is logically running or
 * blocked.  An idle process is free to go out of core.
 */

typedef struct Activity Activity;
struct Activity {
  Link q_link;

  Process *context;	/* zero if no context cache entry. */

  StallQueue *lastq;

  uint8_t state;

  uint8_t actHazard;

  Key processKey;
	/* keyBits_GetType(&processKey) is always KKT_Process or KKT_Void. */
	/* If Activity.context is non-NULL, it identifies the
	associated process, and processKey is not meaningful.
	Otherwise, processKey identifies the associated process or is void. */

  uint32_t unused;	/* keep until we fix assembler ofsets */

  struct ReadyQueue *readyQ;	/* readyQ info for this activity */  

  /* Support for the per-activity timer structure.  The only user
   * activities that will use this are the ones invoking primary system
   * timer keys.
   */

  uint64_t wakeTime; /* if asleep, when to wake up, in ticks */
} ;

extern const char *act_stateNames[act_NUM_STATES]; 

/* Bits in deferredWork: */
#define dw_reschedule 0x1
#define dw_timer      0x2
/* deferredWork must only be used with interrupts disabled. */
extern unsigned int deferredWork;

extern Activity * act_curActivity;
extern Process * proc_curProcess;

extern Activity *act_ActivityTable;
extern Activity * allocatedActivity;

extern unsigned int numFreeActivities;

/* Prototypes for former member functions of Activity */

Activity * kact_InitKernActivity(const char * name, 
			     Priority prio,
                             struct ReadyQueue *rq,
			     void (*pc)(void), 
			     uint32_t *StackBottom, uint32_t *StackTop);

INLINE Activity* 
act_Current() 
{
  return act_curActivity;
}

#include <kerninc/Process-inline.h>

void act_SleepUntilTick(Activity* thisPtr, uint64_t ms);

void act_Dequeue(Activity *t);
Activity *act_DequeueNext(StallQueue *q);
void act_SleepOn(StallQueue *);

/* This relies on the fact that the context will overwrite our process
 * key slot if it is unloaded!
 */
INLINE void 
act_SetContextNotCurrent(Activity * thisPtr, Process * ctxt) 
{
  assert(thisPtr != act_Current());
  thisPtr->context = ctxt;
}

INLINE void
act_SetCurProcess(Process * proc)
{
#if defined(OPTION_DDB)
  extern bool ddb_uyield_debug;
  if (ddb_uyield_debug)
    dprintf(true, "CurProc changes from %#x to %#x\n", proc_curProcess, proc); 
#endif

  proc_curProcess = proc; 
}

INLINE void 
act_SetContextCurrent(Activity * thisPtr, Process * ctxt) 
{
  assert(thisPtr == act_Current());
  thisPtr->context = ctxt;
  act_SetCurProcess(ctxt);
}

INLINE void 
act_SetContext(Activity * thisPtr, Process * ctxt) 
{
  thisPtr->context = ctxt;
  if (thisPtr == act_Current())
    act_SetCurProcess(ctxt);
}

/* Must be under irq_DISABLE */
INLINE void 
act_SetRunning(Activity* thisPtr)
{
  link_Unlink(&thisPtr->q_link);
  act_curActivity = thisPtr;
  act_SetCurProcess(thisPtr->context);
  thisPtr->state = act_Running;
}

INLINE bool 
act_IsUser(Activity* thisPtr)
{
  /* hack alert! */
  return ( thisPtr->context == 0 || proc_IsUser(thisPtr->context) );
}
  
void act_Wakeup(Activity* thisPtr);

void act_DoReschedule();
void act_ForceResched(void);

void ExitTheKernel(void) NORETURN;
void ExitTheKernel_MD(Process *);		// architecture-dependent
void resume_process(Process *) NORETURN;	// architecture-dependent

#ifndef NDEBUG
void act_ValidateActivity(Activity* thisPtr);
#endif

const char* act_Name(Activity* thisPtr);

void act_DeleteActivity(Activity* t);
void act_DeleteCurrent(void);

/*
 * It proves that the only activity migrations that occur in EROS are to
 * processs that are runnable.  If a process is runnable, we know that
 * it has a proper schedule key.  We can therefore assume that the
 * reserve slot of the destination context is populated, and we can
 * simply pick it up and go with it.
 */
INLINE void 
act_AssignTo(Activity * thisPtr, Process * dc)
{
  assert(dc);

  act_SetContext(thisPtr, dc);
  proc_SetActivity(dc, thisPtr);

  /* FIX: Check for preemption! */
  thisPtr->readyQ = dc->readyQ;
}

/* Called by the activity when it wishes to yield the processor: */
INLINE void act_Yield(void) NORETURN;
INLINE void
act_Yield(void)
{
#if defined(OPTION_DDB)
  {
    extern bool ddb_uyield_debug;
    if ( ddb_uyield_debug )
      dprintf(true, "Activity 0x%08x yields\n", act_Current()); 
  }
#endif

  /* mach_Yield clears the stack (!) and goes to act_HandleYieldEntry. */
  extern void mach_Yield(void) NORETURN;
  mach_Yield();
}

void act_HandleYieldEntry(void) NORETURN;

#ifndef NDEBUG
Activity * act_ValidActivityKey(const Key * pKey);
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
void StartActivity(OID oid, ObCount count, uint8_t haz);
void act_AllocActivityTable();

#endif /* __ACTIVITY_H__ */
