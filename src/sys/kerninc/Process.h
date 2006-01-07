#ifndef __PROCESS_H__
#define __PROCESS_H__
/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group.
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

/* This is the revised (improved?) process structure, with reduced
 * architecture-dependent complexity.  Note that we are moving towards
 * C-style architecture-dependent helper functions, so this is not a
 * class.
 */

/* CHANGES TO THIS FILE ****MUST**** BE MADE IN THE
 * ARCHITECTURE-SPECIFIC LAYOUT FILES TOO!!!  When the kernel is
 * converted to C we will be switching to using the LAYOUT file as the
 * ``official'' source.
 */

#include <eros/ProcessState.h>
#include <eros/ProcStats.h>
#include <arch-kerninc/SaveArea.h>
#include <arch-kerninc/PTE.h>
#ifdef EROS_HAVE_ARCH_REGS
#include <eros/machine/archregs.h>
#endif
#ifdef OPTION_PSEUDO_REGS
#include <eros/machine/pseudoregs.h>
#endif
#include <eros/machine/traceregs.h>

/* This file requires #include <kerninc/kernel.h> */
#include <kerninc/Node.h>
#include <kerninc/StallQueue.h>

/* Every running activity has an associated process structure.  The
 * process structure for user activities has a lot more state.  Process
 * structures for kernel activities are dedicated to the activity.  Process
 * structures for user activities are caches of domain state.  A process
 * is in use if it has a non-zero procRoot pointer.
 * 
 * Every process has an associated save area pointer.
 * 
 * Processes are reallocated in round-robin order, which may well prove
 * to be a dumb thing to do.  Kernel processes are not reallocated.
 */

 /* MACHINE DEPENDENT */
enum Hazards {
  hz_Malformed    = 0x01u,
  hz_DomRoot      = 0x02u,
  hz_KeyRegs      = 0x04u,
  hz_FloatRegs    = 0x08u,
  hz_Schedule     = 0x10u,
  hz_AddrSpace    = 0x20u,
#ifdef EROS_HAVE_FPU
  hz_NumericsUnit = 0x40u,	/* need to load FPU */
#endif
  hz_SingleStep   = 0x80u 	/* context may have a live activity */
};
typedef enum Hazards Hazards;

struct Process {
  /* Pieces of the currently loaded domain: */
  KeyRing	keyRing;

  StallQueue    stallQ;  /* procs waiting for this to be available */
  
  Node      	*procRoot;
  bool  isUserContext;

  /* hazards are reasons you cannot run.  They generally devolve to
   * being malformed or needing some functional unit loaded before you
   * can make progress.
   */
  
  uint32_t  hazards;

  /* END MACHINE DEPENDENT */

  /* SaveArea is nonzero exactly when the context has a valid save
   * image -- i.e. it is fully cached and the resulting context image
   * is RUNNABLE (i.e. doesn't violate processor privilege constraints).
   */

  savearea_t  *saveArea;
  struct ReadyQueue *readyQ;   /* ready queue info for this process */


  /* Processor we last ran on, to recover FP regs
   * Processor     *lastProcessor;
   */

  /* At most one activity in a given context at a time, which will be
   * pointed to by the context:
   */

  struct Activity   *curActivity;

  kpmap_t          MappingTable;

  savearea_t	    trapFrame;
#ifdef OPTION_PSEUDO_REGS
  pseudoregs_t      pseudoRegs;
#endif
  traceregs_t       traceRegs;

  /* This should immediately follow fixRegs: */
  Key               keyReg[EROS_PROCESS_KEYREGS];
  Key               lastInvokedKey;

  kva_t             cpuStack;

  uint32_t          faultCode;
  uint32_t          faultInfo;
  uint32_t          nextPC;

  uint8_t           runState;
  uint8_t           processFlags;
  
#ifdef OPTION_SMALL_SPACES
  uva_t             limit;
  ula_t             bias;
  struct PTE        *smallPTE;
#endif
  
#ifdef EROS_HAVE_FPU
  /* FPU support: */
  floatsavearea_t    fpuRegs;

#endif
  
#ifdef EROS_HAVE_ARCH_REGS
  archregs_t	    archRegs;
#endif

  Node              *keysNode;

  char              arch[4];

  /* Useful to have a name for diagnostic purposes. */
  char              name[8];

  ProcStat          stats;
};

/* Former static members of Process */
#ifdef EROS_HAVE_FPU
/* FPU support: */
extern Process*   proc_fpuOwner;	/* FIX: This is not SMP-feasible. */
#endif

#ifdef OPTION_SMALL_SPACES
extern struct PTE *proc_smallSpaces;
#endif

extern Process *proc_ContextCache;
/* End static members */

/* Walk a segment as described in WI until one of the following
 * occurs:
 * 
 *     You find a subsegment whose blss is <= STOPBLSS (walk
 *        succeeded, return true)
 *     You conclude that the desired type of access as described in
 *        ISWRITE cannot be satisfied in this segment (walk failed,
 *        return false)
 * 
 * At each stage in the walk, add dependency entries between the
 * traversed slots and the page table entries named by the passed
 * PTEs.
 * 
 * If 'prompt' is true, WalkSeg returns as described above.  If
 * prompt is false, WalkSeg invokes the prevailing keeper on error.
 * 
 * This routine is designed to be re-entered and cache it's
 * intervening state in the SegWalk structure so that the walk
 * does not need to be repeated.
 */
  
struct PTE;

/*extern*/ bool
proc_WalkSeg(Process * p,SegWalk* wi /*@ NOT NULL @*/, uint32_t stopBlss,
	     struct PTE* pPTE0, struct PTE* pPTE1, bool canMerge);

/* Former member functions of KernProcess */
typedef struct KernProcess KernProcess;

Process *kproc_Init(const char * name,
		    struct Activity* theActivity,
		    Priority prio,
                    struct ReadyQueue *rq,
		    void (*pc)(void),
		    uint32_t *StackBottom, uint32_t *StackTop);

/* Former member functions of Process */

INLINE bool 
proc_IsWellFormed(Process* thisPtr)
{
#ifndef NDEBUG
  if (thisPtr->faultCode == FC_MalformedProcess) {
    assert (thisPtr->processFlags & PF_Faulted);
    assert (thisPtr->saveArea == 0);
  }
#endif
  if (thisPtr->hazards & (hz_DomRoot|hz_KeyRegs|hz_FloatRegs|hz_Malformed)) {
    assert (thisPtr->saveArea == 0);
    return false;
  }
    return true;
}

INLINE Key* 
proc_GetSegRoot(Process* thisPtr)
{
  return node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace);
}

#ifdef EROS_HAVE_FPU
/* FPU support: */

void proc_SaveFPU(Process* thisPtr);
void proc_LoadFPU(Process* thisPtr);
void proc_DumpFloatRegs(Process* thisPtr);
#endif

#ifndef NDEBUG
bool proc_ValidKeyReg(const Key *pKey);
#endif

INLINE bool 
proc_IsKeyReg(const Key *pKey)
{
  /* This isn't quite right, as it will return TRUE for any random
   * pointer in to the process area, but that's good enough for all
   * the places that we use it.
   */
  
  if ( ((uint32_t) pKey >= (uint32_t) proc_ContextCache) &&
       ((uint32_t) pKey <
        (uint32_t) &proc_ContextCache[KTUNE_NCONTEXT]) ) {
    return true;
  }
  return false;
}


/* Returns true if the context has been successfully cached. A
 * context can be successfully cached without being runnable.  If
 * possible, sets the saveArea pointer to a valid save area.  The
 * saveArea pointer cannot be set if (e.g.) it's privileged
 * registers have improper values.
 */
void proc_DoPrepare(Process* thisPtr);

/* Fast-path inline version.  See comment above on DoPrepare().
 * MUST NOT yield if IsRunnable() would return true.
 */
INLINE void 
proc_Prepare(Process* thisPtr)
{
  /* Anything that invalidates the context will zap the saveArea
   * pointer, so this is a quick test, which is useful for fast
   * reload.
   */
  if (thisPtr->saveArea)
    return;
  
  proc_DoPrepare(thisPtr);
}

INLINE void 
proc_NeedRevalidate(Process* thisPtr)
{
  thisPtr->saveArea = 0;
}

/* needRevalidate means that the fault is due to a structural
 * problem in the process, and cannot be cleared without
 * revalidating the context cache.  Another way to think about this
 * is that any fault for which /needRevalidate/ is true is a fault
 * that cannot be cleared on the fast path.
 */

INLINE void 
proc_SetFault(Process* thisPtr, uint32_t code, uint32_t info, bool needRevalidate)
{
  thisPtr->faultCode = code;
  thisPtr->faultInfo = info;
  
  assert(thisPtr->faultCode != FC_MalformedProcess);
  
  if (thisPtr->faultCode)
    thisPtr->processFlags |= PF_Faulted;
  else
    thisPtr->processFlags &= ~PF_Faulted;
  
  if (needRevalidate)
    proc_NeedRevalidate(thisPtr);
  
#ifdef OPTION_DDB
  if (thisPtr->processFlags & PF_DDBTRAP)
    dprintf(true, "Process 0x%08x has trap code set\n", thisPtr);
#endif
}

INLINE void 
proc_SetMalformed(Process* thisPtr)
{
#ifdef OPTION_DDB
  /* This error is most likely of interest to the kernel developer,
     so for now: */
  dprintf(true, "Process is malformed\n");
#endif
  proc_SetFault(thisPtr, FC_MalformedProcess, 0, true);
  thisPtr->hazards |= hz_Malformed; 
}

/* USED ONLY BY INTERRUPT HANDLERS: */
INLINE void 
proc_SetSaveArea(Process* thisPtr, savearea_t *sa)
{
  thisPtr->saveArea = sa;
}

INLINE savearea_t *
proc_UnsafeSaveArea(Process* thisPtr)
{
  return thisPtr->saveArea;
}

INLINE bool 
proc_IsRunnable(Process* thisPtr)
{
  return thisPtr->saveArea ? true : false;
}

INLINE bool 
proc_IsNotRunnable(Process* thisPtr)
{
  return (thisPtr->saveArea == 0) ? true : false;
}

INLINE void 
proc_Deactivate(Process* thisPtr)
{
  assert (thisPtr->isUserContext);
  thisPtr->curActivity = 0;
}

INLINE void 
proc_SetActivity(Process* thisPtr, struct Activity *activity)
{
  assert(thisPtr->curActivity == 0 || thisPtr->curActivity == activity);
  thisPtr->curActivity = activity;
}
  
void proc_Resume(Process* thisPtr) NORETURN;

INLINE bool 
proc_IsUser(Process* thisPtr)
{
  return thisPtr->isUserContext;
}

INLINE bool 
proc_IsKernel(Process* thisPtr)
{
  return !thisPtr->isUserContext;
}

INLINE const char* 
proc_Name(Process* thisPtr)
{
  return thisPtr->name;
}

/* Called by checkpoint logic */
void proc_FlushAll();

void proc_DumpFixRegs(Process* thisPtr);
void proc_DumpPseudoRegs(Process* thisPtr);

/* Generic keeper invoker: */
void proc_InvokeMyKeeper(Process* thisPtr, uint32_t oc,
                         uint32_t warg1,
                         uint32_t warg2,
                         uint32_t warg3,
                         Key *keeperKey, Key* keyArg2,
                         uint8_t *data, uint32_t len);

bool proc_InvokeSegmentKeeper(Process* thisPtr, /*uint32_t code, */ SegWalk* /*@ not null @*/);

void proc_InvokeProcessKeeper(Process* thisPtr);

/******************************************************
 * Begin new code in support of new invocation logic
 ******************************************************/

void proc_BuildResumeKey(Process* thisPtr, Key* resumeKey /*@ not null @*/);

void proc_DoRetryInvocation(Process* thisPtr);
void proc_DoGeneralKeyInvocation(Process* thisPtr);
void proc_DoKeyInvocation(Process* thisPtr);

/* Following is only needed if assembly code is not used to validate
   the entry block. */
#ifndef ASM_ARG_VALIDATE
INLINE void proc_ValidateEntryBlock(Process* thisPtr);	/* may throw */
#endif

struct Invocation;

#ifdef ASM_VALIDATE_STRINGS
INLINE
#endif
void proc_SetupEntryString(Process* thisPtr, struct Invocation* inv /*@ not null @*/);

void proc_SetupExitString(Process* thisPtr, struct Invocation* inv /*@ not null @*/, uint32_t bound);

void proc_DeliverResult(Process* thisPtr, struct Invocation* inv /*@ not null @*/);


/******************************************************
 * End new code in support of new invocation logic
 ******************************************************/

/******************************************************
 *  Code in support of emulated instructions:
 ******************************************************/

typedef struct EmulatedInstr EmulatedInstr;
void proc_DoEmulatedInstr(Process* thisPtr, const EmulatedInstr* instr /*@ not null @*/);

/******************************************************
 * End new code in support of emulated instructions
 ******************************************************/

void proc_LoadFixRegs(Process* thisPtr);
#ifdef EROS_HAVE_FPU
void proc_LoadFloatRegs(Process* thisPtr);
#endif
void proc_LoadKeyRegs(Process* thisPtr);

void proc_FlushFixRegs(Process* thisPtr);
#ifdef EROS_HAVE_FPU
void proc_FlushFloatRegs(Process* thisPtr);
#endif
void proc_FlushKeyRegs(Process* thisPtr);

#ifdef OPTION_SMALL_SPACES
void proc_WriteDisableSmallSpaces();
#endif

void proc_AllocUserContexts(); /* machine dependent! */
Process *proc_allocate(bool isUser);
void proc_Load(Node* procRoot);

void proc_LoadAddressSpace(Process* thisPtr, bool prompt);

void proc_ValidateRegValues(Process* thisPtr);	/* returns false if faulted */

bool proc_HasDevicePriveleges(Process* thisPtr);

void proc_FlushProcessSlot(Process* thisPtr, uint32_t whichKey);

#ifdef OPTION_DDB
void proc_WriteBackKeySlot(Process* thisPtr, uint32_t whichKey);
#endif

void proc_SyncActivity(Process* thisPtr);

void proc_Unload(Process* thisPtr);

typedef struct Registers Registers;

bool proc_GetRegs32(Process* thisPtr, Registers* /*@ not null @*/);
bool proc_SetRegs32(Process* thisPtr, Registers* /*@ not null @*/);

#endif /* __PROCESS_H__ */
