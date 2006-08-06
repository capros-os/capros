/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/Node.h>
#include <arch-kerninc/KernTune.h>
#include <kerninc/Depend.h>
#include <kerninc/Activity.h>
#include <kerninc/util.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Machine.h>
#include <kerninc/IRQ.h>
#include <kerninc/Invocation.h>
#include <kerninc/Check.h>
#include <kerninc/Process.h>
#include <kerninc/CpuReserve.h>
#include <arch-kerninc/Process.h>
#include <arch-kerninc/PTE.h>
#include <eros/Invoke.h>
#include <eros/ProcessState.h>
#include <eros/SegKeeperInfo.h>
#include <eros/Registers.h>
#include <eros/arch/arm/Registers.h>
#include <eros/ProcessKey.h>
#include <kerninc/KernStats.h>
#include <kerninc/Invocation.h>
#include "arm.h"

#include "gen.REGMOVE.h"
/* #define MSGDEBUG
 * #define RESUMEDEBUG
 * #define XLATEDEBUG
 */
void proc_ResetMappingTable(Process * p);
void resume_process(Process * p) NORETURN; 

Process * proc_ContextCache = NULL;
Process * proc_ContextCacheRegion = NULL;

#ifdef OPTION_SMALL_SPACES
PTE *proc_smallSpaces = 0;
#endif

void 
proc_AllocUserContexts()
{
  int i = 0;
  Process * contextCache = proc_ContextCacheRegion;

  if (contextCache == NULL)
    contextCache = MALLOC(Process, KTUNE_NCONTEXT);
  else {
    printf("Context caches already allocated at 0x%08x\n", contextCache);
  }

  /* This is to make sure that the name field does not overflow: */
  assert (KTUNE_NCONTEXT < 1000);

  for (i = 0; i < KTUNE_NCONTEXT; i++) {
    int k;
    Process *p = &contextCache[i];
    keyR_ResetRing(&p->keyRing);
    sq_Init(&p->stallQ);

    for (k = 0; k < EROS_PROCESS_KEYREGS; k++)
      keyBits_InitToVoid(&p->keyReg[k]);

    p->procRoot = 0;
    p->keysNode = 0;
    p->isUserContext = true;		/* until proven otherwise */
    /*p->priority = pr_Never;*/
    p->faultCode = FC_NoFault;
    p->faultInfo = 0;
    p->processFlags = 0;
    p->saveArea = 0;
    p->hazards = 0u;	/* deriver should change this! */
    p->curActivity = 0;
    
    p->name[0] = 'u';
    p->name[1] = 's';
    p->name[2] = 'e';
    p->name[3] = 'r';
    p->name[4] = '0' + (i / 100);
    p->name[5] = '0' + ((i % 100) / 10);
    p->name[6] = '0' + (i % 10);
    p->name[7] = 0;

  }

  proc_ContextCache = contextCache;

  check_Contexts("Initial check");

  printf("Allocated User Contexts: 0x%x at 0x%08x\n",
		 sizeof(Process[KTUNE_NCONTEXT]),
		 proc_ContextCache);
}

/* FIX: It is unfortunate that some of these checks require !NDEBUG.
 * Should they?
 */
/* The string c is for diagnostic identification only. */
bool
check_Contexts(const char *c)
{
  int i = 0;
#ifndef NDEBUG
  unsigned k = 0;
#endif
  bool result = true;
  
  irq_DISABLE();

  /* It is possible for this to get called from interrupt handlers
   * before the context cache has been allocated.
   */
  if (proc_ContextCache) {
    for (i = 0; i < KTUNE_NCONTEXT; i++) {
      Process *p = &proc_ContextCache[i];
    
#ifndef NDEBUG
      if (keyR_IsValid(&p->keyRing, p) == false) {
	result = false;
	break;
      }
#endif
    
#ifndef NDEBUG
      for (k = 0; k < EROS_PROCESS_KEYREGS; k++) {
	if (key_IsValid(&p->keyReg[0]) == false) {
	  result = false;
	  break;
	}
      }
#endif

      if (p->procRoot
          && p->procRoot->node_ObjHdr.obType == ot_NtFreeFrame ) {
	dprintf(true, "Context 0x%08x has free process root 0x%08x\n",
			p, p->procRoot);
	result = false;
      }

      if (p->keysNode
          && p->keysNode->node_ObjHdr.obType == ot_NtFreeFrame ) {
	dprintf(true, "Context 0x%08x has free keys node 0x%08x\n",
			p, p->keysNode);
	result = false;
      }

      if (result == false)
	break;
    }
  }

  irq_ENABLE();

  return result;
}

void
proc_Unload(Process* thisPtr)
{
  /* It might already be unloaded: */
  if (thisPtr->procRoot == 0)
    return;
  
#if defined(DBG_WILD_PTR)
  if (dbg_wild_ptr)
    if (check_Contexts("before unload") == false)
      halt('a');
#endif

#if 1
printf("Unimplemented proc_Unload");
#else
  if (thisPtr->curActivity) {
    proc_SyncActivity(thisPtr);
    act_ZapContext(thisPtr->curActivity);

  }

#if defined(DBG_WILD_PTR)
  if (dbg_wild_ptr)
    if (check_Contexts("after syncactivity") == false)
      halt('b');
#endif
  
  thisPtr->MappingTable = 0;
  thisPtr->saveArea = 0;
  thisPtr->curActivity = 0;

  if ((thisPtr->hazards & hz_KeyRegs) == 0)
    proc_FlushKeyRegs(thisPtr);

  if ((thisPtr->hazards & hz_DomRoot) == 0)
    proc_FlushFixRegs(thisPtr);

  
  if ( keyBits_IsHazard(node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace)) ) {
    Depend_InvalidateKey(node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace));
    thisPtr->hazards |= hz_AddrSpace;
  }

  thisPtr->procRoot->node_ObjHdr.prep_u.context = 0;
  thisPtr->procRoot->node_ObjHdr.obType = ot_NtUnprepared;

  assert(thisPtr->procRoot);
  
  keyR_UnprepareAll(&thisPtr->keyRing);
  thisPtr->hazards = 0;
  thisPtr->procRoot = 0;
  thisPtr->saveArea = 0;

  dprintf(false,  "Unload of context 0x%08x complete\n", thisPtr);

  sq_WakeAll(&thisPtr->stallQ, false);
#endif
}

/* Simple round-robin policy for now:  */
Process *
proc_allocate(bool isUser)
{
  static uint32_t nextClobber = 0;
  Process* p = 0;

  while (p == 0) {
    p = &proc_ContextCache[nextClobber++];
    if (nextClobber >= KTUNE_NCONTEXT)
      nextClobber = 0;

    if (p == act_Current()->context) {	/* can't use current Process */
      p = 0;
      continue;
    }

    if (inv_IsActive(&inv) && p == inv.invokee) {
      p = 0;
      continue;
    }
    
    if (p->isUserContext == false) {	/* can't steal kernel Processes */
      p = 0;
      continue;

    }
    
    if (p->curActivity &&
	p->curActivity->state == act_Running)
      p = 0;
  }
  /* Use Process p. */

  /* wipe out current contents, if any */
  proc_Unload(p);

#if 0
  printf("  unloaded\n");
#endif
  
  assert(p->procRoot == 0
         || p->procRoot->node_ObjHdr.obType == ot_NtProcessRoot);

  p->procRoot = 0;		/* for kernel contexts */
  p->saveArea = 0;		/* make sure not runnable! */
  p->faultCode = FC_NoFault;
  p->faultInfo = 0;
  p->processFlags = 0;
  p->isUserContext = isUser;
  /* FIX: what to do about runState? */

  p->curActivity = 0;
  proc_ResetMappingTable(p);

  return p;
}

/* Initialize a privileged Process. */
Process *
kproc_Init(
           const char *myName,
           Activity* theActivity,
	   Priority prio,
           ReadyQueue *rq,
           void (*pc)(),
           uint32_t * stkBottom, uint32_t * stkTop)
{
  int i = 0;
  Process *p = proc_allocate(false);

  p->hazards = 0;
  /*p->priority = prio;*/
  p->readyQ = rq;
  /*
    p->prioQ.queue = rq->queue;
    p->prioQ.mask = rq->mask;
    p->prioQ.other = rq->other;
    p->prioQ.enqueue = rq->enqueue;
  */
  
  for (i = 0; i < 8; i++)
    p->name[i] = myName[i];
  p->name[7] = 0;

  p->curActivity = theActivity;
  p->runState = RS_Running;

  memset(&p->trapFrame, 0, sizeof(p->trapFrame));
  
  /* Initialize the per-activity save area so that we can schedule this
   * activity.  When the activity is initiated by resume_process() for the
   * first time, it will execute the first instruction of its Start
   * routine.
   */

  proc_ResetMappingTable(p);	/* kernel access only */
  p->trapFrame.CPSR = 0x1f;	/* System mode */
  p->trapFrame.r15 = (uint32_t) pc;
  p->nextPC = (uint32_t) pc;

  /* YES the first of these is dead code.  It suppresses the unused
   * argument warning if DBG_WILD_PTR is not enabled.
   */
  (void) stkBottom;
  p->trapFrame.r13 = (uint32_t) stkTop;
    
  p->saveArea = &p->trapFrame;

  return p;
}

#ifndef ASM_VALIDATE_STRINGS /* This is the case. */
void 
proc_SetupEntryString(Process* thisPtr, Invocation* inv /*@ not null @*/)
{
#ifndef OPTION_PURE_ENTRY_STRINGS
  if (inv->entry.len == 0)
    return;
#endif

  /* Get user's snd_addr from his Message structure. */
  ula_t ula;	/* user's modified virtual address */
  ula_t snd_data;
  ula = thisPtr->trapFrame.r0 + thisPtr->md.pid;
  if (! LoadWordFromUserSpace(ula + offsetof(Message, snd_data),
                              & snd_data)) {
    /* Try to map the address. */
    /* This if should be a procedure ... */
    printf("proc_SetupEntryString fault, unimplemented\n");
  }

  snd_data += thisPtr->md.pid;
  ula = snd_data;
  ula_t ulaTop = ula + inv->entry.len;	/* MVA of last byte +1 */

  /* Ensure each page of the string is mapped. */
  ula &= ~EROS_PAGE_MASK;

  while (ula < ulaTop) {
    /* Fastest way to see if it is mapped is to try to fetch it.
       We don't use the value fetched. */
    uint32_t unused;
    if (! LoadWordFromUserSpace(ula,
                                & unused)) {
      /* Try to map the address. */
      /* This if should be a procedure ... */
      printf("proc_SetupEntryString fault, unimplemented\n");
    }

    ula += EROS_PAGE_SIZE;
  }

  inv->entry.data = (uint8_t *) snd_data;
}
#endif /* ASM_VALIDATE_STRINGS */

void 
proc_SetupExitString(Process* thisPtr, Invocation* inv /*@ not null @*/,
                     uint32_t bound)
{
#ifndef OPTION_PURE_EXIT_STRINGS
  if (inv->validLen == 0)
    return;
#endif
  printf("proc_SetupExitString unimplemented!\n");
}

extern void resume_from_kernel_interrupt(savearea_t *) NORETURN;

void
DumpFixRegs(const savearea_t * sa)
{
  printf("CPSR 0x%08x\n", sa->CPSR);
  printf("r0-r3   0x%08x 0x%08x 0x%08x 0x%08x\n",
         sa->r0, sa->r1, sa->r2, sa->r3);
  printf("r4-r7   0x%08x 0x%08x 0x%08x 0x%08x\n",
         sa->r4, sa->r5, sa->r6, sa->r7);
  printf("r8-r11  0x%08x 0x%08x 0x%08x 0x%08x\n",
         sa->r8, sa->r9, sa->r10, sa->r11);
  printf("r12-r15 0x%08x 0x%08x 0x%08x 0x%08x\n",
         sa->r12, sa->r13, sa->r14, sa->r15);
}

void 
proc_DumpFixRegs(Process* thisPtr)
{
  if (thisPtr->saveArea == 0)
    printf("Note: context is NOT runnable\n");
  printf("Process = 0x%08x  PID   = 0x%08x  nextPC=0x%08x\n",
	 thisPtr, thisPtr->md.pid, thisPtr->nextPC);
  DumpFixRegs(&thisPtr->trapFrame); 
}

void 
proc_Load(Node* procRoot)
{
  Process *p = proc_allocate(true);

  p->hazards =
    hz_DomRoot | hz_KeyRegs | hz_Schedule | hz_AddrSpace;

  assert(procRoot);

  p->procRoot = procRoot;
  
  procRoot->node_ObjHdr.prep_u.context = p;
  procRoot->node_ObjHdr.obType = ot_NtProcessRoot;
}

/* ValidateRegValues() -- runs last to validate that the loaded context
 * will not violate protection rules if it is run.  This routine must
 * be careful not to overwrite an existing fault condition. To avoid
 * this, it must only alter the fault code value if there isn't
 * already a fault code.  Basically, think of this as meaning that bad
 * register values are the lowest priority fault that will be reported
 * to the user.
 */
void
proc_ValidateRegValues(Process* thisPtr)
{
  const uint32_t userCPSRBits = 0xff000000 | MASK_CPSR_Thumb;
		/* bits that the user can play with */
  
  if (thisPtr->processFlags & PF_Faulted)
    return;

  if (thisPtr->isUserContext) {
    /* Rather than force the programmer to set the correct CPSR bits,
       we just set them correctly here. */
    thisPtr->trapFrame.CPSR = (thisPtr->trapFrame.CPSR & userCPSRBits)
                              | CPSRMode_User;
  } else {
    /* Privileged processes might disable IRQ, but we will never see it,
       because the process won't be preempted until IRQ is reenabled. */
    /* Check this as an assertion rather than a process fault.
       The kernel depends on it being correct and fail-fast is
       easier to diagnose. */
    assert((thisPtr->trapFrame.CPSR & ~userCPSRBits) == CPSRMode_System);
  }
}

/* Both loads the register values and validates that the process root
 * is well-formed.
 */
static void 
proc_LoadFixRegs(Process* thisPtr)
{
  uint32_t k;
  uint8_t *rootkey0 = 0;
  assert(thisPtr->hazards & hz_DomRoot);

  assert(thisPtr->procRoot);
  node_MakeDirty(thisPtr->procRoot);

#ifdef ProcAltMsgBuf
#error "Type checks need revision"
#endif
  
  /* Ensure slots containing fixed regs are number keys. */
  for (k = ProcFirstRootRegSlot; k <= ProcLastRootRegSlot; k++) {
    if ( keyBits_IsType(node_GetKeyAtSlot(thisPtr->procRoot, k), KKT_Number) == false) {
      proc_SetMalformed(thisPtr);
      return;
    }

    assert ( keyBits_IsHazard(node_GetKeyAtSlot(thisPtr->procRoot, k)) == false );
  }

  thisPtr->stats.pfCount = 0;

  rootkey0 = (uint8_t *) node_GetKeyAtSlot(thisPtr->procRoot, 0);

  LOAD_FIX_REGS;
  
  for (k = ProcFirstRootRegSlot; k <= ProcLastRootRegSlot; k++)
    keyBits_SetRwHazard(node_GetKeyAtSlot(thisPtr->procRoot, k));

  thisPtr->hazards &= ~hz_DomRoot;

#if 0
  dprintf(true, "Process root oid=0x%x to ctxt 0x%08x, eip 0x%08x hz 0x%08x\n",
		  (uint32_t) procRoot->oid, this, trapFrame.EIP, hazards);
#endif
}

/* The DoPrepare() logic has changed, now that we have merged the
 * process prep logic into it...
 */
void 
proc_DoPrepare(Process* thisPtr)
{
  bool check_disjoint;
  assert(thisPtr->procRoot);
  assert (thisPtr->isUserContext);

  objH_TransLock(DOWNCAST(thisPtr->procRoot, ObjectHeader));
  if (thisPtr->keysNode)
    objH_TransLock(DOWNCAST(thisPtr->keysNode, ObjectHeader));
  
  thisPtr->hazards &= ~hz_Malformed;	/* until proven otherwise */
  
  check_disjoint
    = (thisPtr->hazards & (hz_DomRoot | hz_FloatRegs | hz_KeyRegs)); 

#if 0
  printf("Enter Process::DoPrepare()\n");
#endif
  /* The order in which these are tested is important, because
   * sometimes satisfying one condition imposes another (e.g. floating
   * point bit set in the eflags register)
   */

  if (thisPtr->hazards & hz_DomRoot)
    proc_LoadFixRegs(thisPtr);

  if (thisPtr->faultCode == FC_MalformedProcess) {
    assert (thisPtr->processFlags & PF_Faulted);
    return;
  }
  
  if (thisPtr->hazards & hz_KeyRegs)
    proc_LoadKeyRegs(thisPtr);

  if (check_disjoint) {
    if ( thisPtr->procRoot == thisPtr->keysNode ) {
      proc_SetMalformed(thisPtr);
    }
  }
  
  if (thisPtr->faultCode == FC_MalformedProcess) {
    assert (thisPtr->processFlags & PF_Faulted);
    return;
  }
  
  if (thisPtr->hazards & hz_Schedule) {
    /* FIX: someday deal with schedule keys! */
    Priority pr;
    Key* schedKey /*@ not null @*/ = node_GetKeyAtSlot(thisPtr->procRoot, ProcSched);

    assert(keyBits_IsHazard(schedKey) == false);
    
    keyBits_SetWrHazard(schedKey);

    if (schedKey->keyData & (1u<<pr_Reserve)) {
      /* this is a reserve key */
      int ndx = schedKey->keyData;
      Reserve *r = 0;

      ndx &= ~(1u<<pr_Reserve);
      r = &res_ReserveTable[ndx];
      thisPtr->readyQ = &r->readyQ;
      r->isActive = true;
      printf("set real time key index = %d\n", r->index);
    }
    /* this is a priority key */
    else {
      pr = min(schedKey->keyData, pr_High);
      thisPtr->readyQ = dispatchQueues[pr];
      if (pr == pr_Reserve) {
        Reserve *r = res_GetNextReserve();
        thisPtr->readyQ= &r->readyQ;
      }
    }

    thisPtr->hazards &= ~hz_Schedule;

    /* If context is presently occupied by a activity, need to update the
       readyQ pointer in that activity: */
    if (thisPtr->curActivity) {
      Activity *t = thisPtr->curActivity;

      assert(t->context == thisPtr);
      t->readyQ = thisPtr->readyQ;

      switch(t->state) {
      case act_Running:
        act_ForceResched();
        act_Wakeup(t);
        break;
      case act_Ready:
        /* need to move the activity to the proper ready Q: */
        act_Dequeue(t);
        act_ForceResched();
        act_Wakeup(t);
        break;
      case act_Free:
        assert("Rescheduling free activity");
        break;
      default:
        /* stalled needs no special action. */
        /* IoCompleted should be gone but isn't. */
	break;
      }
    }
  }

  if (thisPtr->hazards & hz_SingleStep) {
    fatal("Single Step not implemented on ARM.\n");
  }

  proc_ValidateRegValues(thisPtr);
  
  /* Change: It is now okay for the context to carry a fault code
   * and prepare correctly.
   */

  thisPtr->saveArea = &thisPtr->trapFrame;
  sq_WakeAll(&thisPtr->stallQ, false);
}

void 
proc_Resume(void)
{
  Process * thisPtr = act_CurContext();
#ifndef NDEBUG
  if ( thisPtr->curActivity != act_Current() )
    fatal("Context 0x%08x (%d) not for current activity 0x%08x (%d)\n",
	       thisPtr, thisPtr - proc_ContextCache,
		  act_Current(),
		  act_Current() - act_ActivityTable);

  if ( act_CurContext() != thisPtr )
    fatal("Activity context 0x%08x not me 0x%08x\n",
	       act_CurContext(), thisPtr);

#endif

  /* must have kernel access */
  assert(thisPtr->md.firstLevelMappingTable != 0);
  assert((thisPtr->md.dacr & 0x3) == 1);
  
#if 1
if (thisPtr->trapFrame.r15 & 0x3)
  printf("Resume user process 0x%08x pc=0x%08x pid=0x%08x\n", thisPtr,
         thisPtr->trapFrame.r15, thisPtr->md.pid);
#endif
  
  /* IRQ must be disabled here. */
  assert( irq_DISABLE_DEPTH() == 1 );

  /* While a process is running, irq_DisableDepth is zero.
     After all, IRQ is enabled. 
     irq_DisableDepth is incremented again on an exception. 
     But no one should be looking at that, except maybe privileged
     processes. */
  irq_DisableDepth--;
  resume_process(thisPtr);	/* continue in assembly code */
}
