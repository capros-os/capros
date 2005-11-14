/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group
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
#include "TSS.h"
#include <eros/Invoke.h>
#include <eros/ProcessState.h>
#include <eros/SegKeeperInfo.h>
#include <eros/Registers.h>
#include <eros/arch/i486/Registers.h>
/*#include <kerninc/PhysMem.h>*/
#include <eros/ProcessKey.h>
#include <kerninc/KernStats.h>
#include <kerninc/Invocation.h>
#if 0
#include <machine/RegLayout.hxx>
#endif
/*#include <disk/DiskLSS.hxx>*/

#include "gen.REGMOVE.h"
/* #define MSGDEBUG
 * #define RESUMEDEBUG
 * #define XLATEDEBUG
 */

Process *proc_ContextCache = NULL;
#ifdef EROS_HAVE_FPU
Process *proc_fpuOwner;
#endif
#ifdef OPTION_SMALL_SPACES
PTE *proc_smallSpaces = 0;
#endif

bool
proc_HasDevicePriveleges(Process* thisPtr)
{
  bool hasPrivs = false;
  
  assert((thisPtr->hazards & hz_DomRoot) == 0);
  assert((thisPtr->hazards & hz_KeyRegs) == 0);

  if (keyBits_IsType(&thisPtr->procRoot->slot[ProcIoSpace], KKT_DevicePrivs))
    hasPrivs = true;

  return hasPrivs;
}

kva_t proc_ContextCacheRegion = 0;

void 
proc_AllocUserContexts()
{
  int i = 0;
  Process *contextCache = KVAtoV(Process *, proc_ContextCacheRegion);

  if (contextCache == NULL)
    contextCache = MALLOC(Process, KTUNE_NCONTEXT);
  else
    printf("Context caches already allocated at 0x%08x\n", contextCache);

  /* This is to make sure that the name field does not overflow: */
  assert (KTUNE_NCONTEXT < 1000);

  for (i = 0; i < KTUNE_NCONTEXT; i++) {
    int k;
    Process *p = &contextCache[i];
    keyR_ResetRing(&p->keyRing);
    sq_Init(&p->stallQ);

    for (k = 0; k < EROS_PROCESS_KEYREGS; k++)
      keyBits_InitToVoid(&p->keyReg[k]);

    p->cpuStack = 0;
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

      if (p->procRoot && objH_IsFree(DOWNCAST(p->procRoot, ObjectHeader))) {
	dprintf(true, "Context 0x%08x has free process root 0x%08x\n",
			p, p->procRoot);
	result = false;
      }

      if (p->keysNode && objH_IsFree(DOWNCAST(p->keysNode, ObjectHeader))) {
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
  
  if ( ((uint32_t) pKey == (uint32_t) &p->lastInvokedKey) )
    goto check_offset;

  if ( ((uint32_t) pKey < (uint32_t) &p->keyReg[0] ) || 
       ((uint32_t) pKey >= (uint32_t) &p->keyReg[EROS_PROCESS_KEYREGS]) )
    return false;

 check_offset:
  offset = ((uint32_t) pKey) - ((uint32_t) &p->keyReg[0]);

  offset %= sizeof(Key);
  if (offset == 0)
    return true;
  
  return false;
}
#endif

#ifndef NDEBUG
bool
ValidCtxtKeyRingPtr(const KeyRing* kr)
{
  uint32_t c;
  for (c = 0; c < KTUNE_NCONTEXT; c++) {
    Process *ctxt = &proc_ContextCache[c];

    if (kr == &ctxt->keyRing)
      return true;
  }

  return false;
}
#endif

void 
proc_FlushAll()
{
  uint32_t c;
  for (c = 0; c < KTUNE_NCONTEXT; c++) {
    Process *ctxt = &proc_ContextCache[c];

    /* Unload the context structure, as we are going to COW the process
     * root anyway.
     */
    
    if (ctxt->isUserContext)
      proc_Unload(ctxt);
  }
}

extern void resume_from_kernel_interrupt(savearea_t *);
extern void resume_process(Process *);
extern void resume_v86_process(Process *);

void 
proc_DumpFixRegs(Process* thisPtr)
{
  if (thisPtr->saveArea == 0)
    printf("Note: context is NOT runnable\n");
  printf("ASID   = 0x%08x  VASID = 0x%08x  nextPC=0x%08x\n",
	 thisPtr->MappingTable, thisPtr->MappingTable, thisPtr->nextPC);
  DumpFixRegs(&thisPtr->trapFrame); 
}

void 
proc_DumpPseudoRegs(Process* thisPtr)
{
  pseudoregs_t *pr = &thisPtr->pseudoRegs;

  printf(
	 "invTy = %d invKey = 0x%x sndPtr = 0x%08x sndLen= %5d sndKeys=0x%08x rcvKeys=0x%08x\n",
	 pr->invType, pr->invKey, pr->sndPtr, pr->sndLen,
	 pr->sndKeys, pr->rcvKeys);
  printf(
	 "testMe = 0x%08x\n",
	 pr->testme);

}


#ifdef EROS_HAVE_FPU
void 
proc_DumpFloatRegs(Process* thisPtr)
{
  printf("fctrl: 0x%04x fstatus: 0x%04x ftag: 0x%04x fopcode 0x%04x\n",
		 thisPtr->fpuRegs.f_ctrl, 
		 thisPtr->fpuRegs.f_status, 
		 thisPtr->fpuRegs.f_tag, 
		 thisPtr->fpuRegs.f_opcode);
  printf("fcs: 0x%04x fip: 0x%08x fds: 0x%04x fdp: 0x%08x\n",
		 thisPtr->fpuRegs.f_cs, 
		 thisPtr->fpuRegs.f_ip,
		 thisPtr->fpuRegs.f_ds,
		 thisPtr->fpuRegs.f_dp);
}
#endif


void
DumpFixRegs(const savearea_t * fx)
{
#if 0
  uint32_t cip;
  __asm__("movl 4(%%ebp),%0"
		: "=g" (cip)
	        : /* no inputs */);
#endif

#if 0
  if ( sa_IsProcess(fx) )
    printf("Process Savearea 0x%08x (cip 0x%08x)\n", fx, cip);
  else
    printf("Kernel Savearea 0x%08x (cip 0x%08x)\n", fx, cip);
#else
  if ( sa_IsProcess(fx) )
    printf("Process Savearea 0x%08x\n", fx);
  else
    printf("Kernel Savearea 0x%08x\n", fx);
#endif
  
  printf(
       "EFLAGS = 0x%08x\n"
       "EAX    = 0x%08x  EBX   = 0x%08x  ECX    = 0x%08x\n"
       "EDX    = 0x%08x  EDI   = 0x%08x  ESI    = 0x%08x\n"
       "EBP    = 0x%08x  CS    = 0x%08x  EIP    = 0x%08x\n"
       "ExNo   = 0x%08x  ExAdr = 0x%08x  Error  = 0x%08x\n",

       fx->EFLAGS,
       fx->EAX, fx->EBX, fx->ECX,
       fx->EDX, fx->EDI, fx->ESI,
       fx->EBP, fx->CS, fx->EIP,
       fx->ExceptNo, fx->ExceptAddr, fx->Error);

  if ( sa_IsProcess(fx) ) {
    /* user save area: */
    const savearea_t* sa = fx;
    
#if 0
    printf(
	   "invTy = %d invKey = 0x%x sndPtr = 0x%08x sndLen= %5d sndKeys=0x%08x rcvKeys=0x%08x\n",
	   sa->invType, sa->invKey, sa->sndPtr, sa->sndLen,
	   sa->sndKeys, sa->rcvKeys);
#endif

    printf(
		   "SS     = 0x%08x  ESP   = 0x%08x\n"
		   "DS   = 0x%04x  ES   = 0x%04x  FS   = 0x%04x"
		   "  GS   = 0x%04x\n",
		   sa->SS, sa->ESP,
		   sa->DS, sa->ES, sa->FS, sa->GS);
  }
  else {
    uint16_t ss;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    
    __asm__ __volatile__ ("mov %%ss, %0"
			  : "=r" (ss)
			  : /* no input */);
    __asm__ __volatile__ ("mov %%ds, %0"
			  : "=r" (ds)
			  : /* no input */);
    __asm__ __volatile__ ("mov %%es, %0"
			  : "=r" (es)
			  : /* no input */);
    __asm__ __volatile__ ("mov %%fs, %0"
			  : "=r" (fs)
			  : /* no input */);
    __asm__ __volatile__ ("mov %%gs, %0"
			  : "=r" (gs)
			  : /* no input */);
    printf(
	 "SS     = 0x%08x  ESP   = 0x%08x\n"
	 "DS   = 0x%04x  ES   = 0x%04x  FS   = 0x%04x  GS   = 0x%04x\n",

	 ss, ((uint32_t) fx) + 60,
	 ds, es, fs, gs);
  }
}


/* NEW POLICY FOR USER CONTEXT MANAGEMENT:
 * 
 *   A context goes through several stages before being allowed to
 *   run:
 * 
 *   HAZARD    ACTION
 * 
 *   DomRoot     Load the process root registers.
 * 
 *   Annex0      Reprepare the process general registers annex and load
 *               it's registers 
 * 
 *   KeyRegs     Reprepare the process key registers annex and load
 *               it's registers
 * 
 *   Validate    Verify that all register values are suitable.
 * 
 *   AddrSpace   Reload the master address space pointer.
 * 
 *   FloatUnit   Floating point unit needs to be loaded.
 * 
 * Note that the Annex0 hazard implies that the associated capability
 * in the process root is NOT validated.  If any of 'DomRoot' 'Annex0'
 * or 'KeyRegs' cannot be cleared the process is malformed.
 * 
 * Since it has been getting me in trouble, I am no longer trying to
 * micro-optimize slot reload.
 */

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

    if (p == act_Current()->context) {
      p = 0;
      continue;
    }

    if (inv_IsActive(&inv) && p == inv.invokee) {
      p = 0;
      continue;
    }
    
    if (p->isUserContext == false) {
      p = 0;
      continue;

    }
#if 0
    if (p->pinCount)
      p = 0;
#endif
    
    if (p->curActivity &&
	p->curActivity->state == act_Running)
      p = 0;
  }

  /* wipe out current contents, if any */
  proc_Unload(p);

#if 0
  printf("  unloaded\n");
#endif
  
  assert(p->procRoot == 0 || p->procRoot->node_ObjHdr.obType == ot_NtProcessRoot);

  p->cpuStack = 0;
  p->procRoot = 0;		/* for kernel contexts */
  p->saveArea = 0;		/* make sure not runnable! */
  p->faultCode = FC_NoFault;
  p->faultInfo = 0;
  p->processFlags = 0;
  p->isUserContext = isUser;
  /* FIX: what to do about runState? */

  if (isUser) {
#ifdef OPTION_SMALL_SPACES
    uint32_t ndx;
    uint32_t pg;
    ndx = p - proc_ContextCache;

    p->limit = SMALL_SPACE_PAGES * EROS_PAGE_SIZE;
    p->bias = UMSGTOP + (ndx * SMALL_SPACE_PAGES * EROS_PAGE_SIZE);
    p->smallPTE = &proc_smallSpaces[SMALL_SPACE_PAGES * ndx];

#if 0
    dprintf(true, "Loading small space process 0x%X bias 0x%x "
	    "limit 0x%x\n",
	    procRoot->oid, cc.bias, cc.limit);
#endif
  
    for (pg = 0; pg < SMALL_SPACE_PAGES; pg++)
      pte_Invalidate(&p->smallPTE[pg]);
#endif
  }

  p->curActivity = 0;
  p->MappingTable = KernPageDir_pa;

  return p;
}

void 
proc_Load(Node* procRoot)
{
  Process *p = proc_allocate(true);

  p->hazards =
#ifdef EROS_HAVE_FPU
  /* Must be hazarded by float regs so that we can correctly re-issue
   * floating point exceptions on restart:
   */
    hz_FloatRegs |
#endif
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
  uint32_t code = 0;
  uint32_t info = 0;
  uint32_t wantCode;
  uint32_t wantData;
  uint32_t wantPseudo;
  fixreg_t iopl;
  
  if (thisPtr->processFlags & PF_Faulted)
    return;

  iopl = (thisPtr->trapFrame.EFLAGS & MASK_EFLAGS_IOPL) >> SHIFT_EFLAGS_IOPL;

  /* FIX: This is no longer correct, as a valid user-mode application
   * can now carry PrivDomainCode and PrivDomainData in some cases. */
  wantCode =
    thisPtr->isUserContext ? sel_DomainCode : sel_KProcCode;

  wantData =
    thisPtr->isUserContext ? sel_DomainData : sel_KProcData;

  wantPseudo =
    thisPtr->isUserContext ? sel_DomainPseudo : sel_KProcData;

  /* FIX: The whole segment register check needs to be MUCH more
   * specific. There are only a few allowable values, and they should
   * be explicitly tested here.
   */

#define validate(seg, want) \
  do { \
    if ( ((seg) != sel_Null) && ((seg) != (want)) ) {   \
      code = FC_RegValue;               \
      info = (seg);                     \
      goto fault;                       \
    }                                   \
  } while (0)        

  validate(thisPtr->trapFrame.CS, wantCode);

  /* Strictly speaking, it's okay for a user program to place the code
   * segment value in a data segment register, so long as read-only
   * access is sufficient. Fix this when somebody complains someday. */
  validate(thisPtr->trapFrame.DS, wantData);
  validate(thisPtr->trapFrame.ES, wantData);
  validate(thisPtr->trapFrame.FS, wantData);
  validate(thisPtr->trapFrame.GS, wantPseudo);
  validate(thisPtr->trapFrame.SS, wantData);
  
#undef validate
  
  if ( (thisPtr->trapFrame.EFLAGS & MASK_EFLAGS_Interrupt) == 0 ) {
    /* interrupts must be enabled, various others not: */
    code = FC_RegValue;
  }
  else if ( thisPtr->trapFrame.EFLAGS & MASK_EFLAGS_Nested ) {
    code = FC_RegValue;
  }
  else if ( iopl != 0 && !proc_HasDevicePriveleges(thisPtr) ) {
    code = FC_RegValue;
  }

#ifdef EROS_HAVE_FPU
  /* Check for pending floating point exceptions too. If an unmasked
     exception is pending, invoke the process keeper.

     The bits in the control words are 'mask if set', while the bits
     in the status words are 'raise if set'.  The and-not logic is
     therefore what we want: */
  else if (thisPtr->fpuRegs.f_status & ~(thisPtr->fpuRegs.f_ctrl & MASK_FPSTATUS_EXCEPTIONS)) {
    code = FC_FloatingPointError;
    info = thisPtr->fpuRegs.f_cs;
  }

  /* The floating point %f_cs, %f_ip, %f_ds, and %f_dp registers
     record the location of the last operation.  They are output only,
     and need not be checked. */
#endif
  
 fault:
  if (code)
    proc_SetFault(thisPtr, code, info, true);
  
#if 0
  if (faultCode)
    printf("Bad register values\n");
#endif
  return;
}

/* Both loads the register values and validates that the process root
 * is well-formed.
 */
void 
proc_LoadFixRegs(Process* thisPtr)
{
  uint32_t k;
  uint8_t *rootkey0 = 0;
  assert(thisPtr->hazards & hz_DomRoot);

  assert(thisPtr->procRoot);
  objH_MakeObjectDirty(DOWNCAST(thisPtr->procRoot, ObjectHeader));

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

#ifdef EROS_HAVE_FPU
/* This version requires no annex node. */
void 
proc_LoadFloatRegs(Process* thisPtr)
{
  uint8_t *rootkey0 = 0;
  assert (thisPtr->hazards & hz_FloatRegs);
  assert (thisPtr->procRoot);
  assert (objH_IsDirty(DOWNCAST(thisPtr->procRoot, ObjectHeader)));
  
  rootkey0 = (uint8_t *) node_GetKeyAtSlot(thisPtr->procRoot, 0);

  LOAD_FLOAT_REGS;

  thisPtr->hazards &= ~hz_FloatRegs;
}
#endif

void 
proc_LoadKeyRegs(Process* thisPtr)
{
  Node *kn = 0;
  uint32_t k;
  assert (thisPtr->hazards & hz_KeyRegs);
  assert (keyBits_IsHazard(&thisPtr->procRoot->slot[ProcGenKeys]) == false);


  if (keyBits_IsType(&thisPtr->procRoot->slot[ProcGenKeys], KKT_Node) == false) {
    proc_SetMalformed(thisPtr);
    return;
  }

  /* Load the "Last Invoked Key" register value */
  key_NH_Set(&thisPtr->lastInvokedKey, 
	     &thisPtr->procRoot->slot[ProcLastInvokedKey]);
  keyBits_SetRwHazard(node_GetKeyAtSlot(thisPtr->procRoot ,ProcLastInvokedKey));

  key_Prepare(&thisPtr->procRoot->slot[ProcGenKeys]);
  kn = (Node *) key_GetObjectPtr(&thisPtr->procRoot->slot[ProcGenKeys]);
  assertex(kn,objH_IsUserPinned(DOWNCAST(kn, ObjectHeader)));
  

  assert ( node_Validate(kn) );
  node_Unprepare(kn, false);

  if (kn->node_ObjHdr.obType != ot_NtUnprepared || kn == thisPtr->procRoot) {
    proc_SetMalformed(thisPtr);
    return;
  }

  objH_MakeObjectDirty(DOWNCAST(kn, ObjectHeader));

  for (k = 0; k < EROS_NODE_SIZE; k++) {
#ifndef NDEBUG
    if ( keyBits_IsHazard(node_GetKeyAtSlot(kn, k)))
      dprintf(true, "Key register slot %d is hazarded in node 0x%08x%08x\n",
		      k,
		      (uint32_t) (kn->node_ObjHdr.kt_u.ob.oid >> 32), (uint32_t) kn->node_ObjHdr.kt_u.ob.oid);

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
  kn->node_ObjHdr.prep_u.context = thisPtr;
  kn->node_ObjHdr.obType = ot_NtKeyRegs;
  thisPtr->keysNode = kn;

  keyBits_SetWrHazard(&thisPtr->procRoot->slot[ProcGenKeys]);

  assert(thisPtr->keysNode);

  thisPtr->hazards &= ~hz_KeyRegs;
}

void 
proc_FlushFixRegs(Process* thisPtr)
{
  uint32_t k;
  uint8_t *rootkey0 = 0;
  assert((thisPtr->hazards & hz_DomRoot) == 0);
  
  assert(thisPtr->procRoot);
  assert(objH_IsDirty(DOWNCAST(thisPtr->procRoot, ObjectHeader)));
  assert(thisPtr->isUserContext);

#ifdef ProcAltMsgBuf
#error "Type checks need revision"
#endif
  
  for (k = ProcFirstRootRegSlot; k <= ProcLastRootRegSlot; k++) {
    assert ( keyBits_IsRdHazard(node_GetKeyAtSlot(thisPtr->procRoot, k)));
    assert ( keyBits_IsWrHazard(node_GetKeyAtSlot(thisPtr->procRoot, k)));
    keyBits_UnHazard(node_GetKeyAtSlot(thisPtr->procRoot, k));
  }

  rootkey0 = (uint8_t *)  (node_GetKeyAtSlot(thisPtr->procRoot, 0));

  UNLOAD_FIX_REGS;

  /* ISSUE: If this process has IOPL privileges, it is not clear that
   * we should ever be recording them back to the domain root, as this
   * makes them fairly well permanent. */
  
  keyBits_UnHazard(node_GetKeyAtSlot(thisPtr->procRoot, ProcSched));

  thisPtr->hazards |= (hz_DomRoot | hz_Schedule);
  thisPtr->saveArea = 0;
}

#ifdef EROS_HAVE_FPU
void 
proc_SaveFPU(Process* thisPtr)
{
  assert(proc_fpuOwner == thisPtr);

  mach_EnableFPU();

  __asm__ __volatile__("fnsave %0\n\t" : "=m" (thisPtr->fpuRegs));

  proc_fpuOwner = 0;

  mach_DisableFPU();
}

#ifdef EROS_HAVE_FPU
void 
proc_LoadFPU(Process* thisPtr)
{
  /* FPU is unloaded, or is owned by some other process.  Unload
   * that, and load the current process's FPU state in it's place.
   */
  if (proc_fpuOwner)
    proc_SaveFPU(proc_fpuOwner);

  assert(proc_fpuOwner == 0);

  mach_EnableFPU();

  __asm__ __volatile__("frstor %0\n\t"
		       : /* no outputs */
		       : "m" (thisPtr->fpuRegs));

  proc_fpuOwner = thisPtr;
  mach_DisableFPU();

  thisPtr->hazards &= ~hz_NumericsUnit;
}
#endif

/* This version requires no annex node. */
void 
proc_FlushFloatRegs(Process* thisPtr)
{
  uint8_t *rootkey0 = 0;
  assert ((thisPtr->hazards & hz_FloatRegs) == 0);
  assert (thisPtr->procRoot);
  assert (objH_IsDirty(DOWNCAST(thisPtr->procRoot, ObjectHeader)));

  rootkey0 = (uint8_t *) (node_GetKeyAtSlot(thisPtr->procRoot, 0));

  UNLOAD_FLOAT_REGS;

  thisPtr->hazards |= hz_FloatRegs;
  thisPtr->saveArea = 0;
}
#endif

void 
proc_FlushKeyRegs(Process* thisPtr)
{
  uint32_t k;
  assert (thisPtr->keysNode);
  assert ((thisPtr->hazards & hz_KeyRegs) == 0);
  assert (thisPtr->procRoot);
  assert (objH_IsDirty(DOWNCAST(thisPtr->keysNode, ObjectHeader)));

  assert ( node_Validate(thisPtr->keysNode) );

  /* Load the "Last Invoked Key" register value */
  keyBits_UnHazard(&thisPtr->procRoot->slot[ProcLastInvokedKey]);
  key_NH_Set(&thisPtr->procRoot->slot[ProcLastInvokedKey],
	     &thisPtr->lastInvokedKey);
  key_NH_SetToVoid(&thisPtr->lastInvokedKey);

#if 0
  printf("Flushing key regs on ctxt=0x%08x\n", this);
  if (inv.IsActive() && inv.invokee == this)
    dprintf(true,"THAT WAS INVOKEE!\n");
#endif

  for (k = 0; k < EROS_NODE_SIZE; k++) {
    keyBits_UnHazard(&thisPtr->keysNode->slot[k]);
    key_NH_Set(&thisPtr->keysNode->slot[k], &thisPtr->keyReg[k]);

    /* Not hazarded because key register */
    key_NH_SetToVoid(&thisPtr->keyReg[k]);

    /* We know that the context structure key registers are unhazarded
     * and unlinked by virtue of the fact that they are unloaded.
     */
  }

  thisPtr->keysNode->node_ObjHdr.prep_u.context = 0;
  thisPtr->keysNode->node_ObjHdr.obType = ot_NtUnprepared;
  thisPtr->keysNode = 0;

  thisPtr->hazards |= hz_KeyRegs;

  keyBits_UnHazard(&thisPtr->procRoot->slot[ProcGenKeys]);
  thisPtr->saveArea = 0;
}

/* Rewrite the process key back to our current activity.  Note that
 * the activity's process key is not reliable unless this unload has
 * been performed.
 */
void
proc_SyncActivity(Process* thisPtr)
{
  Key *procKey = 0;
  assert(thisPtr->curActivity);
  assert(thisPtr->procRoot);
  assert (thisPtr->curActivity->context == thisPtr);
  
  procKey /*@ not null @*/ = &thisPtr->curActivity->processKey;

  assert (keyBits_IsHazard(procKey) == false);

  /* Not hazarded because activity key */
  if (keyBits_IsPrepared(procKey))
    key_NH_Unprepare(procKey);

  keyBits_InitType(procKey, KKT_Process);
  procKey->u.unprep.oid = thisPtr->procRoot->node_ObjHdr.kt_u.ob.oid;
  procKey->u.unprep.count = thisPtr->procRoot->node_ObjHdr.kt_u.ob.allocCount; 
}

void
proc_Unload(Process* thisPtr)
{
  /* It might already be unloaded: */
  if (thisPtr->procRoot == 0)
    return;
  
#if 0
  {
    const char *descrip = "other";
    bool shouldStop = false;
    
    if (this == Activity::Current()->context)
      descrip = "current";
    if (inv.IsActive() && this == inv.invokee) {
      descrip = "invokee";
      shouldStop = true;
    }
   
    dprintf(shouldStop, "Unloading %s ctxt 0x%08x\n", descrip, this);
  }
#endif
    
#if 0
  if (hazards & hz::DomRoot)
    dprintf(false, "Calling Context::Unload() on 0x%08x, eip=???\n", this);
  else
    dprintf(false, "Calling Context::Unload() on 0x%08x, eip=0x%08x\n",
		   this, trapFrame.EIP);
#endif

#if defined(DBG_WILD_PTR)
  if (dbg_wild_ptr)
    if (check_Contexts("before unload") == false)
      halt('a');
#endif

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
    keyBits_UnHazard(node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace));
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
}

/* If the process has an invalid address space slot, it would probably
 * be more efficient to simply invoke the keeper right here rather
 * than start up a activity we know will fault.  There are three reasons
 * not to do so:
 * 
 * 1. It might get fixed before it runs.
 * 2. It is more modular to let the pagefault handler do it, since
 *    that handler needs to deal with this situation already.
 * 3. This code is goddamn well hairy enough as it is.
 * 
 * To load the mapping table pointer, we walk down the address space
 * segment looking for a node spanning 2^32 bits (blss=7).  We then
 * examine it's products looking for a suitable mapping table.
 * 
 * If we do not find one, we can't just build one on the spot, because
 * we must fabricate suitable depend entries for the new mapping
 * table.
 */




/* This is somewhat misnamed, as loading the address space on the x86
 * doesn't really load the address space at all -- it locates and
 * loads the page directory, which need not contain any valid entries.
 * Further faults will be handled in the page fault path.
 * 
 * In loading the page directory, the LoadAddressSpace logic does not
 * need to construct any dependency table entries for the BLSS::bit32
 * node.  That node is a producer, and if it is taken out of core or
 * its keys are deprepared we can (and must) walk the producer chain
 * in any case.
 * 
 * The address space segment slot may contain a segment key to a
 * segment whose span is larger than the span of the hardware address
 * space.  For example, the segment might span 2^40 bits while the
 * hardware address space is only 2^32 bits.  In interpreting such a
 * segment, we take the view that the address space of the process
 * starts at offset 0 in the larger segment, and is only as big as the
 * hardware will support.
 * 
 * In the event that the address space segment is oversized in this
 * way, however, LoadAddressSpace() must create depend entries for all
 * nodes whose BLSS is > BLSS::bit32.  We commit a slight bit of
 * silliness in doing this, which in practice works out okay.  A page
 * directory holds 1024 entries.  For nodes whose BLSS is >
 * BLSS::bit32, we know that the only key acting in a memory context
 * is the key in slot 0.  We therefore claim (lying through our teeth)
 * that these nodes span a range of 16 * 1024 == 16k entries in the
 * page directory.  We check for out of range invalidation in the
 * invalidate logic, so we never get caught by this.  Also, the
 * invalidate logic knows the difference between page directories and
 * page tables, and never zaps the kernel entries.
 * 
 */

void
proc_LoadAddressSpace(Process* thisPtr, bool prompt)
{
  assert (thisPtr->hazards & hz_AddrSpace);
  
  /* It's possible that we will end up invalidating exactly the
   * entries we are after!
   */
  if ( proc_DoPageFault(thisPtr, thisPtr->trapFrame.EIP, false, prompt) ) {
    thisPtr->hazards &= ~hz_AddrSpace;
  }
}

/* Fault code is tricky.  It is conceivable that the process does not
 * posess a number key in the fault code slot.  In that event we set
 * the fault code to FC_MalformedProcess.  Unless the process node
 * posesses a number key in the fault code slot, the fault code will
 * not be written back to the process root.  Note that it is not
 * possible for a process with faultCode = FC_MalformedProcess to
 * achieve any other fault code without first resolving that one, so
 * it is okay to fail to write FC_MalformedProcess back to the process
 * root.
 * 
 * Note the runstate is not loaded here.  If the process root is
 * malformed it's runstate is intrinsically undefined.  Such a process
 * cannot execute instructions.  It's malformedness is discovered in
 * one of two ways:
 * 
 *   1. It was running and somehow became malformed, in which case it
 *      invokes it's keeper on it's own behalf.
 *   2. It was invoked by a third party, who is made to invoke its
 *      keeper on its behalf.
 * 
 * If the process root later becomes well formed we will be able to
 * load its run state at that time.
 */
#if 0
void
Process::LoadFaultCode()
{
  assert (procRoot);
  
  if ( procRoot->slot[ProcTrapCode].IsType(KKT_Number) ) {
    DomRegLayout* pDomRegs = (DomRegLayout*) & ((*procRoot)[0]);

    faultCode = pDomRegs->faultCode;
    faultInfo = pDomRegs->faultInfo;
  }
  else {
    faultCode = FC_MalformedProcess;
    faultInfo = 0;
  }
  
  hazards &= ~State::DomFaultCode;
}
#endif

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
        act_Preempt(t);
        act_Wakeup(t);
        break;
      case act_Ready:
        /* need to move the activity to the proper ready Q: */
        act_Dequeue(t);
        act_Wakeup(t);
        act_Preempt(t);
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

#if 0
  /* This is wrong.  The original idea was that by prefaulting the EIP
   * address we could avoid an unnecessary kernel reentry.
   * Unfortunately, the prepare logic needs to be callable in order to
   * load the 32 bit register set from InvokeProcessKeeper.  If no
   * valid address space exists for the process, then trying to build the
   * address space here causes a segment fault.  If the relevant
   * segment has no keeper, this in turn yields in order to cause the
   * process keeper (if any) to be invoked.  In that particular
   * sequence of events, however, an infinite loop is created...
   * 
   * Rather than try to automatically build the address space here, we
   * go ahead with a zero mapping table and take the extra instruction
   * fault.
   */
  
  if (trapFrame.MappingTable == 0) {
    hazards |= hz::AddrSpace;
  }
  
  if (hazards & hz::AddrSpace)
    LoadAddressSpace(false);
#endif
  
  if (thisPtr->hazards & hz_SingleStep) {
    thisPtr->trapFrame.EFLAGS |= MASK_EFLAGS_Trap;
    thisPtr->hazards &= ~hz_SingleStep;
  }

  proc_ValidateRegValues(thisPtr);
  
  /* Change: It is now okay for the context to carry a fault code
   * and prepare correctly.
   */

  thisPtr->saveArea = &thisPtr->trapFrame;
  sq_WakeAll(&thisPtr->stallQ, false);
}

void 
proc_InvokeProcessKeeper(Process* thisPtr)
{
  Registers regs;
  Key processKey;

  keyBits_InitToVoid(&processKey);

#if 0
  dprintf(false, "Fetching register values for InvokeProcessKeeper()\n");
#endif
  proc_GetRegs32(thisPtr, &regs);

  /* Override the current domain state, as this is about to change in
     the course of the invocation. */ 
  regs.domState = RS_Waiting;

#if 0
  dprintf(false, "  ExNo: 0x%08x, Error: 0x%08x  Info: 0x%08x  FVA 0x%08x\n",
		  trapFrame.ExceptNo,
		  trapFrame.Error,
		  faultInfo,
		  trapFrame.ExceptAddr);
#endif
  /* Must be unprepared in case we throw()! */
  

  keyBits_InitType(&processKey, KKT_Process);
  processKey.u.unprep.oid = thisPtr->procRoot->node_ObjHdr.kt_u.ob.oid;
  processKey.u.unprep.count = thisPtr->procRoot->node_ObjHdr.kt_u.ob.allocCount;

  /* Guarantee that prepared resume key will be in dirty object -- see
   * comment in kern_Invoke.cxx:
   */
  assert (objH_IsDirty(DOWNCAST(thisPtr->procRoot, ObjectHeader)));
 

  proc_InvokeMyKeeper(thisPtr, OC_PROCFAULT, 0, 0, 0, &thisPtr->procRoot->slot[ProcKeeper],
		 &processKey, (uint8_t *) &regs, sizeof(regs));

}

void
proc_FlushProcessSlot(Process* thisPtr, uint32_t whichKey)
{
  assert(thisPtr->procRoot);
  
  switch (whichKey) {
  case ProcLastInvokedKey:
  case ProcGenKeys:
    assert ((thisPtr->hazards & hz_KeyRegs) == 0);
    proc_FlushKeyRegs(thisPtr);
    break;

  case ProcAddrSpace:
    Depend_InvalidateKey(node_GetKeyAtSlot(thisPtr->procRoot, whichKey));
    keyBits_UnHazard(node_GetKeyAtSlot(thisPtr->procRoot, whichKey));
    thisPtr->hazards |= hz_AddrSpace;
    assert(thisPtr->MappingTable == 0);
    thisPtr->saveArea = 0;

    break;

  case ProcSched:
    /* We aren't demolishing the process (probably), but after a schedule
       slot change the new schedule key may be invalid, in which case
       process will cease to execute and _may_ cease to be occupied by a
       activity.

       At a minimum, however, we need to mark a schedule hazard. */
    keyBits_UnHazard(node_GetKeyAtSlot(thisPtr->procRoot, whichKey));
    thisPtr->hazards |= hz_Schedule;
    thisPtr->saveArea = 0;

    break;

  default:
#ifdef EROS_HAVE_FPU
    if ( (thisPtr->hazards & hz_FloatRegs) == 0 )
      proc_FlushFloatRegs(thisPtr);
#endif
    
    assert ((thisPtr->hazards & hz_DomRoot) == 0);
    proc_FlushFixRegs(thisPtr);

    break;
  }

  thisPtr->saveArea = 0;
}

#ifdef OPTION_DDB
void
proc_WriteBackKeySlot(Process* thisPtr, uint32_t k)
{
  /* Write back a single key in support of DDB getting the display correct */
  assert ((thisPtr->hazards & hz_KeyRegs) == 0);
  assert (objH_IsDirty(DOWNCAST(thisPtr->keysNode, ObjectHeader)));

  keyBits_UnHazard(&thisPtr->keysNode->slot[k]);
  key_NH_Set(&thisPtr->keysNode->slot[k], &thisPtr->keyReg[k]);

  keyBits_SetRwHazard(&thisPtr->keysNode->slot[k]);
}
#endif

#if 0
void
Process::SetFault(uint32_t code, uint32_t info, bool needRevalidate)
{
  faultCode = code;
  faultInfo = info;

  /* For FC_MalformedProcess, should use SetMalformed() instead!! */
  assert(faultCode != FC_MalformedProcess);
  
#if 1
  if (faultCode)
    processFlags |= PF_Faulted;
  else
    processFlags &= ~PF_Faulted;
#endif
  
#if 0
  printf("Fault code set to %d info=0x%x\n", code, info);
#endif

#if 0
  /* It is now sufficient to set the PF_Faulted flag, which has the
   * effect of rendering the process non-runnable without totally
   * zapping the entire universe.
   */
  
  if (needRevalidate)
    NeedRevalidate();
#endif

#if 0
  Debugger();
#endif
}
#endif

/* On entry. inv.rcv.code == RC_OK */
bool
proc_GetRegs32(Process* thisPtr, struct Registers* regs /*@ not null @*/)
{  
  assert (proc_IsRunnable(thisPtr));

  /* Following is x86 specific.  On architectures with more annex
   * nodes this would need to check those too:
   */
  
  if (thisPtr->hazards & hz_DomRoot) {
    return false;
  }
  
  regs->arch   = ARCH_I386;
  regs->len    = sizeof(*regs);
  regs->pc     = thisPtr->trapFrame.EIP;
  regs->sp     = thisPtr->trapFrame.ESP;
  regs->faultCode = thisPtr->faultCode;
  regs->faultInfo = thisPtr->faultInfo;
  regs->domState = thisPtr->runState;

  /* RetryLik bit is NOT exposed via this interface */
  regs->domFlags = thisPtr->processFlags;
  regs->nextPC  = thisPtr->nextPC;

  regs->EDI    = thisPtr->trapFrame.EDI;
  regs->ESI    = thisPtr->trapFrame.ESI;
  regs->EBP    = thisPtr->trapFrame.EBP;
  regs->EBX    = thisPtr->trapFrame.EBX;
  regs->EDX    = thisPtr->trapFrame.EDX;
  regs->ECX    = thisPtr->trapFrame.ECX;
  regs->EAX    = thisPtr->trapFrame.EAX;
  regs->EFLAGS = thisPtr->trapFrame.EFLAGS;
  regs->CS     = thisPtr->trapFrame.CS;
  regs->SS     = thisPtr->trapFrame.SS;
  regs->ES     = thisPtr->trapFrame.ES;
  regs->DS     = thisPtr->trapFrame.DS;
  regs->FS     = thisPtr->trapFrame.FS;
  regs->GS     = thisPtr->trapFrame.GS;

  return true;
}

/* On entry. inv.rcv.code == RC_OK */
bool
proc_SetRegs32(Process* thisPtr, struct Registers* regs /*@ not null @*/)
{
#if 0
  dprintf(true, "ctxt=0x%08x: Call to SetRegs32\n", this);
#endif

  assert (proc_IsRunnable(thisPtr));

  if (thisPtr->hazards & hz_DomRoot) {
    dprintf(true, "ctxt=0x%08x: No root regs\n", thisPtr);
    return false;
  }
  
  /* Done with len, architecture */
  thisPtr->trapFrame.EIP    = regs->pc;
#if 0
  nextPC         = regs.pc;
#endif
  thisPtr->trapFrame.ESP    = regs->sp;
  thisPtr->faultCode        = regs->faultCode;
  thisPtr->faultInfo        = regs->faultInfo;
  thisPtr->runState         = regs->domState;

  /* RetryLik cannot be changed via this interface: */
  thisPtr->processFlags     = regs->domFlags;
  thisPtr->nextPC           = regs->nextPC;

  thisPtr->trapFrame.EDI    = regs->EDI;
  thisPtr->trapFrame.ESI    = regs->ESI;
  thisPtr->trapFrame.EBP    = regs->EBP;
  thisPtr->trapFrame.EBX    = regs->EBX;
  thisPtr->trapFrame.EDX    = regs->EDX;
  thisPtr->trapFrame.ECX    = regs->ECX;
  thisPtr->trapFrame.EAX    = regs->EAX;
  thisPtr->trapFrame.EFLAGS = regs->EFLAGS;
  thisPtr->trapFrame.CS     = regs->CS;
  thisPtr->trapFrame.SS     = regs->SS;
  thisPtr->trapFrame.ES     = regs->ES;
  thisPtr->trapFrame.DS     = regs->DS;
  thisPtr->trapFrame.FS     = regs->FS;
  thisPtr->trapFrame.GS     = regs->GS;

#if 0
  dprintf(true, "SetRegs(): ctxt=0x%08x: EFLAGS now 0x%08x\n", this, trapFrame.EFLAGS);
#endif

  proc_NeedRevalidate(thisPtr);
    
  return true;
}
