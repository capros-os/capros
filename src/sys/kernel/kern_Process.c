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
#include <disk/DiskNodeStruct.h>
#include <kerninc/Key.h>
#include <kerninc/ObjectHeader.h>
#include <kerninc/Node.h>
#include <kerninc/Activity.h>
#include <kerninc/Depend.h>
#include <kerninc/Invocation.h>
#include <kerninc/CpuReserve.h>

void
proc_ClearActivity(Process * proc)
{
  Activity * act = proc->curActivity;
  assert(act);

  act_Dequeue(act);
  act_SetContext(act, 0);
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
    Key * key = node_GetKeyAtSlot(thisPtr->keysNode, k);
    keyBits_UnHazard(key);
    key_NH_Set(key, &thisPtr->keyReg[k]);

    /* Not hazarded because key register */
    key_NH_SetToVoid(&thisPtr->keyReg[k]);

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

  node_MakeDirty(kn);

  for (k = 0; k < EROS_NODE_SIZE; k++) {
#ifndef NDEBUG
    if ( keyBits_IsHazard(node_GetKeyAtSlot(kn, k)))
      dprintf(true, "Key register slot %d is hazarded in node 0x%08x%08x\n",
		      k,
		      (uint32_t) (kn->node_ObjHdr.oid >> 32),
                      (uint32_t) kn->node_ObjHdr.oid);

    /* We know that the context structure key registers are unhazarded
     * and unprepared by virtue of the fact that they are unloaded,
     * but check here just in case:
     */
    if ( keyBits_IsHazard(&thisPtr->keyReg[k]) )
      dprintf(true, "Key register %d is hazarded in Process 0x%08x\n",
		      k, thisPtr);

    if ( keyBits_IsUnprepared(&thisPtr->keyReg[k]) == false )
      dprintf(true, "Key register %d is prepared in Process 0x%08x\n",
		      k, thisPtr);
#endif

    if (k == 0)
      key_NH_SetToVoid(&thisPtr->keyReg[0]);	/* key register 0 is always void */
    else
      key_NH_Set(&thisPtr->keyReg[k], &kn->slot[k]);

    keyBits_SetRwHazard(node_GetKeyAtSlot(kn ,k));
  }
  
  /* Node is now known to be valid... */
  kn->node_ObjHdr.obType = ot_NtKeyRegs;
  kn->node_ObjHdr.prep_u.context = thisPtr;
  thisPtr->keysNode = kn;

  keyBits_SetWrHazard(genKeysKey);

  assert(thisPtr->keysNode);

  thisPtr->hazards &= ~hz_KeyRegs;
}

/* Rewrite the process key back to our current activity.  Note that
 * the activity's process key is not reliable unless this unload has
 * been performed.
 */
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

  keyBits_InitType(procKey, KKT_Process);
  procKey->u.unprep.oid = thisPtr->procRoot->node_ObjHdr.oid;
  procKey->u.unprep.count = thisPtr->procRoot->node_ObjHdr.allocCount; 
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
  
  keyR_UnprepareAll(&thisPtr->keyRing);	// Why? CRL
  thisPtr->hazards = 0;
  thisPtr->procRoot = 0;

  // dprintf(false,  "Unload of context 0x%08x complete\n", thisPtr);

  sq_WakeAll(&thisPtr->stallQ, false);
exit: ;
  assert(! thisPtr->curActivity);
}

/* Simple round-robin policy for now:  */
Process *
proc_allocate(bool isUser)
{
  static uint32_t nextClobber = 0;
  Process * p;

  for (;;) {
    p = &proc_ContextCache[nextClobber++];
    if (nextClobber >= KTUNE_NCONTEXT)
      nextClobber = 0;

    if (p == proc_Current())	/* can't use current Process */
      continue;

    if (inv_IsActive(&inv) && p == inv.invokee)
      continue;
    
    if (p->isUserContext == false)	/* can't steal kernel Processes */
      continue;
    
    if (p->curActivity &&
	p->curActivity->state == act_Running)
      continue; 

    break;	// Use this process
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

  objH_TransLock(node_ToObj(thisPtr->procRoot));
  if (thisPtr->keysNode)
    objH_TransLock(node_ToObj(thisPtr->keysNode));
  
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

bool 
proc_ValidKeyReg(const Key *pKey)
{
  uint32_t ctxt;
  Process *p = 0;
  uint32_t offset;
  if ( ((uint32_t) pKey < (uint32_t) proc_ContextCache ) || 
       ((uint32_t) pKey >= (uint32_t)
	&proc_ContextCache[KTUNE_NCONTEXT]) )
    return false;

  /* Find the containing context: */
  ctxt = ((uint32_t) pKey) - ((uint32_t) proc_ContextCache);
  ctxt /= sizeof(Process);

  p = &proc_ContextCache[ctxt];
  
  if ( ((uint32_t) pKey < (uint32_t) &p->keyReg[0] ) || 
       ((uint32_t) pKey >= (uint32_t) &p->keyReg[EROS_PROCESS_KEYREGS]) )
    return false;

  offset = ((uint32_t) pKey) - ((uint32_t) &p->keyReg[0]);

  offset %= sizeof(Key);
  if (offset == 0)
    return true;
  
  return false;
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
