/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* Driver for key invocation */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Debug.h>
#include <kerninc/Check.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Invocation.h>
#include <kerninc/IRQ.h>
#include <arch-kerninc/Process-inline.h>
#include <eros/Invoke.h>
#ifdef OPTION_DDB
#include <eros/StdKeyType.h>
#endif

#include <kerninc/Machine.h>
#include <kerninc/KernStats.h>
#if 0
#include <machine/PTE.hxx>
#endif

/* #define GATEDEBUG 5
 * #define KPRDEBUG
 */

/* #define TESTING_INVOCATION */

#ifdef TESTING_INVOCATION
const dbg_wild_ptr = 1;
#endif

// #define OLD_PC_UPDATE

#ifdef OPTION_DDB
uint32_t ddb_inv_flags = 0;
#define DDB_STOP(x) (ddb_inv_flags & DDB_INV_##x)
#else
#define DDB_STOP(x) 0
#endif

#define __EROS_PRIMARY_KEYDEF(x, isValid, bindTo) void bindTo##Key (Invocation* msg);
#include <eros/StdKeyType.h>

#if 0
static void
UnknownKey(Invocation* inv /*@ not null @*/)
{
  key_Print(inv->key);
  fatal("Invocation of unknown primary key type 0x%x\n",
        keyBits_GetType(inv->key));
}
#endif

void
UnimplementedKey(Invocation* inv /*@ not null @*/)
{
  fatal("Invocation of unimplemented primary key type 0x%x\n",
        keyBits_GetType(inv->key));
}

#ifndef OPTION_KBD
#define KeyboardKey VoidKey
#endif

#define FNDISPATCH
#define  KKT_TimePage 24
#define KKT_TimeOfDay 25
#ifdef FNDISPATCH
INLINE void 
proc_KeyDispatch(Invocation *pInv)
{
  switch(inv.invKeyType) {
#define __EROS_PRIMARY_KEYDEF(kn, isValid, bindTo) case KKT_##kn: bindTo##Key(pInv); break;
#include <eros/StdKeyType.h>
    default:
      UnimplementedKey(pInv);
      break;
  }
}

#else
const KeyHandler keyHandler[KKT_NUM_KEYTYPE] = {
#if 1
#define __EROS_PRIMARY_KEYDEF(x, isValid, bindTo) bindTo##Key,
#include <eros/StdKeyType.h>
#else
  StartKey,			/* KKT_Start */
  ResumeKey,			/* KKT_Resume */
  WrapperKey,			/* KKT_Wrapper */
  NodeKey,			/* KKT_Node */
  SegmentKey,			/* KKT_Segment */
  ProcessKey,			/* KKT_Process */
  PageKey,			/* KKT_Page */
  DeviceKey,			/* KKT_Device */
  NumberKey,			/* KKT_Number */
  UnimplementedKey,		/* KKT_Timer */
  SchedKey,			/* KKT_Sched */
  RangeKey,			/* KKT_Range */
  RangeKey,			/* KKT_PrimeRange */
  RangeKey,			/* KKT_PhysRange */
  KeyBitsKey,			/* KKT_KeyBits */
  DiscrimKey,			/* KKT_Discrim */
  UnimplementedKey,		/* KKT_Returner */
  ProcessToolKey,		/* KKT_ProcessTools */
  CheckpointKey,		/* KKT_CheckpointKey */
  VoidKey,			/* KKT_VoidKey */
  SleepKey,			/* KKT_SleepKey */
  ConsoleKey,			/* KKT_ConsoleKey */
  SchedCreatorKey,		/* KKT_SchedCreator */
  SysTraceKey,			/* KKT_SysTrace */
  DevicePrivsKey,		/* KKT_DeviceProvs */

#ifdef KKT_TimePageKey
  TimePageKey,			/* KKT_TimePage */
#else
  UnknownKey,			/* (unassigned) */
#endif

#ifdef KKT_TimeOfDayKey
  TimeOfDayKey,			/* KKT_TimeOfDay */
#else
  UnknownKey,			/* (unassigned) */
#endif

#ifdef OPTION_KBD
  KeyboardKey,			/* KKT_Keyboard */
#else
  UnknownKey,			/* (unassigned) */
#endif
#endif
};
#endif

#ifdef OPTION_KERN_TIMING_STATS
uint64_t inv_KeyHandlerCycles[PRIMARY_KEY_TYPES][IT_NUM_INVTYPES];
uint64_t inv_KeyHandlerCounts[PRIMARY_KEY_TYPES][IT_NUM_INVTYPES];

void 
inv_ZeroStats()
{
  for (int i = 0; i < PRIMARY_KEY_TYPES; i++) {
    inv_KeyHandlerCycles[i][IT_Return] = 0;
    inv_KeyHandlerCycles[i][IT_PReturn] = 0;
    inv_KeyHandlerCycles[i][IT_Call] = 0;
    inv_KeyHandlerCycles[i][IT_Send] = 0;
    inv_KeyHandlerCycles[i][IT_Post] = 0;
    inv_KeyHandlerCounts[i][IT_Return] = 0;
    inv_KeyHandlerCounts[i][IT_PReturn] = 0;
    inv_KeyHandlerCounts[i][IT_Call] = 0;
    inv_KeyHandlerCounts[i][IT_Send] = 0;
    inv_KeyHandlerCounts[i][IT_Post] = 0;
  }
}
#endif

void 
inv_BootInit()
{
#if defined(OPTION_KERN_TIMING_STATS)
  inv_ZeroStats();
#endif
}

/* KEY INVOCATION
 * 
 * Logic in Principle: Entry, Action, Exit
 * 
 * Entry: 
 * 
 *    1. Validate that the invocation is well-formed, and that the
 *       data argument if any is present and validly mapped.  
 * 
 *    2. Copy all of the information associated with the
 *       invocation to a place of safety against the possibility that
 *       entry and exit arguments overlap.
 * 
 *    3. Determine who we will be exiting to (the invokee) -- this is
 *       often the invoker.  It can also be the domain named by the
 *       invoked gate key.  It can also be the domain named by the key
 *       in slot 4 if this is a kernel key (YUCK).  It can also be
 *       NULL if the key in slot 4 proves not to be a resume key.
 * 
 *       Invoked      Invocation    Slot 4     Invokee
 *       Key Type     Type          Key Type
 * 
 *       Gate Key     ANY           *          Named by gate key
 *       Kernel Key   CALL          *          Same as invoker
 *       Kernel Key   SEND,RETURN   Resume     Named by resume key
 *       Kernel Key   SEND,RETURN   !Resume    NONE
 * 
 *       Note that the case of CALL on a kernel key is in some sense
 *       the same as SEND/RETURN on a kernel key -- the resume key was
 *       generated by the call.  I may in fact implement the first
 *       version this way.
 * 
 *    4. Determine if the invokee will receive a resume key to the
 *       invoker.  (yes if the invocation is a CALL).
 * 
 *    5. Determine if a resume key will be consumed by the invocation
 *       (yes if the invokee was named by a resume key, including the
 *       case of CALL on kernel key).
 * 
 *    6. Determine if the invokee is in the proper state:
 * 
 *       Invoked      Invocation    Invokee
 *       Key Type     Type          State
 * 
 *       Start Key    ANY           AVAILABLE
 *       Resume Key   ANY           WAITING
 *       Kernel Key   CALL          RUNNING (really waiting per note above)
 *       Kernel Key   SEND,RETURN   WAITING
 * 
 *    7. Determine if a new thread is to be created.
 * 
 *    8. Construct a receive buffer mapping, splicing in the
 *       kernel-reserved void page where no mapping is present.
 * 
 *    NONE OF THE ABOVE CAN MODIFY THE PROGRAM as the action may put
 *    the program to sleep, causing us to need to restart the entire
 *    invocation.
 * 
 * Action:
 *    Whatever the key is specified to do.  This may require stalling
 *    the application.  The interesting case is the gate key
 *    implementation.  Given that the above actions have already been
 *    performed, the gate key action
 * 
 * Exit:
 *    If a resume key is to be copied
 *      If already consumed copy a null key
 *      else construct resume key
 *    Migrate the outbound thread to the invokee.
 * 
 * 
 * NOTE DELICATE ISSUE
 * 
 *   Wrapper invocation is the only place where a resume key can
 *   become prepared without it's containing object being dirty (the
 *   other case -- key registers -- guarantees that the object is
 *   already dirty.  Preparing a resume key without guaranteeing dirty
 *   object causes resume key rescind to violate the checkpoint
 *   constraint by creating a situation in which the resume key may
 *   get turned into void before the containing node is stabilized.
 *   The code here is careful to check -- if the keeper gate key is a
 *   resume key, we mark the containing node dirty.
 * 
 *   In practice, if you use a resume key in a keeper slot you are a
 *   bogon anyway, so I'm not real worried about it...
 */

Invocation inv;
  
#ifndef NDEBUG
bool InvocationCommitted = false;
#endif

/* May Yield. */
void 
inv_Commit(Invocation* thisPtr)
{
  inv_MaybeDecommit(thisPtr);
  
#ifndef NDEBUG
  InvocationCommitted = true;
#endif

  if (act_Current()->context) {
    /* This is bogus, because this gets called from GateKey(), which
     * can in turn be called from InvokeMyKeeper, within which
     * CurContext is decidedly NOT runnable.
     *
     * assert ( act_CurContext()->IsRunnable() );
     */
#ifdef OLD_PC_UPDATE
#error this is not the case
    proc_SetPC((Process *) act_Current()->context , 
	       act_Current()->context->nextPC);
#endif
  }
}

bool 
inv_IsInvocationKey(Invocation* thisPtr, const Key* pKey)
{
  /* because this gets set up early in the keeper invocation
   * path, /inv.key/ may not be set yet, so check this early.
   */
  if (pKey == &thisPtr->redNodeKey)
    return true;
  
  if (thisPtr->key == 0)
    return false;
  
  if (pKey == thisPtr->key)
    return true;
  
  if (pKey == &thisPtr->scratchKey)
    return true;
  
  return false;
}

/* Some fields in Invocation are assumed to be initialized at the
   beginning of an invocation. Initialize them here, and also in inv_Cleanup. */
void
inv_InitInv(Invocation *thisPtr)
{
#ifndef NDEBUG
  InvocationCommitted = false;
#endif

  thisPtr->flags = 0;
}

void 
inv_Cleanup(Invocation* thisPtr)
{
  thisPtr->key = 0;
  
#ifndef NDEBUG
  InvocationCommitted = false;
#endif
  
  /* exit register values are set up in PopulateExitBlock, where they
   * are only done on one path.  Don't change them here.
   */
  
#ifndef NDEBUG
  /* Leaving the key pointers dangling causes no harm */
  thisPtr->entry.key[0] = 0;
  thisPtr->entry.key[1] = 0;
  thisPtr->entry.key[2] = 0;
  thisPtr->entry.key[3] = 0;
#endif

  /* NH_Unprepare is expensive.  Avoid it if possible: */
  if (thisPtr->flags & INV_SCRATCHKEY)
    key_NH_SetToVoid(&thisPtr->scratchKey);
  if (thisPtr->flags & INV_REDNODEKEY)
    key_NH_SetToVoid(&thisPtr->redNodeKey);
#if 0
  if (flags & INV_RESUMEKEY)
    resumeKey.NH_VoidKey();
#endif
#if 0
  if (thisPtr->flags & INV_EXITKEY0)
    exit.key[0].NH_VoidKey();
  if (flags & INV_EXITKEY3)
    exit.key[3].INH_VoidKey();
  if (flags & INV_EXITKEYOTHER) {
    exit.key[1].NH_VoidKey();
    exit.key[2].NH_VoidKey();
  }
#endif
  
  thisPtr->flags = 0;
}

/* Yields, does not return. */
void 
inv_RetryInvocation(Invocation* thisPtr)
{
  inv_Cleanup(thisPtr);
  act_CurContext()->runState = RS_Running;

  act_Yield(act_Current());
  /* Returning all the way to user mode and taking an invocation exception
  into the kernel again is very expensive on IA-32. 
  We could optimize out the change to and from user mode, if we can
  prove the process will just reexecute its invocation. 
  Don't forget to do what this path does, such as calling UpdateTLB. */
}

#ifndef NDEBUG
bool 
inv_IsCorrupted(Invocation* thisPtr)
{
  int i = 0;
  for (i = 0; i < 4; i++) {
    if (thisPtr->entry.key[i])
      return true;
#if 0
    if (exit.key[i].IsPrepared())
      return true;
#endif
  }
  return false;
}
#endif

/* Copy at most COUNT bytes out to the process.  If the process
 * receive length is less than COUNT, silently truncate the outgoing
 * bytes.  Rewrite the process receive count field to indicate the
 * number of bytes actually transferred.
 */
uint32_t 
inv_CopyOut(Invocation* thisPtr, uint32_t len, void *data)
{
  assert(InvocationCommitted);

  thisPtr->sentLen = len;

  if (thisPtr->validLen < len)
    len = thisPtr->validLen;

  thisPtr->exit.len = len;
  
  if (thisPtr->exit.len)
    memcpy(thisPtr->exit.data, data, thisPtr->exit.len);

  return thisPtr->exit.len;
}

/* Copy at most COUNT bytes in from the process.  If the process
 * send length is less than COUNT, copy the number of bytes in the
 * send buffer.  Return the number of bytes transferred.
 */
uint32_t 
inv_CopyIn(Invocation* thisPtr, uint32_t len, void *data)
{
  assert(InvocationCommitted);
  
  if (thisPtr->entry.len < len)
    len = thisPtr->entry.len;
  
  if (thisPtr->entry.len)
    memcpy(data, thisPtr->entry.data, len);

  return len;
}

/* Fabricate a prepared resume key. */
void 
proc_BuildResumeKey(Process* thisPtr,	/* resume key to this process */
  Key* resumeKey /*@ not null @*/)	/* must be not hazarded */
{
#ifndef NDEBUG
  if (!keyR_IsValid(&thisPtr->keyRing, thisPtr))
    fatal("Key ring screwed up\n");
#endif

  key_NH_Unchain(resumeKey);
  
  keyBits_InitType(resumeKey, KKT_Resume);
  keyBits_SetPrepared(resumeKey);
  resumeKey->u.gk.pContext = thisPtr;
  link_Init(&resumeKey->u.gk.kr);
  link_insertBefore(&thisPtr->keyRing, &resumeKey->u.gk.kr);

#if 0
  printf("Dumping key ring (context=0x%08x, resumeKey=0x%08x):\n"
		 "kr.prev=0x%08x kr.next=0x%08x\n",
		 this, &resumeKey, kr.prev, kr.next);
  KeyRing      *pkr=kr.prev;
  int		count=0;
  while (pkr!=&kr)
    {
      printf("pkr->prev=0x%08x pkr->next=0x%08x\n",
		     pkr->prev, pkr->next);
      ((Key *)pkr)->Print();
      pkr=pkr->prev;
      count++;
    }
  if (count>1)
    dprintf(true, "Multiple keys in key ring\n");
#endif

#ifdef GATEDEBUG
  dprintf(GATEDEBUG>2, "Crafted resume key\n");
#endif
}

#ifdef __cplusplus
extern "C" {
  uint64_t rdtsc();
};
#else
extern uint64_t rdtsc();
#endif

/* This is a carefully coded routine, probably less than clear.  The
 * version that handles all of the necessary cases is
 * DoGeneralKeyInvocation; this version is trying to cherry pick the
 * high flyers.
 * 
 * The common cases are, in approximate order:
 * 
 *  1. CALL   on a gate key
 *  2. RETURN on a gate key    (someday: via returner)
 *  3. CALL   on a kernel key
 *  4. Anything else
 * 
 * This version also assumes that the string arguments are valid, and
 * does only the fast version of the string test.
 */

/* May Yield. */
void 
proc_DoKeyInvocation(Process* thisPtr)
{
#ifdef OPTION_KERN_TIMING_STATS
  uint64_t pre_handler;
  uint64_t top_time = rdtsc();
#ifdef OPTION_KERN_EVENT_TRACING
  uint64_t top_cnt0 = mach_ReadCounter(0);
  uint64_t top_cnt1 = mach_ReadCounter(1);
#endif
#endif

  KernStats.nInvoke++;

#ifndef NDEBUG
  InvocationCommitted = false;
#endif

  thisPtr->nextPC = proc_CalcPostInvocationPC(thisPtr);

  /* Roll back the invocation PC in case we need to restart this
     operation */
  proc_AdjustInvocationPC(thisPtr);

  inv.suppressXfer = false;  /* suppress compiler bitching */
  
  objH_BeginTransaction();
  
  proc_SetupEntryBlock(thisPtr, &inv);

#if 0
  printf("Ivk proc=0x%08x ", thisPtr);
  if (thisPtr->procRoot &&
      keyBits_IsType(&thisPtr->procRoot->slot[ProcSymSpace], KKT_Number)) {
    void db_eros_print_number_as_string(Key* k);
    db_eros_print_number_as_string(&thisPtr->procRoot->slot[ProcSymSpace]);
  }
  printf(" invSlot=%d oc=%d\n",
         inv.key - &thisPtr->keyReg[0], inv.entry.code);
#endif

#ifdef OPTION_DDB
  if ( ddb_inv_flags )
    goto general_path;
#endif

  /* Send is a pain in the butt.  Fuck 'em if they can't take a joke. */
  if (inv.invType == IT_Send)
    goto general_path;

  if (!keyBits_IsPrepared(inv.key))
    goto general_path;

  /* If it's a segment key, it might be a red segment key.  Take the
   * long way:
   */
  if (inv.invKeyType == KKT_Segment)
    goto general_path;

  /* If it's a wrapper key, take the long way:
   */
  if (inv.invKeyType == KKT_Wrapper)
    goto general_path;

  if ((inv.invType == IT_PReturn) && inv.invKeyType != KKT_Resume)
    goto general_path;

  if (keyBits_IsGateKey(inv.key)) {
    inv.invokee = inv.key->u.gk.pContext;
    if (proc_IsWellFormed(inv.invokee) == false)
      goto general_path;

    /* If this is a resume key, we know the process is in the
     * RS_Waiting state.  If it is a start key, the process might be
     * in any state.  Do not solve the problem here to avoid loss of
     * I-cache locality in this case; we are going to sleep anyway.
     */
    if (inv.invKeyType == KKT_Start && inv.invokee->runState != RS_Available)
      goto general_path;
  }
  else {
    if (inv.invType != IT_Call)
      goto general_path;
    
    inv.invokee = thisPtr;
  }
  
#ifdef OPTION_PURE_ENTRY_STRINGS
  if (inv.entry.len != 0)
    SetupEntryString(inv);
#endif

  if (proc_IsRunnable(inv.invokee) == false)
    goto general_path;
  
  if (inv.invKeyType == KKT_Resume && inv.key->keyPerms == KPRM_FAULT)
    inv.suppressXfer = true;

  /* At this point, the only possible invocations we could be handling
     are CALL, RETURN, and PRETURN, so the following is correct: */
  thisPtr->runState = (inv.invType == IT_Call) ? RS_Waiting : RS_Available;

  /* It does not matter if the invokee fails to prepare!
   * Note that we may be calling this with 'this == 0'
   */
  proc_SetupExitBlock(inv.invokee, &inv);
#ifdef OPTION_PURE_EXIT_STRINGS
#error conversion required here
  if (inv.validLen != 0)
    inv.invokee->SetupExitString(inv, inv.validLen);
#endif

  {
#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
    if (dbg_wild_ptr)
      check_Consistency("DoKeyInvocation() before invoking handler\n");
#endif

#if defined(OPTION_KERN_TIMING_STATS)
    pre_handler = rdtsc();
#endif
#ifdef FNDISPATCH
    proc_KeyDispatch(&inv);
#else	
    keyHandler[inv.invKeyType](&inv);
#endif
#if defined(OPTION_KERN_TIMING_STATS)
    {
      extern uint32_t inv_delta_reset;
      if (inv_delta_reset == 0) {
	extern uint64_t inv_handler_cy;
	uint64_t post_handler = rdtsc();
	inv_handler_cy += (post_handler - pre_handler);
	inv_KeyHandlerCycles[inv.invKeyType][inv.invType] +=
	  (post_handler - pre_handler);
	inv_KeyHandlerCounts[inv.invKeyType][inv.invType] ++;
      }
    }  
#endif

#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
    if (dbg_wild_ptr)
      check_Consistency("DoKeyInvocation() after invoking handler\n");
#endif

    assert (InvocationCommitted);

#if !defined(OPTION_NEW_PC_ADVANCE) && !defined(OLD_PC_UPDATE)
#error this is not the case
    proc_SetPC(thisPtr, thisPtr->nextPC);
#endif
    assert(act_Current()->context == thisPtr);
#ifndef OPTION_NEW_PC_ADVANCE
#error this is not the case
    assertex(thisPtr, thisPtr->trapFrame.EIP == thisPtr->nextPC);
#endif

#ifndef NDEBUG
    InvocationCommitted = false;    
#endif
  }

  /* Now for the tricky part.  It's possible that the proces did an
   * invocation whose effect was to blow the invokee apart.  I know of
   * no way to speed these checks:
   */

  if (proc_IsNotRunnable(inv.invokee)) {
    if (inv.invokee->procRoot == 0)
      goto invokee_died;

    proc_Prepare(inv.invokee);

    if (proc_IsNotRunnable(inv.invokee))
      goto invokee_died;
  }

  inv.invokee->runState = RS_Running;

#ifdef OPTION_NEW_PC_ADVANCE
  /* 
     Invokee is resuming from either waiting or available state, so
     advance their PC past the trap instruction.

     If this was a kernel key invocation in the fast path, we never
     bothered to actually set them waiting, but they were logically in
     the waiting state nonetheless.

     This is the C fast path, so we don't need to worry about the SEND
     case.
  */
  /* dprintf(false, "Advancing PC in fast path\n"); */

  proc_SetPC(inv.invokee, inv.invokee->nextPC);
  proc_ClearNextPC(inv.invokee);
#endif

  if (!inv.suppressXfer) {
    proc_DeliverResult(inv.invokee, &inv);
#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
    if (dbg_wild_ptr)
      check_Consistency("DoKeyInvocation() after DeliverResult()\n");
#endif
  }

  if (inv.invokee != thisPtr) {
    /* Following is only safe because we handle non-call on primary
     * key in the general path.
     */
    if (inv.invKeyType == KKT_Resume)
      keyR_ZapResumeKeys(&inv.invokee->keyRing);

    act_MigrateTo(act_Current(), inv.invokee);
#ifdef OPTION_DDB
    /*if (inv.invokee->priority == pr_Never)*/
    if (inv.invokee->readyQ == &prioQueues[pr_Never])
      dprintf(true, "Thread now in ctxt 0x%08x w/ bad schedule\n", 
              inv.invokee);
#endif
  }
  
  if (thisPtr->runState == RS_Available)
    sq_WakeAll(&thisPtr->stallQ, false);

  inv_Cleanup(&inv);
  inv.invokee = 0;

#ifdef OPTION_KERN_TIMING_STATS
  {
    extern uint32_t inv_delta_reset;

    if (inv_delta_reset == 0) {
      extern uint64_t inv_delta_cy;

      uint64_t bot_time = rdtsc();
#ifdef OPTION_KERN_EVENT_TRACING
      extern uint64_t inv_delta_cnt0;
      extern uint64_t inv_delta_cnt1;

      uint64_t bot_cnt0 = mach_ReadCounter(0);
      uint64_t bot_cnt1 = mach_ReadCounter(1);
      inv_delta_cnt0 += (bot_cnt0 - top_cnt0);
      inv_delta_cnt1 += (bot_cnt1 - top_cnt1);
#endif
      inv_delta_cy += (bot_time - top_time);
    }
    else
      inv_delta_reset = 0;
  }
#endif

#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
  if (dbg_wild_ptr)
    check_Consistency("bottom DoKeyInvocation()");
#endif
  
  return;

 general_path:
  thisPtr->runState = RS_Running;
  /* This path has its own, entirely separate recovery logic.... */
  proc_DoGeneralKeyInvocation(thisPtr);
  return;
  
 invokee_died:
  inv.invokee = 0;
  inv_Cleanup(&inv);
  act_MigrateTo(act_Current(), 0);
}

Activity *activityToRelease = 0;

/* May Yield. */
void
proc_DoGeneralKeyInvocation(Process* thisPtr)
{
  Process *p = 0;
  Process *wakeRoot = 0;
  Activity *activityToMigrate = 0;
#if defined(OPTION_DDB) && !defined(NDEBUG)
  bool invoked_gate_key;
#endif
  
  /* Revise the invoker runState to what it will be when this is all
   * over.  We'll fix it above if we yield.
   */
  static const uint8_t newState[IT_NUM_INVTYPES] = {
    RS_Available,		/* IT_Return */
    RS_Available,		/* IT_PReturn */
    RS_Waiting,			/* IT_Call */
    RS_Running,			/* IT_Send */
  };

#ifdef OPTION_KERN_TIMING_STATS
  uint64_t top_time = rdtsc();
  uint64_t pre_handler;
#ifdef OPTION_KERN_EVENT_TRACING
  uint64_t top_cnt0 = mach_ReadCounter(0);
  uint64_t top_cnt1 = mach_ReadCounter(1);
#endif
#endif

  activityToRelease = 0;
  
  /* Set up the entry block, faulting in any necessary data pages and
   * constructing an appropriate kernel mapping:
   */
  proc_SetupEntryBlock(thisPtr, &inv);

#ifdef OPTION_PURE_ENTRY_STRINGS
  if (inv.entry.len != 0)
    SetupEntryString(inv);
#endif

#ifdef GATEDEBUG
  dprintf(true, "Populated entry block\n");
#endif

  assert(keyBits_IsPrepared(inv.key));
  
  /* If this is a prompt return, it MUST be done on a
   * resume key. If it isn't, behave as for prompt return on void key.
   */
  if ((inv.invType == IT_PReturn)
      && (inv.invKeyType != KKT_Resume)) {
    /* dprintf(true, "PTRETURN on non-resume key!\n"); */
    inv.invType = IT_PReturn;
    inv.key = &key_VoidKey;
#ifndef invKeyType
    inv.invKeyType = KKT_Void;
#endif
    inv.invokee = thisPtr;

#if 0
    key_Prepare(inv.key);	/* MAY YIELD!!! */
#endif
  }

  /* There are two cases where the actual invocation may proceed on a
   * key other than the invoked key:
   * 
   *   Invocation of kept red segment key proceeds as invocation on
   *     the keeper, AND observes the slot 2 convention of the format
   *     key!!!   Because this must overwrite slot 2, it must occur
   *     following the entry block preparation.
   * 
   *   Gate key to malformed domain proceeds as invocation on void.
   * 
   * The red seg test is done first because the extracted gate key (if
   * any) needs to pass the well-formed test too.
   *
   * In the latest revised kernel, the Wrapper node is beginning to
   * subsume the role of the red segment node. We do either/or logic
   * here because we don't want them to nest.
   */
  
  if ( inv.invKeyType == KKT_Wrapper
       && keyBits_IsReadOnly(inv.key) == false
       && keyBits_IsNoCall(inv.key) == false
       && keyBits_IsWeak(inv.key) == false ) {

    /* The original plan here was to hide all of the sanity checking
     * for the wrapper node in the PrepAsWrapper() logic. That doesn't
     * work out -- it causes the wrapper node to need deprepare as a
     * unit when slots are altered, with the unfortunate consequence
     * that perfectly good mapping tables can get discarded. It is
     * therefore better to check the necessary constraints here.
     */
    Node *wrapperNode = (Node *) key_GetObjectPtr(inv.key);
    Key* fmtKey /*@ not null @*/ = &wrapperNode->slot[WrapperFormat];

    if (keyBits_IsType(fmtKey, KKT_Number)
	&& keyBits_IsGateKey(&wrapperNode->slot[WrapperKeeper]) ) {
      /* Unlike the older red segment logic, the format key has
       * preassigned slots for the keeper, address space, and so
       * forth.
       */

      if (fmtKey->u.nk.value[0] & WRAPPER_BLOCKED) {
	keyBits_SetWrHazard(fmtKey);

	act_SleepOn(act_Current(), 
		    ObjectStallQueueFromObHdr(&wrapperNode->node_ObjHdr));
	act_Yield(act_Current());
      }

      if (fmtKey->u.nk.value[0] & WRAPPER_SEND_NODE) {
	/* Not hazarded because invocation key */
	key_NH_Set(&inv.scratchKey, inv.key);
	keyBits_SetType(&inv.scratchKey, KKT_Node);
	inv.entry.key[2] = &inv.scratchKey;
	inv.flags |= INV_SCRATCHKEY;
      }

      if (fmtKey->u.nk.value[0] & WRAPPER_SEND_WORD)
	inv.entry.w1 = fmtKey->u.nk.value[1];

      /* Not hazarded because invocation key */
      inv.key = &(wrapperNode->slot[WrapperKeeper]);
#ifndef invKeyType
      inv.invKeyType = keyBits_GetType(inv.key);
#endif

      /* Prepared resume keys can only reside in dirty objects! */
      if (inv.invKeyType == KKT_Resume)
	node_MakeDirty(wrapperNode);
	    
      key_Prepare(inv.key);	/* MAY YIELD!!! */
    }
  } 
  
  assert(keyBits_IsPrepared(inv.key));

  inv.invokee = 0;		/* until proven otherwise */
  
  /* Right now a corner case here is buggered because we have not yet
   * updated the caller's runstate according to the call type.  As a
   * result, a return on a start key to yourself won't work in this
   * code.
   */
  
  if ( keyBits_IsGateKey(inv.key) ) {
    assert (keyBits_IsPrepared(inv.key));
    /* Make a local copy (subvert alias analysis pessimism) */
    p = inv.key->u.gk.pContext;
    inv.invokee = p;
    proc_Prepare(p);		/* may yield */

    /* This is now checked in pk_GateKey.cxx */
    if (inv.invKeyType == KKT_Resume && inv.key->keyPerms == KPRM_FAULT)
      inv.suppressXfer = true;

    if (proc_IsWellFormed(p) == false) {
      /* Not hazarded because invocation key */
      /* Pretend we invoked a void key. */
      inv.key = &key_VoidKey;
#ifndef invKeyType
      inv.invKeyType = KKT_Void;
#endif
      inv.invokee = thisPtr;
      inv.entry.key[RESUME_SLOT] = &key_VoidKey;
#ifndef NDEBUG
      printf("Jumpee malformed\n");
#endif
    }

#ifndef NDEBUG
    else if (inv.invKeyType == KKT_Resume &&
	     p->runState != RS_Waiting) {
      fatal("Resume key to wrong-state context\n");
    }
#endif
    else if (inv.invKeyType == KKT_Start && p->runState != RS_Available) {
      act_SleepOn(act_Current(), &p->stallQ);
      act_Yield(act_Current());
    }
#if 0
    else if ( inv.invType == IT_Call ) {
      BuildResumeKey(inv.resumeKey);
      inv.entry.key[RESUME_SLOT] = &inv.resumeKey;
      inv.flags |= INV_RESUMEKEY;
    }
#endif
  }
  else if (inv.invType == IT_Call) {
    /* Call on non-gate key always returns to caller and requires no
     * resume key.
     *
     * ISSUE: This will not look right to DDB, but it's correct. When
     * you next look at puzzling DDB state and wonder why the resume
     * key never got generated, it is because I did not want to deal
     * with the necessary key destruction overhead. The register
     * renaming key fabrication strategy should regularize this rather
     * nicely when we get there.
     */
    inv.invokee = thisPtr;
    inv.entry.key[3] = &key_VoidKey;
  }
  else {
    /* Kernel key invoked with RETURN or SEND.  Key in slot 3 must be a
     * resume key, and if so the process must be waiting, else we will
     * not return to anyone.
     */

    Key* rk /*@ not null @*/ = inv.entry.key[3];
    key_Prepare(rk);

    /* Kernel keys return as though via the returner, so the key in
     * slot four must be a resume key to a party in the right state.
     */

    assert(keyBits_IsHazard(rk) == false);
    
    if (keyBits_IsPreparedResumeKey(rk)) {
      Process *p = rk->u.gk.pContext;
      proc_Prepare(p);

      assert(p->runState == RS_Waiting);
      inv.invokee = p;

      /* it can, however, be a fault key.  Since we are not going via
       * GateKey(), request xfer suppression here.
       */
      if (rk->keyPerms == KPRM_FAULT)
	inv.suppressXfer = true;
    }
    else
      assert (inv.invokee == 0);
  }

  /* Pointer to the domain root (if any) whose sleepers we should
   * awaken on successful completion:
   */
  wakeRoot = 0;


  thisPtr->runState = newState[inv.invType];
  if (thisPtr->runState == RS_Available)
    /* Ensure that we awaken the sleeping activityies (if any). */
    wakeRoot = thisPtr;
  
  /********************************************************************
   * AT THIS POINT we know that the invocation will complete in
   * principle. It is still possible that the invoker will block while
   * some part of the invokee gets paged in or while waiting for an
   * available Activity structure.  The latter is a problem, and needs
   * to be dealt with.  Note that the finiteness of the activity pool
   * isn't the source of the problem -- the real problem is that we
   * might not be able to fit all of the running domains in the swap
   * area. Eventually we shall need to implement a decongester to deal
   * with this, but that can wait.
   *********************************************************************/


#ifdef GATEDEBUG
  dprintf(GATEDEBUG>3, "Checked for well-formed recipient\n");
#endif


  if (inv.invokee && proc_IsWellFormed(inv.invokee) == false)
    inv.invokee = 0;


  assert(keyBits_IsPrepared(inv.key));
  
#ifdef GATEDEBUG
  dprintf(GATEDEBUG>2, "Invokee now valid\n");
#endif

  assert(inv.invokee == 0 || proc_IsRunnable(inv.invokee));
  
  /* It does not matter if the invokee fails to prepare!
   * Note that we may be calling this with 'this == 0'
   */
  proc_SetupExitBlock(inv.invokee, &inv);
#ifdef OPTION_PURE_EXIT_STRINGS
  if (inv.validLen != 0)
    inv.invokee->SetupExitString(inv, inv.validLen);
#endif

#ifdef GATEDEBUG
  dprintf(GATEDEBUG>2, "Populated exit block\n");
#endif
  
#ifdef GATEDEBUG
  if (inv.suppressXfer)
    dprintf(true, "xfer is suppressed\n");
#endif
  
  /* Identify the activity that will migrate to the recipient.  Normally
   * it's the current activity.  If this is a SEND invocation, it's a
   * new activity.  Populate this activity consistent with the invokee.
   */
  
  
  activityToMigrate = act_Current();
  

  if (inv.invType == IT_Send) {
    Key* rk /*@ not null @*/ = inv.entry.key[3];

    /* If this is a send, and we are either (a) invoking a gate key,
       or (b) invoking a kernel key and passing a resume key to
       someone else, then we will need a new activity.

       That covers most cases. For real fun, consider a send on a
       domain key saying 'start this process' with resume key to third
       party in slot 3 -- we will (someday) handle that in the
       ProcessKey handler as a special case prior to the
       COMMIT_POINT() logic. */

    if ( keyBits_IsGateKey(inv.key) || keyBits_IsType(rk, KKT_Resume) ) {
  
      activityToMigrate = (Activity *) act_AllocActivity();
  
      activityToRelease = activityToMigrate;
      activityToMigrate->state = act_Ready;
#ifdef GATEDEBUG
      dprintf(true, "Built new activity for fork\n");
#endif
    }
    else
      activityToMigrate = 0;
  }
  
#if defined(OPTION_DDB) && !defined(NDEBUG)
  invoked_gate_key = keyBits_IsGateKey(inv.key);
  
#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
  if (dbg_wild_ptr)
    check_Consistency("DoKeyInvocation() before invoking handler\n");
#endif

  /* suppressXfer only gets set if this was a fault key, in which case 
   * this is likely a re-invocation of the process by the keeper.
   */
  if ( DDB_STOP(all) ||
       (DDB_STOP(gate) && invoked_gate_key) ||
       (DDB_STOP(keeper) && inv.suppressXfer) ||
       (DDB_STOP(pflag) && 
	( (thisPtr->processFlags & PF_DDBINV) ||
	  (inv.invokee && inv.invokee->processFlags & PF_DDBINV) ))
       )
    dprintf(true, "About to invoke key handler (inv.ty=%d) ic=%d\n",
		    inv.invKeyType, KernStats.nInvoke);
#endif

#if defined(OPTION_KERN_TIMING_STATS)
  pre_handler = rdtsc();
#endif

#ifdef FNDISPATCH
  proc_KeyDispatch(&inv);
#else	
  keyHandler[inv.invKeyType](&inv);
#endif
  
#if defined(OPTION_KERN_TIMING_STATS)
  {
    extern uint32_t inv_delta_reset;
    if (inv_delta_reset == 0) {
      extern uint64_t inv_handler_cy;
      uint64_t post_handler = rdtsc();
      inv_handler_cy += (post_handler - pre_handler);
      inv_KeyHandlerCycles[inv.invKeyType][inv.invType] +=
	(post_handler - pre_handler);
      inv_KeyHandlerCounts[inv.invKeyType][inv.invType] ++;
    }
  }  
#endif

#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
  if (dbg_wild_ptr)
    check_Consistency("DoKeyInvocation() after invoking handler\n");
#endif

  assert (InvocationCommitted);

#if !defined(OLD_PC_UPDATE) && !defined(OPTION_NEW_PC_ADVANCE)
#error this is not the case
  proc_SetPC(thisPtr, thisPtr->nextPC);
#endif
  assert(act_Current()->context == thisPtr);
#ifndef OPTION_NEW_PC_ADVANCE
#error this is not the case
  assertex(thisPtr, thisPtr->trapFrame.EIP == thisPtr->nextPC);
#endif

#ifndef NDEBUG
  InvocationCommitted = false;    
#endif
  
#if defined(OPTION_DDB) && !defined(NDEBUG)
  /* inv.suppressXfer only gets set if this was a fault key, in which
   * case this is likely a re-invocation of the process by the keeper.
   * FIX: This is no longer true
   */
  if ( DDB_STOP(all) ||
       ( DDB_STOP(gate) && invoked_gate_key ) ||
       ( DDB_STOP(keeper) && inv.suppressXfer) ||
       ( DDB_STOP(return) && (   inv.invType == IT_NPReturn
                              || inv.invType == IT_PReturn ) ) ||
       (DDB_STOP(pflag) && 
	( (thisPtr->processFlags & PF_DDBINV) ||
	  (inv.invokee && inv.invokee->processFlags & PF_DDBINV) )) ||
       ( DDB_STOP(keyerr) &&
	 !invoked_gate_key &&
	 (inv.invKeyType != KKT_Void) &&
	 (inv.exit.code != RC_OK) ) )
    dprintf(true, "Before DeliverResult() (invokee=0x%08x)\n",
		    inv.invokee); 
#endif
  
  /* Check the sanity of the receiving process in various ways: */
  
  if (inv.invokee) {
    if (proc_IsNotRunnable(inv.invokee)) {
      if (inv.invokee->procRoot == 0) {
	inv.invokee = 0;
	goto bad_invokee;
      }
      
      proc_Prepare(inv.invokee);

      if (proc_IsNotRunnable(inv.invokee)) {
	inv.invokee = 0;
	goto bad_invokee;
      }
    }

    /* Invokee is okay.  Deliver the result: */
    inv.invokee->runState = RS_Running;

#ifdef OPTION_NEW_PC_ADVANCE
    /* Invokee is resuming from either waiting or available state, so
       advance their PC past the trap instruction.

       If this was a kernel key invocation in the fast path, we never
       bothered to actually set them waiting, but they were logically
       in the waiting state nonetheless. */
    /* dprintf(false, "Advancing PC in slow path\n"); */
    proc_SetPC(inv.invokee, inv.invokee->nextPC);
    proc_ClearNextPC(inv.invokee);
#endif

    if (!inv.suppressXfer) {
      proc_DeliverResult(inv.invokee, &inv);
#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
      if (dbg_wild_ptr)
	check_Consistency("DoKeyInvocation() after DeliverResult()\n");
#endif
    }

    /* If we are returning to ourselves, the resume key was never
     * generated.
     */
    if (inv.invokee != thisPtr)
      keyR_ZapResumeKeys(&inv.invokee->keyRing);
  }
 bad_invokee:
  
#ifdef OPTION_NEW_PC_ADVANCE
  if (inv.invType == IT_Send) {
    /* dprintf(false, "Advancing SENDer PC in slow path\n"); */
    proc_SetPC(thisPtr, thisPtr->nextPC);
    proc_ClearNextPC(inv.invokee);
  }
#endif

  /* ONCE DELIVERRESULT IS CALLED, NONE OF THE INPUT CAPABILITIES
     REMAINS ALIVE!!! */
  
  /* Clean up the invocation block: */
  inv_Cleanup(&inv);
#ifdef GATEDEBUG
  dprintf(GATEDEBUG>2, "Cleaned up invocation\n");
#endif

#ifdef GATEDEBUG
  dprintf(GATEDEBUG>2, "Updated invokee runstate\n");
#endif
  
  if (activityToMigrate) {
    act_MigrateTo(activityToMigrate, inv.invokee);

#ifdef OPTION_DDB
    /*if (inv.invokee->priority == pr_Never)*/
    if (inv.invokee->readyQ == &prioQueues[pr_Never])
      dprintf(true, "Activity now in ctxt 0x%08x w/ bad schedule\n", 
		      inv.invokee);
#endif

    if (inv.invType == IT_Send && inv.invokee) {
      act_Wakeup(activityToMigrate);
#ifdef GATEDEBUG
      dprintf(true, "Woke up forkee\n");
#endif
    }
  }
  
  if (wakeRoot) {
#if 0
    dprintf(false, "Wake up all of the losers sleeping on dr=0x%08x\n", wakeRoot);
#endif
    sq_WakeAll(&wakeRoot->stallQ, false);
  }
  
#ifdef DBG_WILD_PTR
  {
    extern void ValidateAllActivityies();
    ValidateAllActivities();
  }
#endif

#ifdef GATEDEBUG
  dprintf(GATEDEBUG>2, "Migrated the activity\n");
#else
#if 0
  if ( invoked_gate_key )
    dprintf(true, "Migrated the activity\n");
#endif
#endif

  inv.invokee = 0;
  activityToRelease = 0;
  
#ifdef OPTION_KERN_TIMING_STATS
  {
    extern uint32_t inv_delta_reset;

    if (inv_delta_reset == 0) {
      extern uint64_t inv_delta_cy;

      uint64_t bot_time = rdtsc();
#ifdef OPTION_KERN_EVENT_TRACING
      extern uint64_t inv_delta_cnt0;
      extern uint64_t inv_delta_cnt1;

      uint64_t bot_cnt0 = mach_ReadCounter(0);
      uint64_t bot_cnt1 = mach_ReadCounter(1);
      inv_delta_cnt0 += (bot_cnt0 - top_cnt0);
      inv_delta_cnt1 += (bot_cnt1 - top_cnt1);
#endif
      inv_delta_cy += (bot_time - top_time);
    }
    else
      inv_delta_reset = 0;
  }
#endif
}

/* KEEPER INVOCATION -- this looks a lot like key invocation, and the
 * code for the two should probably be merged.  The difficulty is that
 * the keeper invocation code is able to make a variety of useful
 * assumptions about abandonment that the general path cannot make.
 */
/* May Yield. */
void
proc_InvokeMyKeeper(Process* thisPtr, uint32_t oc,
                    uint32_t warg1,
                    uint32_t warg2,
                    uint32_t warg3,
                    Key *keeperKey,
                    Key* keyArg2, uint8_t *data, uint32_t len)
{
  bool canInvoke;
  Process * /*const*/ invokee;
#ifdef OPTION_KERN_TIMING_STATS
  uint64_t top_time = rdtsc();
#ifdef OPTION_KERN_EVENT_TRACING
  uint64_t top_cnt0 = mach_ReadCounter(0);
  uint64_t top_cnt1 = mach_ReadCounter(1);
#endif
#endif

  inv.suppressXfer = false;
  KernStats.nInvoke++;
  KernStats.nInvKpr++;
  
  if ( DDB_STOP(keeper) )
    dprintf(true, "About to invoke process keeper\n");

#ifdef KPRDEBUG
  dprintf(true, "Enter InvokeMyKeeper\n");
#endif
  canInvoke = true;
  
  /* Do not call BeginTransaction here -- the only way we can be here
   * is if we are already in some transaction, and it's okay to let
   * that transaction prevail.
   */
  
  key_Prepare(keeperKey);

  if (keyBits_IsGateKey(keeperKey) == false)
    canInvoke = false;
  
#ifdef KPRDEBUG
  dprintf(true, "Kpr key is gate key? '%c'\n", canInvoke ? 'y' : 'n');
#endif

  /* A recovery context has already been established, either by a call
   * to PageFault handler or by act_Resched().
   */

#ifndef NDEBUG
  InvocationCommitted = false;
#endif
  /* Not hazarded because invocation key */
  inv.key = keeperKey;
  inv.invType = IT_Call;
#ifndef invKeyType
  inv.invKeyType = keyBits_GetType(keeperKey);
#endif

  thisPtr->nextPC = proc_GetPC(thisPtr);

#ifdef KPRDEBUG
  dprintf(true, "Populated invocation block\n");
#endif

  if (keyBits_IsType(keeperKey, KKT_Resume)
      && keeperKey->keyPerms == KPRM_FAULT) {
    inv.suppressXfer = true;
    printf("Suppress xfer -- key is restart key\n");
  }

  invokee = canInvoke ? keeperKey->u.gk.pContext : 0;

  if (invokee) {
    inv.invokee = invokee;
    
#ifdef KPRDEBUG
    dprintf(true, "Prepare invokee\n");
#endif

    /* Make sure invokee is valid */
    proc_Prepare(invokee);
  
#ifdef KPRDEBUG
    dprintf(true, "Invokee prepare completed\n");
#endif

    if ( proc_IsWellFormed(invokee) == false )
      canInvoke = false;

#ifdef KPRDEBUG
    dprintf(true, "Have good keeper, ctxt=0x%08x\n", invokee);
#endif

#ifndef NDEBUG
    if (keyBits_IsType(keeperKey, KKT_Resume) && invokee->runState != RS_Waiting)
      /* Bad resume key! */
      canInvoke = false;
#endif

    if (keyBits_IsType(keeperKey, KKT_Start) && invokee->runState != RS_Available) {
      act_SleepOn(act_Current(), &invokee->stallQ);
      act_Yield(act_Current());
    }

    if (keyBits_IsType(keeperKey, KKT_Resume) && keeperKey->keyPerms == KPRM_FAULT)
      inv.suppressXfer = true;
  }

  if (canInvoke) {
#ifdef KPRDEBUG
    dprintf(true, "Keeper in right state\n");
#endif

    /* Now have valid invokee in proper state.  Build exit block: */
    proc_SetupExitBlock(invokee, &inv);
    if (inv.validLen != 0)
      proc_SetupExitString(invokee, &inv, len);

#ifdef KPRDEBUG
    dprintf(true, "Populated exit block\n");
#endif

    assert(thisPtr == act_Current()->context);

    COMMIT_POINT();
    
    thisPtr->runState = RS_Waiting;
  
    assert (InvocationCommitted);

#if !defined(OLD_PC_UPDATE) && !defined(OPTION_NEW_PC_ADVANCE)
#error this is not the case
    proc_SetPC(thisPtr, thisPtr->nextPC);
#endif
    assert(act_Current()->context == thisPtr);
#ifndef OPTION_NEW_PC_ADVANCE
#error this is not the case
    assertex(thisPtr, thisPtr->trapFrame.EIP == thisPtr->nextPC);
#endif

#ifdef OPTION_NEW_PC_ADVANCE
    /* Invokee is resuming from either waiting or available state, so
       advance their PC past the trap instruction.

       If this was a kernel key invocation in the fast path, we never
       bothered to actually set them waiting, but they were logically
       in the waiting state nonetheless. */
    /* dprintf(false, "Advancing PC in gate key path\n"); */
    proc_SetPC(invokee, invokee->nextPC);
    proc_ClearNextPC(inv.invokee);
#endif

    if (!inv.suppressXfer) {
      inv_CopyOut(&inv, len, data);
    
      /* This is weird, because we are using DeliverGateResult(): */
      
      inv.entry.code = oc;
      inv.entry.w1 = warg1;
      inv.entry.w2 = warg2;
      inv.entry.w3 = warg3;

      inv.entry.key[0] = &key_VoidKey;
      inv.entry.key[1] = &key_VoidKey;
      inv.entry.key[2] = keyArg2;
      inv.entry.key[3] = &key_VoidKey;
      
      proc_DeliverGateResult(invokee, &inv, true);
    
#if defined(DBG_WILD_PTR) || defined(TESTING_INVOCATION)
      if (dbg_wild_ptr)
	check_Consistency("DoKeyInvocation() after invoking keeper\n");
#endif
    }
    
#ifndef NDEBUG
    InvocationCommitted = false;    
#endif

    invokee->runState = RS_Running;

    if (keyBits_IsType(keeperKey, KKT_Resume))
      keyR_ZapResumeKeys(&invokee->keyRing);

    /* Clean up the invocation block: */
    inv_Cleanup(&inv);
 

#ifdef GATEDEBUG
    dprintf(GATEDEBUG>2, "Cleaned up invocation\n");
#endif

    act_MigrateTo(act_Current(), invokee);

#ifdef OPTION_DDB
    /*if (invokee->priority == pr_Never)*/
    if (invokee->readyQ == &prioQueues[pr_Never])
      dprintf(true, "Activity now in ctxt 0x%08x w/ bad schedule\n", 
		      invokee);
#endif
#ifdef KPRDEBUG
    dprintf(true, "Activity %s has migrated\n", act_Current()->Name());
#endif
    inv.invokee = 0;
  }
  else {
#ifndef NDEBUG
    OID oid = act_CurContext()->procRoot->node_ObjHdr.oid;
    printf("No keeper for OID 0x%08x%08x, FC %d FINFO 0x%08x\n",
		   (uint32_t) (oid >> 32),
                   (uint32_t) oid,
		   ((Process *)act_CurContext())->faultCode, 
		   ((Process *)act_CurContext())->faultInfo);
    dprintf(true,"Dead context was 0x%08x\n", act_CurContext());
#endif
    /* Just retire the activity, leaving the domain in the running
     * state.
     */
    act_MigrateTo(act_Current(), 0);
#if 0
    act_Current()->SleepOn(procRoot->ObjectStallQueue());
    act_Yield(act_Current());
#endif
  }

#ifdef OPTION_KERN_TIMING_STATS
  {
    extern uint64_t kpr_delta_cy;

    uint64_t bot_time = rdtsc();
#ifdef OPTION_KERN_EVENT_TRACING
    extern uint64_t kpr_delta_cnt0;
    extern uint64_t kpr_delta_cnt1;
    
    uint64_t bot_cnt0 = mach_ReadCounter(0);
    uint64_t bot_cnt1 = mach_ReadCounter(1);
    kpr_delta_cnt0 += (bot_cnt0 - top_cnt0);
    kpr_delta_cnt1 += (bot_cnt1 - top_cnt1);
#endif
    kpr_delta_cy += (bot_time - top_time);
  }
#endif
}
