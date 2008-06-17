/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

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
#include <kerninc/Invocation.h>
#include <idl/capros/arch/arm/Process.h>
#include "arm.h"

#include "gen.REGMOVE.h"
/* #define MSGDEBUG
 * #define RESUMEDEBUG
 * #define XLATEDEBUG
 */

#define userCPSRBits (0xff000000 | MASK_CPSR_Thumb)
		/* bits that the user can play with */

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
  if (thisPtr->processFlags & capros_Process_PF_FaultToProcessKeeper)
    return;

  if (thisPtr->isUserContext) {
    if (proc_HasDevicePrivileges(thisPtr)) {
      thisPtr->kernelFlags |= KF_IoPriv;
      /* He can have IRQ disabled: */
      thisPtr->trapFrame.CPSR &= (userCPSRBits | MASK_CPSR_IRQDisable);
    }
    else {
      thisPtr->kernelFlags &= ~KF_IoPriv;
      thisPtr->trapFrame.CPSR &= userCPSRBits;
    }

    /* Rather than force the programmer to set the correct CPSR bits,
       we just set them correctly here. */
    thisPtr->trapFrame.CPSR |= CPSRMode_User;
  } else {
    /* Kernel processes might disable IRQ, but we will never see it,
       because they don't execute SWI's and don't page fault and
       the process won't be preempted until IRQ is reenabled. */
    assert((thisPtr->trapFrame.CPSR & ~userCPSRBits) == CPSRMode_System);
  }
}

// One-time initialization:
void
proc_InitProcessMD(Process * proc)
{
}

// Initialization on each allocation:
void
proc_Init_MD(Process * p, bool isUser)
{
  proc_ResetMappingTable(p);
}

/* Initialize a privileged Process. */
Process *
kproc_Init(
           const char *myName,
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

  p->runState = RS_Running;

  memset(&p->trapFrame, 0, sizeof(p->trapFrame));
  
  proc_ResetMappingTable(p);	/* kernel access only */
  p->trapFrame.CPSR = 0x1f;	/* System mode */
  p->trapFrame.r15 = (uint32_t) pc;

  p->trapFrame.r13 = (uint32_t) stkTop;
    
  return p;
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
  if (proc_IsNotRunnable(thisPtr))
    printf("Note: process is NOT runnable\n");
  printf("Process = 0x%08x  PID   = 0x%08x\n",
	 thisPtr, thisPtr->md.pid);
  DumpFixRegs(&thisPtr->trapFrame); 
}

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
    Depend_InvalidateKey(node_GetKeyAtSlot(thisPtr->procRoot, ProcAddrSpace));
    assert(thisPtr->md.firstLevelMappingTable == FLPT_NullPA);
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

  regs->arch   = capros_Process_ARCH_ARMProcess;
  regs->faultCode = thisPtr->faultCode;
  regs->faultInfo = thisPtr->faultInfo;
  regs->pc     = thisPtr->trapFrame.r15;
  regs->sp     = thisPtr->trapFrame.r13;

  if (thisPtr->runState == RS_Running)
    /* If runState == RS_Running,
       processFlags.PF_expectingMsg isn't significant and could be set.
       Make sure it's cleared in procFlags for consistency. */
    thisPtr->processFlags &= ~capros_Process_PF_ExpectingMessage;
  regs->procFlags = thisPtr->processFlags;
}

bool
proc_GetRegs32(Process * thisPtr,
  struct capros_arch_arm_Process_Registers * regs)
{  
  if (thisPtr->hazards & hz_DomRoot) {
    return false;
  }
  
  proc_GetCommonRegs32(thisPtr,
                       (struct capros_Process_CommonRegisters32 *)regs);

  regs->len    = sizeof(*regs);

  regs->registers[0]  = thisPtr->trapFrame.r0;
  regs->registers[1]  = thisPtr->trapFrame.r1;
  regs->registers[2]  = thisPtr->trapFrame.r2;
  regs->registers[3]  = thisPtr->trapFrame.r3;
  regs->registers[4]  = thisPtr->trapFrame.r4;
  regs->registers[5]  = thisPtr->trapFrame.r5;
  regs->registers[6]  = thisPtr->trapFrame.r6;
  regs->registers[7]  = thisPtr->trapFrame.r7;
  regs->registers[8]  = thisPtr->trapFrame.r8;
  regs->registers[9]  = thisPtr->trapFrame.r9;
  regs->registers[10] = thisPtr->trapFrame.r10;
  regs->registers[11] = thisPtr->trapFrame.r11;
  regs->registers[12] = thisPtr->trapFrame.r12;
  regs->registers[13] = thisPtr->trapFrame.r13;
  regs->registers[14] = thisPtr->trapFrame.r14;
  regs->registers[15] = thisPtr->trapFrame.r15;
  regs->CPSR  = thisPtr->trapFrame.CPSR;

  return true;
}

void
proc_SetCommonRegs32MD(Process * thisPtr,
  struct capros_Process_CommonRegisters32 * regs)
{ 
  thisPtr->trapFrame.r15    = regs->pc;
  thisPtr->trapFrame.r13    = regs->sp;
}

void
proc_SetRegs32(Process * thisPtr,
  struct capros_arch_arm_Process_Registers * regs)
{
  proc_SetCommonRegs32(thisPtr,
                       (struct capros_Process_CommonRegisters32 *) regs);
  
  thisPtr->trapFrame.r0  = regs->registers[0];
  thisPtr->trapFrame.r1  = regs->registers[1];
  thisPtr->trapFrame.r2  = regs->registers[2];
  thisPtr->trapFrame.r3  = regs->registers[3];
  thisPtr->trapFrame.r4  = regs->registers[4];
  thisPtr->trapFrame.r5  = regs->registers[5];
  thisPtr->trapFrame.r6  = regs->registers[6];
  thisPtr->trapFrame.r7  = regs->registers[7];
  thisPtr->trapFrame.r8  = regs->registers[8];
  thisPtr->trapFrame.r9  = regs->registers[9];
  thisPtr->trapFrame.r10 = regs->registers[10];
  thisPtr->trapFrame.r11 = regs->registers[11];
  thisPtr->trapFrame.r12 = regs->registers[12];
  // thisPtr->trapFrame.r13 = regs->registers[13];
  thisPtr->trapFrame.r14 = regs->registers[14];
  // thisPtr->trapFrame.r15 = regs->registers[15];
  thisPtr->trapFrame.CPSR = regs->CPSR;

  proc_ValidateRegValues(thisPtr);
}

/* May Yield. */
void
proc_InvokeProcessKeeper(Process * thisPtr)
{
  struct capros_arch_arm_Process_Registers regs;
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

void
proc_LoadSingleStep(Process * thisPtr)
{
  fatal("Single Step not implemented on ARM.\n");
}

/* Architecture-dependent C code for resuming a process. */
void 
ExitTheKernel_MD(Process * thisPtr)
{
  /* must have kernel access */
  assert(thisPtr->md.firstLevelMappingTable != 0);
  assertex(thisPtr, (thisPtr->md.dacr & 0x3) == 1);
  
#ifndef NDEBUG
#if 1
  if (thisPtr->trapFrame.r15 & 0x3	// unaligned PC
     )
    dprintf(true, "Resume user proc 0x%08x pc=0x%08x pid=0x%08x cpsr=0x%08x\n",
           thisPtr,
           thisPtr->trapFrame.r15, thisPtr->md.pid, thisPtr->trapFrame.CPSR);
#endif
#endif
}
