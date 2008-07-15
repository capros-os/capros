#ifndef __PROCESS_H__
#define __PROCESS_H__
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

/* CHANGES TO THIS FILE ****MUST**** BE MADE IN THE
 * ARCHITECTURE-SPECIFIC LAYOUT FILE TOO!!!
 */

#ifndef __ASSEMBLER__

#include <kerninc/kernel.h>
#include <idl/capros/Process.h>
#include <eros/ProcStats.h>
#include <arch-kerninc/SaveArea.h>
#include <arch-kerninc/Process.h>
#ifdef EROS_HAVE_ARCH_REGS
#include <eros/machine/archregs.h>
#endif
#ifdef OPTION_PSEUDO_REGS
#include <eros/machine/pseudoregs.h>
#endif
#include <eros/machine/traceregs.h>

#include <kerninc/Key.h>
#include <kerninc/StallQueue.h>

struct PTE;
struct Activity;
struct Invocation;
struct SegWalk;

#endif /* __ASSEMBLER__ */

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

/* Bits in kernelFlags are defined here. */

/* KF_IoPriv says whether the process can execute I/O instructions.
   It is set iff the process has the DevicePrivs key in ProcIoSpace.
   The information is duplicated here to speed up traps for
   I/O operations. */
#define KF_IoPriv 0x01

/* KF_DDBINV and KF_DDBTRAP are a temporary expedient until
   we are able to get a minimal per-process debugger running. */
#define KF_DDBINV    0x40 /* process invocations should be
			     reported by DDB */
#define KF_DDBTRAP   0x80 /* process traps should be reported by DDB */

#ifndef __ASSEMBLER__

 /* Hazards are architecture-dependent and should perhaps be moved. */
enum Hazards {
  hz_Malformed    = 0x01u,
  hz_DomRoot      = 0x02u,
  hz_KeyRegs      = 0x04u,
  // unused         0x08u,
  hz_Schedule     = 0x10u,
#ifdef EROS_HAVE_FPU
  hz_FloatRegs    = 0x20u,
  hz_NumericsUnit = 0x40u,	/* need to load FPU */
#endif
  hz_SingleStep   = 0x80u 	/* context may have a live activity */
};

/* Use of this procedure is a warning that we are relying on the kludgy
 * representation pun of the first 4 fields of the Process structure. */
INLINE ObjectHeader *
proc_ToObj(Process * proc)
{
  return (ObjectHeader *)proc;
}

struct Process {
/* Kludge alert: The first 4 fields here match exactly the corresponding
 * fields of ObjectHeader (representation pun).
 * Some day, Process will be a first-class object type,
 * and it will include an ObjectHeader. */
  uint8_t obType;               // not used

  uint8_t objAge;

  uint16_t userPin;

  KeyRing keyRing;

  StallQueue    stallQ;  /* procs waiting for this to be available */
  
  Node      	*procRoot;

  /* kernelFlags is at an address that is a multiple of 4,
     to save a cycle in ARM SWI_DisableIRQ_Handler. */
  uint8_t kernelFlags;

  bool isUserContext;

  /* hazards are reasons you cannot run.  They generally devolve to
   * being malformed or needing some functional unit loaded before you
   * can make progress.
   */
  uint8_t hazards;

  uint8_t runState;

  /* Bits in processFlags are defined in Process.idl. */
  uint8_t processFlags;

  struct ReadyQueue *readyQ;   /* ready queue info for this process */

  /* Processor we last ran on, to recover FP regs
   * Processor     *lastProcessor;
   */

  /* At most one activity in a given context at a time, which will be
   * pointed to by the context:
   */

  struct Activity   *curActivity;

  ProcMD md;	/* Machine-Dependent stuff */

  savearea_t	    trapFrame;
#ifdef OPTION_PSEUDO_REGS
  pseudoregs_t      pseudoRegs;
#endif
  traceregs_t       traceRegs;

  /* This should immediately follow fixRegs: */
  Key               keyReg[EROS_PROCESS_KEYREGS];

  uint32_t          faultCode;
  uint32_t          faultInfo;
  
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

#ifdef EROS_HAVE_FPU
/* FPU support: */
extern Process*   proc_fpuOwner;	/* FIX: This is not SMP-feasible. */
#endif

extern Process *proc_ContextCache;
/* End static members */
  
/* Former member functions of KernProcess */
typedef struct KernProcess KernProcess;

Process *kproc_Init(const char * name,
		    Priority prio,
                    struct ReadyQueue *rq,
		    void (*pc)(void),
		    uint32_t *StackBottom, uint32_t *StackTop);

#ifndef NDEBUG
INLINE const Process *
keyR_ToProc(const KeyRing * kr)
{
  return container_of(kr, const Process, keyRing);
}
#endif

struct PTE *
proc_BuildMapping(Process* p, ula_t ula, bool forWriting, bool prompt);


#ifdef EROS_HAVE_FPU
/* FPU support: */

void proc_SaveFPU(Process* thisPtr);
void proc_LoadFPU(Process* thisPtr);
void proc_DumpFloatRegs(Process* thisPtr);
#endif

#ifndef NDEBUG
bool ValidCtxtPtr(const Process * ctxt);
bool ValidCtxtKeyRingPtr(const KeyRing* kr);
Process * proc_ValidKeyReg(const Key * pKey);
#endif

void proc_DoPrepare(Process* thisPtr);

/* MUST NOT yield if IsRunnable() would return true. (Why not? -CRL) */
// May Yield.
INLINE void 
proc_Prepare(Process* thisPtr)
{
  /* Anything that invalidates the context will set a hazard,
   * so this is a quick test, which is useful for fast reload.
   */
  if (thisPtr->hazards)
    proc_DoPrepare(thisPtr);
}

INLINE bool 
proc_IsRunnable(Process* thisPtr)
{
  return ! thisPtr->hazards;
}

INLINE bool 
proc_IsNotRunnable(Process* thisPtr)
{
  return thisPtr->hazards;
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

void proc_DumpFixRegs(Process* thisPtr);
#ifdef OPTION_PSEUDO_REGS
void proc_DumpPseudoRegs(Process* thisPtr);
#endif

/* Generic keeper invoker: */
void proc_InvokeMyKeeper(Process* thisPtr, uint32_t oc,
                         uint32_t warg1,
                         uint32_t warg2,
                         uint32_t warg3,
                         Key *keeperKey, Key* keyArg2,
                         uint8_t *data, uint32_t len);

void
 proc_InvokeSegmentKeeper(Process * thisPtr, struct SegWalk *,
                          bool invokeProcessKeeperOK,
                          uva_t vaddr );

void proc_InvokeProcessKeeper(Process* thisPtr);

void proc_BuildResumeKey(Process* thisPtr, Key* resumeKey /*@ not null @*/);

void proc_DoKeyInvocation(Process* thisPtr);

struct Invocation;

void proc_SetupEntryBlock(Process* thisPtr,
                          struct Invocation * inv /*@ not null @*/);

void proc_SetupExitBlock(Process* thisPtr,
                         struct Invocation * inv /*@ not null @*/);

void proc_DeliverGateResult(Process* thisPtr,
                            struct Invocation* inv /*@ not null @*/,
                            bool wantFault);

void proc_SetupExitString(Process* thisPtr, struct Invocation* inv /*@ not null @*/, uint32_t bound);

void proc_DeliverResult(Process* thisPtr, struct Invocation* inv /*@ not null @*/);

void proc_FlushFixRegs(Process* thisPtr);
#ifdef EROS_HAVE_FPU
void proc_LoadFloatRegs(Process* thisPtr);
void proc_FlushFloatRegs(Process* thisPtr);
#endif
void proc_FlushKeyRegs(Process* thisPtr);

#ifdef OPTION_SMALL_SPACES
void proc_WriteDisableSmallSpaces();
#endif

bool check_Contexts(const char *);

void proc_InitProcessMD(Process * proc);	//machine dependent
void proc_AllocUserContexts(void);
Process *proc_allocate(bool isUser);
void proc_Init_MD(Process * p, bool isUser); /* machine dependent */
void proc_LoadSingleStep(Process * thisPtr);
void proc_LoadFixRegs(Process* thisPtr);
void proc_ValidateRegValues(Process* thisPtr);
void proc_SetFault(Process * thisPtr, uint32_t code, uint32_t info);

void proc_FlushProcessSlot(Process * thisPtr, unsigned int whichKey);

#ifdef OPTION_DDB
void proc_WriteBackKeySlot(Process* thisPtr, uint32_t whichKey);
#endif

void proc_SyncActivity(Process* thisPtr);

void proc_Unload(Process* thisPtr);

void proc_GetCommonRegs32(Process * thisPtr,
       struct capros_Process_CommonRegisters32 * regs);
void proc_SetCommonRegs32(Process * thisPtr,
       struct capros_Process_CommonRegisters32 * regs);
void proc_SetCommonRegs32MD(Process * thisPtr,
  struct capros_Process_CommonRegisters32 * regs);	// machine dependent

struct Invocation;
void ProcessKeyCommon(struct Invocation * inv, Process *);

#endif /* __ASSEMBLER__ */

#endif /* __PROCESS_H__ */
