#ifndef __ACTIVITY_H__
#define __ACTIVITY_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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

/* CapROS activity list management.  For every process that is runnable
 * or stalled, an entry exists in the activity table list.  When one
 * process calls or returns to another, we migrate the Activity from the
 * invoker to the invokee (to save allocation and deallocation).
 * When a process issues a SEND, it allocates a new Activity.
 */

#include <eros/Link.h>
#include <kerninc/Key.h>
#include <kerninc/Process.h>
#include <kerninc/SysTimer.h>
#include <kerninc/ReadyQueue.h>

/* Values for Activity.state:

act_Free: free, on the FreeActivityList.
  Exception: the Activity, if any, that the variable allocatedActivity
    points to, may have state == act_Free, but it really isn't free;
    it is reserved for use by the current invocation,
    and it isn't on the FreeActivityList.

act_Ready: on a ReadyQueue.
  If it has a Process (without hz_DomRoot):
    If actHazard has actHaz_None, the Process's runState is RS_Running.
    If actHazard has actHaz_WakeOK or actHaz_WakeRestart,
      the Process's runState is RS_Waiting.

act_Running: active on a processor, not on any queue.
  The activity is act_curActivity.
  If it has a Process (without hz_DomRoot), the Process's runState is
    the same as in the act_Ready state.

act_Stall: blocked on an event, on a StallQueue.
  When the event occurs, the process will be restarted.
  If it has a Process (without hz_DomRoot), the Process's runState is
    the same as in the act_Ready state.

act_Sleeping: blocked on a timer, on the SleepQueue.
  actHazard is actHaz_None.
  If it has a Process (without hz_DomRoot),
    the Process's runState is RS_Waiting.

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
because otherwise it is difficult to implement "wait for n minutes"
in the kernel; if you restart the invoker after n minutes,
it will simply wait for another n minutes.
*/
enum {
  act_Free,
  act_Ready,
  act_Running,
  act_Stall,
  act_Sleeping,

  act_NUM_STATES
};

/* Values for actHazard:

 actHaz_None: No special processing required.

 actHaz_WakeOK: Before running the process, we must send it a message
	with return code RC_OK.

 actHaz_WakeRestart: Before running the process, we must send it a message
	with return code RC_capros_key_Restart.

 actHaz_WakeResume: If u.callCount is stale, this Activity should be deleted.
	Otherwise, Before running the process, we must send it a message
	with return code RC_capros_key_Restart.
 */
enum {actHaz_None,
      actHaz_WakeOK,
      actHaz_WakeRestart,
      actHaz_WakeResume,
      actHaz_END};
 
/* The activity structure captures the portion of a process's state
 * that MUST remain in core while the process is logically running or
 * blocked.  An idle process is free to go out of core.
 */

typedef struct Activity Activity;
struct Activity {
  Link q_link;

  union {
    uint64_t wakeTime; /* if state == act_Sleeping, when to wake up, in ticks */

    ObCount callCount;	// used if actHazard == actHaz_WakeResume
    /* Note: if actHazard != actHaz_WakeResume, we do not need the callCount.
    When a Resume key is rescinded (an act which invalidates any
    Activity in the Process), the Process is in memory and its curActivity
    field points to any Activity. 
    When a Process is rescinded, either the Process is in memory,
    or we explicitly look for any Activity for the Process and delete it.
    (We have to do the latter to avoid leaving stale Activitys allocated.) */
  } u;

  union {
    OID oid;		// used if ! hasProcess

    Process * actProcess;	// used if hasProcess
  } id;

  StallQueue *lastq;

  uint8_t state;

  uint8_t actHazard;

  bool hasProcess;	// valid if state != act_Free

  struct ReadyQueue * readyQ;
	/*
        If state == act_Free, readyQ has NULL.
        If there is an associated Process,
	  readyQ contains a copy of Process.readyQ.
        If there is no associated Process,
	  readyQ contains the last known readyQ of the Process.
          If no Process was ever known,
          readyQ contains dispatchQueues[capros_SchedC_Priority_Max],
          which is good enough to get the Activity scheduled. */
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

void act_ValidateActivity(Activity * thisPtr);
void ValidateAllActivitys(void);

bool act_GetOID(Activity * act, OID * oid);

Activity * kact_InitKernActivity(const char * name, 
                             struct ReadyQueue *rq,
			     void (*pc)(void), 
			     uint32_t *StackBottom, uint32_t *StackTop);
Activity * act_FindByOid(Node * node);
void act_UnloadProcess(Activity * act);

INLINE bool
act_HasProcess(Activity * act)
{
  return act->hasProcess;
}

INLINE Process *
act_GetProcess(Activity * act)
{
  assert(act_HasProcess(act));
  return act->id.actProcess;
}

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
act_SetProcess(Activity * act, Process * proc) 
{
  assert(proc);
  assert(act->actHazard != actHaz_WakeResume);
  act->hasProcess = true;
  act->id.actProcess = proc;
  proc_SetActivity(proc, act);
}

INLINE bool 
act_IsUser(Activity* thisPtr)
{
  return (! act_HasProcess(thisPtr) || proc_IsUser(act_GetProcess(thisPtr)) );
}
  
void act_Wakeup(Activity* thisPtr);

void act_ForceResched(void);

void ExitTheKernel(void) NORETURN;
void ExitTheKernel_MD(Process *);		// architecture-dependent
void resume_process(Process *) NORETURN;	// architecture-dependent

const char* act_Name(Activity* thisPtr);

void act_DeleteActivity(Activity* t);
void act_DeleteCurrent(void);
void act_AssignTo(Activity * act, Process * proc);
void MigrateAllocatedActivity(Process * proc);

INLINE void 
act_AssignToRunnable(Activity * act, Process * proc)
{
  act_AssignTo(act, proc);

  assert(proc_IsRunnable(proc));

  /* FIX: Check for preemption! */
  act->readyQ = proc->readyQ;
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

Activity * act_AllocActivity();
void act_FreeActivity(Activity * act);
void StartActivity(OID oid, ObCount count, uint8_t haz);
void act_AllocActivityTable();

#endif /* __ACTIVITY_H__ */
