/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, 2008, Strawberry Development Group
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
#include <kerninc/Node.h>
#include <arch-kerninc/KernTune.h>
#include <kerninc/Depend.h>
#include <kerninc/Activity.h>
#include <kerninc/util.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Machine.h>
#include <kerninc/Invocation.h>
#include <kerninc/Check.h>
#include <kerninc/Process.h>
#include <kerninc/CpuReserve.h>
#include <kerninc/IRQ.h>
#include <arch-kerninc/Process.h>
#include <arch-kerninc/PTE.h>
#include "TSS.h"
#include <eros/Invoke.h>
#include <disk/DiskNodeStruct.h>
#include <eros/ProcessState.h>
#include <idl/capros/arch/i386/Process.h>
#include <kerninc/Invocation.h>
#include "Process486.h"

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
/* Pointer to the (contiguous) page tables for small spaces. */
PTE * proc_smallSpaces = 0;
#endif

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

    p->md.cpuStack = 0;
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
  
  irqFlags_t flags = local_irq_save();

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

  local_irq_restore(flags);

  return result;
}

void 
proc_DumpFixRegs(Process* thisPtr)
{
  if (proc_IsNotRunnable(thisPtr))
    printf("Note: process is NOT runnable\n");
  printf("MappingTable = 0x%08x\n", thisPtr->md.MappingTable);
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

void
proc_Init_MD(Process * p, bool isUser)
{
  p->md.cpuStack = 0;

  if (isUser) {
#ifdef OPTION_SMALL_SPACES
    proc_InitSmallSpace(p);	// always start out with a small space

#if 0
    dprintf(true, "Loading small space process 0x%X bias 0x%x "
	    "limit 0x%x\n",
	    procRoot->oid, cc.md.bias, cc.md.limit);
#endif
  
    /* Clear the PTEs, because the change in p->procRoot is not
       tracked by Depend. */
    uint32_t pg;
    for (pg = 0; pg < SMALL_SPACE_PAGES; pg++)
      pte_Invalidate(&p->md.smallPTE[pg]);
#endif
  }

  p->md.MappingTable = KernPageDir_pa;

#ifdef EROS_HAVE_FPU
  /* Must be hazarded by float regs so that we can correctly re-issue
   * floating point exceptions on restart:
   */
  p->hazards |= hz_FloatRegs;
#endif
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
  
  if (thisPtr->processFlags & capros_Process_PF_FaultToProcessKeeper)
    return;

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
      code = capros_arch_i386_Process_FC_InvalidSegReg; \
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
  
  thisPtr->trapFrame.EFLAGS &= ~MASK_EFLAGS_Nested;

  if (proc_HasDevicePrivileges(thisPtr)) {
    // He has iopl privileges.
    thisPtr->trapFrame.EFLAGS |= MASK_EFLAGS_IOPL;	// set IOPL = 3
    /* KF_IOPRIV isn't used on this architecture - IOPL is sufficient. */
  }
  else {
    // He doesn't have iopl privileges.
    thisPtr->trapFrame.EFLAGS &= ~MASK_EFLAGS_IOPL;	// set IOPL = 0
    /* Interrupts must be enabled. Just set the bit rather than bitch. */
    thisPtr->trapFrame.EFLAGS |= MASK_EFLAGS_Interrupt;
  }

#ifdef EROS_HAVE_FPU
  /* Check for pending floating point exceptions too. If an unmasked
     exception is pending, invoke the process keeper.

     The bits in the control words are 'mask if set', while the bits
     in the status words are 'raise if set'.  The and-not logic is
     therefore what we want: */
  if (thisPtr->fpuRegs.f_status & ~(thisPtr->fpuRegs.f_ctrl & MASK_FPSTATUS_EXCEPTIONS)) {
    code = capros_Process_FC_FPFault;
    info = thisPtr->fpuRegs.f_cs;
  }

  /* The floating point %f_cs, %f_ip, %f_ds, and %f_dp registers
     record the location of the last operation.  They are output only,
     and need not be checked. */
#endif
  
  if (code) {
    fault:
    proc_SetFault(thisPtr, code, info);
  }
  
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

  keyBits_SetWrHazard(node_GetKeyAtSlot(thisPtr->procRoot, ProcIoSpace));

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
proc_FlushFixRegs(Process * thisPtr)
{
  assert((thisPtr->hazards & hz_DomRoot) == 0);
  
  assert(thisPtr->procRoot);
  assert(objH_IsDirty(node_ToObj(thisPtr->procRoot)));
  assert(thisPtr->isUserContext);

  unsigned int k;
  Key * key;
  for (k = ProcFirstRootRegSlot; k <= ProcLastRootRegSlot; k++) {
    key = node_GetKeyAtSlot(thisPtr->procRoot, k);
    assert(keyBits_IsRdHazard(key));
    assert(keyBits_IsWrHazard(key));
    keyBits_UnHazard(key);
  }

  // ProcIoSpace affects EFLAGS.IOPL.
  key = node_GetKeyAtSlot(thisPtr->procRoot, ProcIoSpace);
  assert(keyBits_IsWrHazard(key));
  keyBits_UnHazard(key);

  uint8_t * rootkey0 = (uint8_t *) (node_GetKeyAtSlot(thisPtr->procRoot, 0));

  UNLOAD_FIX_REGS;

  keyBits_UnHazard(node_GetKeyAtSlot(thisPtr->procRoot, ProcSched));

  thisPtr->hazards |= (hz_DomRoot | hz_Schedule);
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
}
#endif

void
proc_LoadSingleStep(Process * thisPtr)
{
  thisPtr->trapFrame.EFLAGS |= MASK_EFLAGS_Trap;
  thisPtr->hazards &= ~hz_SingleStep;
}

void
proc_FlushProcessSlot(Process * thisPtr, unsigned int whichKey)
{
  assert(thisPtr->procRoot);
  
  switch (whichKey) {
  case ProcGenKeys:
    assert ((thisPtr->hazards & hz_KeyRegs) == 0);
    proc_FlushKeyRegs(thisPtr);
    break;

  case ProcAddrSpace:
    Depend_InvalidateKey(node_GetKeyAtSlot(thisPtr->procRoot, whichKey));
    break;

  case ProcSched:
    /* We aren't demolishing the process (probably), but after a schedule
       slot change the new schedule key may be invalid, in which case
       process will cease to execute and _may_ cease to be occupied by a
       activity.

       At a minimum, however, we need to mark a schedule hazard. */
    keyBits_UnHazard(node_GetKeyAtSlot(thisPtr->procRoot, whichKey));
    thisPtr->hazards |= hz_Schedule;

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
}

void
proc_GetCommonRegs32(Process * thisPtr,
  struct capros_Process_CommonRegisters32 * regs)
{  
  assert (proc_IsRunnable(thisPtr));

  regs->arch   = capros_Process_ARCH_I386;
  regs->faultCode = thisPtr->faultCode;
  regs->faultInfo = thisPtr->faultInfo;
  regs->pc     = thisPtr->trapFrame.EIP;
  regs->sp     = thisPtr->trapFrame.ESP;

  if (thisPtr->runState == RS_Running)
    /* If runState == RS_Running,
       processFlags.PF_expectingMsg isn't significant and could be set.
       Make sure it's cleared in procFlags for consistency. */
    thisPtr->processFlags &= ~capros_Process_PF_ExpectingMessage;
  regs->procFlags = thisPtr->processFlags;
}

bool
proc_GetRegs32(Process * thisPtr,
  struct capros_arch_i386_Process_Registers * regs)
{  
  /* Following is x86 specific.  On architectures with more annex
   * nodes this would need to check those too:
   */
  
  if (thisPtr->hazards & hz_DomRoot) {
    return false;
  }

  proc_GetCommonRegs32(thisPtr,
                       (struct capros_Process_CommonRegisters32 *)regs);
  
  regs->len    = sizeof(*regs);

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

void
proc_SetCommonRegs32MD(Process * thisPtr,
  struct capros_Process_CommonRegisters32 * regs)
{
  thisPtr->trapFrame.EIP    = regs->pc;
  thisPtr->trapFrame.ESP    = regs->sp;
}

void
proc_SetRegs32(Process * thisPtr,
  struct capros_arch_i386_Process_Registers * regs)
{
  proc_SetCommonRegs32(thisPtr,
                       (struct capros_Process_CommonRegisters32 *) regs);
  
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

  proc_ValidateRegValues(thisPtr);
}

/* May Yield. */
void 
proc_InvokeProcessKeeper(Process * thisPtr)
{
  struct capros_arch_i386_Process_Registers regs;
  proc_GetRegs32(thisPtr, &regs);

  // Show the state as it will be after the keeper invocation.
  regs.procFlags &= ~capros_Process_PF_ExpectingMessage;

  Key * kpr = node_GetKeyAtSlot(thisPtr->procRoot, ProcKeeper);

#ifndef NDEBUG
  if (! keyBits_IsGateKey(kpr))
    dprintf(true, "Process faulting, no keeper.\n");
#endif

  key_SetToProcess(&inv.keeperArg, thisPtr, KKT_Process, 0);
  inv.flags |= INV_KEEPERARG;

  proc_InvokeMyKeeper(thisPtr, OC_PROCFAULT, 0, 0, 0, kpr,
                      &inv.keeperArg, (uint8_t *) &regs, sizeof(regs));
}
