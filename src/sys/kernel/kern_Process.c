/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <kerninc/Process.h>
#include <kerninc/Key.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <kerninc/Depend.h>
#include <kerninc/Invocation.h>
#include <kerninc/CpuReserve.h>
#include <kerninc/Key-inline.h>
#include <kerninc/IRQ.h>

Process * proc_ContextCache = NULL;
Process * proc_ContextCacheRegion = NULL;

void
proc_ClearActivity(Process * proc)
{
  Activity * act = proc->curActivity;
  assert(act);

  act_Dequeue(act);
  act_SetContext(act, NULL);
  proc_Deactivate(proc);
  act_DeleteActivity(act);
}

void 
proc_SetFault(Process * thisPtr, uint32_t code, uint32_t info)
{
  assert(code);

  thisPtr->faultCode = code;
  thisPtr->faultInfo = info;
  thisPtr->processFlags |= capros_Process_PF_FaultToProcessKeeper;
  
#ifdef OPTION_DDB
  if (thisPtr->kernelFlags & KF_DDBTRAP)
    dprintf(true, "Process 0x%08x has trap code set\n", thisPtr);
#endif
#if 1	// if failing fast for debugging
  // Halt if no keeper:
  assert(keyBits_IsType(& thisPtr->procRoot->slot[ProcKeeper], KKT_Start));
#endif
}

void 
proc_FlushKeyRegs(Process * thisPtr)
{
  uint32_t k;
  assert(thisPtr->procRoot);
  assert(thisPtr->keysNode);
  assert((thisPtr->hazards & hz_KeyRegs) == 0);
  assert(objH_IsDirty(node_ToObj(thisPtr->keysNode)));
  assert(node_Validate(thisPtr->keysNode));

#if 0
  printf("Flushing key regs on ctxt=0x%08x\n", this);
  if (inv.IsActive() && inv.invokee == this)
    dprintf(true,"THAT WAS INVOKEE!\n");
#endif

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    Key * nodeSlot = node_GetKeyAtSlot(thisPtr->keysNode, k);
    Key * procSlot = &thisPtr->keyReg[k];
    assert(! keyBits_IsHazard(nodeSlot));
    key_NH_Set(nodeSlot, procSlot);

    /* Not hazarded because key register */
    key_NH_SetToVoid(procSlot);

    /* We know that the context structure key registers are unhazarded
     * and unlinked by virtue of the fact that they are unloaded.
     */
  }

  thisPtr->keysNode->node_ObjHdr.obType = ot_NtUnprepared;
  thisPtr->keysNode = 0;

  thisPtr->hazards |= hz_KeyRegs;

  keyBits_UnHazard(&thisPtr->procRoot->slot[ProcGenKeys]);
}

/* May Yield. */
static void 
proc_LoadKeyRegs(Process * thisPtr)
{
  uint32_t k;
  assert (thisPtr->hazards & hz_KeyRegs);
  Key * genKeysKey = &thisPtr->procRoot->slot[ProcGenKeys];
  assert(! keyBits_IsHazard(genKeysKey));

  key_Prepare(genKeysKey);

  if (! keyBits_IsType(genKeysKey, KKT_Node)) {
    proc_SetMalformed(thisPtr);
    return;
  }

  Node * kn = objH_ToNode(genKeysKey->u.ok.pObj);
  assert(objH_IsUserPinned(node_ToObj(kn)));

  assert ( node_Validate(kn) );

  if (kn->node_ObjHdr.obType != ot_NtUnprepared
        /* Strangely prepared? Process creator shouldn't allow this. */
      || kn == thisPtr->procRoot) {
    proc_SetMalformed(thisPtr);
    return;
  }

  node_EnsureWritable(kn);

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    Key * procSlot = &thisPtr->keyReg[k];
    Key * nodeSlot = node_GetKeyAtSlot(kn, k);
#ifndef NDEBUG
    if (keyBits_IsHazard(nodeSlot))
      dprintf(true, "Key register slot %d is hazarded in node 0x%08x%08x\n",
		      k,
		      (uint32_t) (kn->node_ObjHdr.oid >> 32),
                      (uint32_t) kn->node_ObjHdr.oid);
#endif

    /* The context structure key registers are unhazarded
     * and unprepared by virtue of the fact that they are unloaded. */
    assert(! keyBits_IsHazard(procSlot));
    assert(keyBits_IsUnprepared(procSlot));

    if (k == 0)
      key_NH_SetToVoid(procSlot);	/* key register 0 is always void */
    else
      key_NH_Set(procSlot, nodeSlot);

    /* We used to call keyBits_SetRwHazard(nodeSlot),
    so reads and writes to the keys node would get the right answer.
    That resulted in extra logic to avoid unloading the key registers
    when a key in that node is merely rescinded.
    Now, we no longer make those slots hazarded.
    If the process creator behaves properly, no one should
    read or write the keys node directly after a process is built.
    If someone does, a read may get stale data or a write may not take effect.
    */
  }
  
  /* Node is now known to be valid... */
  kn->node_ObjHdr.obType = ot_NtKeyRegs;
  kn->node_ObjHdr.prep_u.context = thisPtr;
  thisPtr->keysNode = kn;

  keyBits_SetWrHazard(genKeysKey);

  assert(thisPtr->keysNode);

  thisPtr->hazards &= ~hz_KeyRegs;
}

/* Rewrite the process key back to our current activity,
   prior to clearing Activity.context. */
void
proc_SyncActivity(Process * thisPtr)
{
  assert(thisPtr->curActivity);
  assert(thisPtr->procRoot);
  assert(thisPtr->curActivity->context == thisPtr);
  
  Key * procKey = &thisPtr->curActivity->processKey;

  assert (keyBits_IsHazard(procKey) == false);
  /* Not hazarded because activity key */
  key_NH_Unchain(procKey);

  key_SetToProcess(procKey, thisPtr, KKT_Process, 0);
}

void
proc_SetCommonRegs32(Process * thisPtr,
  struct capros_Process_CommonRegisters32 * regs)
{
  assert (proc_IsRunnable(thisPtr));

  assert(! (thisPtr->hazards & hz_DomRoot));
  
  /* Ignore len, architecture */
  proc_SetCommonRegs32MD(thisPtr, regs);

  thisPtr->processFlags = regs->procFlags;

  if (regs->faultCode == capros_Process_FC_NoFault) {
    proc_ClearFault(thisPtr);
  } else {
    thisPtr->faultCode        = regs->faultCode;
    thisPtr->faultInfo        = regs->faultInfo;
  }
}

void
proc_Unload(Process * thisPtr)
{
  /* It might already be unloaded: */
  if (thisPtr->procRoot == 0)
    goto exit;
  
#if defined(DBG_WILD_PTR)
  if (dbg_wild_ptr)
    if (check_Contexts("before unload") == false)
      halt('a');
#endif

#ifdef OPTION_DDB
    if (thisPtr->kernelFlags & KF_DDBTRAP)
      dprintf(true, "Process %#x is being unloaded\n", thisPtr);
#endif

  if (thisPtr->curActivity) {
    proc_SyncActivity(thisPtr);
    act_SetContext(thisPtr->curActivity, NULL);
    proc_Deactivate(thisPtr);
  }

#if defined(DBG_WILD_PTR)
  if (dbg_wild_ptr)
    if (check_Contexts("after syncactivity") == false)
      halt('b');
#endif
  
#ifdef EROS_HAVE_FPU
  if ( (thisPtr->hazards & hz_FloatRegs) == 0)
    proc_FlushFloatRegs(thisPtr);
#endif

  if ((thisPtr->hazards & hz_KeyRegs) == 0)
    proc_FlushKeyRegs(thisPtr);

  if ((thisPtr->hazards & hz_DomRoot) == 0)
    proc_FlushFixRegs(thisPtr);

  if ( keyBits_IsHazard(node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace)) ) {
    Depend_InvalidateKey(node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace));
  }

  assert(thisPtr->procRoot);

  thisPtr->procRoot->node_ObjHdr.prep_u.context = 0;	// for safety
  thisPtr->procRoot->node_ObjHdr.obType = ot_NtUnprepared;
  
  // An unloaded process has an empty keyRing:
  keyR_UnprepareAll(&thisPtr->keyRing);
  thisPtr->hazards = 0;
  thisPtr->procRoot = 0;

  // dprintf(false,  "Unload of context 0x%08x complete\n", thisPtr);

  sq_WakeAll(&thisPtr->stallQ, false);
exit: ;
  assert(! thisPtr->curActivity);
}

#define age_proc_Steal 2

Process *
proc_allocate(bool isUser)
{
  static uint32_t nextClobber = 0;
  Process * p;

  /* Simple round-robin policy for now:  */
  for (;;) {
    p = &proc_ContextCache[nextClobber++];
    if (nextClobber >= KTUNE_NCONTEXT)
      nextClobber = 0;

    if (objH_IsUserPinned(proc_ToObj(p)))
      continue;
    
    if (p->curActivity &&
	p->curActivity->state == act_Running) {
      objH_SetReferenced(proc_ToObj(p));
      continue; 
    }

    /* We can't use the current Process.
     * (We don't pin it for performance reasons.) */
    // Given the above, is the below needed?
    if (p == proc_Current())
      continue;
    
    if (proc_IsKernel(p))	/* can't steal kernel Processes */
      continue;

    if (proc_ToObj(p)->objAge < age_proc_Steal) {
      proc_ToObj(p)->objAge++;	// age it
      continue;
    }

    break;	// Use this process
  }
  /* Use Process p. */

  assert(! (inv_IsActive(&inv) && p == inv.invokee));
	// because it should be user pinned

  /* wipe out current contents, if any */
  proc_Unload(p);

#if 0
  printf("  unloaded\n");
#endif
  
  assert(p->procRoot == 0
         || p->procRoot->node_ObjHdr.obType == ot_NtProcessRoot);

  p->procRoot = 0;		/* for kernel contexts */
  p->faultCode = capros_Process_FC_NoFault;
  p->faultInfo = 0;
  p->kernelFlags = 0;
  // Set hazards to the default for user. Will be overwritten if kernel proc.
  p->hazards = hz_DomRoot | hz_KeyRegs | hz_Schedule;
  p->processFlags = 0;
  p->isUserContext = isUser;
  /* FIX: what to do about runState? */

  proc_Init_MD(p, isUser);

  return p;
}

// May Yield.
void 
proc_DoPrepare(Process * thisPtr)
{
  bool check_disjoint;
  assert(thisPtr->procRoot);
  assert (thisPtr->isUserContext);

  objH_TransLock(proc_ToObj(thisPtr));
  
  thisPtr->hazards &= ~hz_Malformed;	/* until proven otherwise */
  
  check_disjoint
    = (thisPtr->hazards & (hz_DomRoot | hz_KeyRegs
#ifdef EROS_HAVE_FPU
                           | hz_FloatRegs
#endif
                          )); 

#if 0
  dprintf(true,"Enter proc_DoPrepare()\n");
#endif
  /* The order in which these are tested is important, because
   * sometimes satisfying one condition imposes another (e.g. floating
   * point bit set in the eflags register)
   */

  if (thisPtr->hazards & hz_DomRoot)
    proc_LoadFixRegs(thisPtr);

  if (thisPtr->faultCode == capros_Process_FC_MalformedProcess) {
    assert (thisPtr->processFlags & capros_Process_PF_FaultToProcessKeeper);
    return;
  }

#ifdef EROS_HAVE_FPU
  if (thisPtr->hazards & hz_FloatRegs)
    proc_LoadFloatRegs(thisPtr);

  if (thisPtr->hazards & hz_NumericsUnit)
    proc_LoadFPU(thisPtr);

#endif
  
  if (thisPtr->hazards & hz_KeyRegs)
    proc_LoadKeyRegs(thisPtr);

  if (check_disjoint) {
    if ( thisPtr->procRoot == thisPtr->keysNode ) {
      proc_SetMalformed(thisPtr);
    }
  }
  
  if (thisPtr->faultCode == capros_Process_FC_MalformedProcess) {
    assert (thisPtr->processFlags & capros_Process_PF_FaultToProcessKeeper);
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
      case act_Stall:
        /* stalled needs no special action. */
	break;
      default:
        assert(! "Rescheduling strange activity");
        break;
      }
    }
  }

  if (thisPtr->hazards & hz_SingleStep) {
    proc_LoadSingleStep(thisPtr);
  }

  proc_ValidateRegValues(thisPtr);
  
  /* Change: It is now okay for the context to carry a fault code
   * and prepare correctly.
   */

  sq_WakeAll(&thisPtr->stallQ, false);
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
  
  irqFlags_t flags = local_irq_save();

  /* It is possible for this to get called from interrupt handlers
   * before the context cache has been allocated.
   */
  if (proc_ContextCache) {
    for (i = 0; i < KTUNE_NCONTEXT; i++) {
      Process *p = &proc_ContextCache[i];
    
#ifndef NDEBUG
      if (! keyR_IsValid(&p->keyRing, p)) {
	result = false;
	break;
      }
#endif
    
#ifndef NDEBUG
      for (k = 0; k < EROS_PROCESS_KEYREGS; k++) {
	if (! key_IsValid(&p->keyReg[0])) {
	  result = false;
	  break;
	}
      }
#endif

      if (p->procRoot) {
        if (node_ToObj(p->procRoot)->obType != ot_NtProcessRoot) {
	  dprintf(true, "Context %#x process root %#x has type %d\n",
			p, p->procRoot, node_ToObj(p->procRoot)->obType);
	  result = false;
        }

        if (! p->keysNode) {
	  dprintf(true, "Context %#x has no keysNode\n", p);
	  result = false;
        } else {
          if (node_ToObj(p->keysNode)->obType != ot_NtKeyRegs) {
	    dprintf(true, "Context %#x keysNode %#x has type %d\n",
		  	  p, p->keysNode, node_ToObj(p->keysNode)->obType);
	    result = false;
          }
        }

        bool statesOK = false;	// until proven otherwise
        switch (p->runState) {
        default:
          break;

        case RS_Running:
          if (p->curActivity
              && p->curActivity->state != act_Sleeping )
            statesOK = true;
          break;

        case RS_Waiting:
          // If it has an activity, it must be sleeping:
          if (p->curActivity)
            statesOK = p->curActivity->state == act_Sleeping;
          else
            statesOK = true;
          break;

        case RS_Available:
          statesOK = ! p->curActivity;	// should have no activity
          break;
        }
        if (! statesOK) {
	  dprintf(true, "Context %#x state %d has Activity %#x state %d\n",
                  p, p->runState, p->curActivity,
                  (p->curActivity ? p->curActivity->state : 0));
	  result = false;
        }
      }

      if (result == false)
	break;
    }
  }

  local_irq_restore(flags);

  return result;
}

void 
proc_AllocUserContexts(void)
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
    p->faultCode = capros_Process_FC_NoFault;
    p->faultInfo = 0;
    p->kernelFlags = 0;
    p->processFlags = 0;
    p->hazards = 0u;	/* deriver should change this! */
    p->curActivity = 0;
    proc_InitProcessMD(p);
    
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

#ifdef OPTION_DDB
void
proc_WriteBackKeySlot(Process* thisPtr, uint32_t k)
{
  /* Write back a single key in support of DDB getting the display correct */
  assert ((thisPtr->hazards & hz_KeyRegs) == 0);
  assert (objH_IsDirty(node_ToObj(thisPtr->keysNode)));

  keyBits_UnHazard(&thisPtr->keysNode->slot[k]);
  key_NH_Set(&thisPtr->keysNode->slot[k], &thisPtr->keyReg[k]);

  keyBits_SetRwHazard(&thisPtr->keysNode->slot[k]);
}
#endif

#ifndef NDEBUG
bool
ValidCtxtPtr(const Process *ctxt)
{
  uint32_t offset;
  if ( ((uint32_t) ctxt < (uint32_t) proc_ContextCache ) || 
       ((uint32_t) ctxt >= (uint32_t)
	&proc_ContextCache[KTUNE_NCONTEXT]) )
    return false;

  offset = ((uint32_t) ctxt) - ((uint32_t) proc_ContextCache);
  offset %= sizeof(Process);

  if (offset == 0)
    return true;
  return false;
}

Process *
proc_ValidKeyReg(const Key *pKey)
{
  uint32_t ctxt;
  Process *p = 0;
  uint32_t offset;
  if ( ((uint32_t) pKey < (uint32_t) proc_ContextCache ) || 
       ((uint32_t) pKey >= (uint32_t)
	&proc_ContextCache[KTUNE_NCONTEXT]) )
    return NULL;

  /* Find the containing context: */
  ctxt = ((uint32_t) pKey) - ((uint32_t) proc_ContextCache);
  ctxt /= sizeof(Process);

  p = &proc_ContextCache[ctxt];
  
  if ( ((uint32_t) pKey < (uint32_t) &p->keyReg[0] ) || 
       ((uint32_t) pKey >= (uint32_t) &p->keyReg[EROS_PROCESS_KEYREGS]) )
    return NULL;

  offset = ((uint32_t) pKey) - ((uint32_t) &p->keyReg[0]);

  offset %= sizeof(Key);
  if (offset == 0)
    return p;
  
  return NULL;
}

bool
ValidCtxtKeyRingPtr(const KeyRing * kr)
{
  uint32_t const kri = (uint32_t)kr;
  if (kri < (uint32_t)&proc_ContextCache[0])
    return false;		// before the table
  if (kri >= (uint32_t)&proc_ContextCache[KTUNE_NCONTEXT])
    return false;		// after the table
  if ((kri - (uint32_t)&proc_ContextCache[0].keyRing) % sizeof(Process))
     return false;		// not a keyRing pointer
  return true;
}
#endif	// NDEBUG
