/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
#include <kerninc/Activity.h>
/*#include <kerninc/util.h>*/
#include <kerninc/ObjectCache.h>
#include <kerninc/Machine.h>
#include <kerninc/IRQ.h>
#include <kerninc/Invocation.h>
#include <kerninc/Process.h>
#include <arch-kerninc/PTE.h>
#include "TSS.h"
#include "GDT.h"
#include <eros/Invoke.h>
#include <eros/ProcessState.h>
#include <eros/Registers.h>
#include <eros/i486/Registers.h>
/*#include <kerninc/PhysMem.h>*/
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>
#include <arch-kerninc/Process.h>

/* #define MSGDEBUG
 * #define RESUMEDEBUG
 * #define XLATEDEBUG
 */

/*extern "C" {*/
extern void resume_process(Process *) NORETURN;
#ifdef V86_SUPPORT
extern void resume_v86_process(Process *) NORETURN;
#endif
/*};*/

void 
proc_Resume(Process* thisPtr)
{
#if 0
  printf("Resume user thread 0x%08x\n", thread);
#endif
  
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

#if 0
  if (fixRegs.ReloadUnits) {
    printf("Don't know how to reload fpu regs yet\n");
    printf("ctxt = 0x%08x\n", this);
    fixRegs.Dump();
    halt();
  }
#endif

#if 0
  printf("Resume user activity savearea 0x%08x\n", saveArea);
#endif

  /* Need to have a valid directory or the machine reboots.  It's
   * possible that the mapping table entry was nailed by a depend zap,
   * in which case we will rebuild it next time through. For now,
   * simply make sure the mapping table value at least maps the kernel
   */
 
  if (thisPtr->MappingTable == 0)
    thisPtr->MappingTable = KernPageDir_pa;
  
  assert( irq_DISABLE_DEPTH() == 1 );

  /* We will be reloading the user segment descriptors on the way out.
   * Set up here for those to have the proper base and bound.
   */
  {
#ifdef OPTION_SMALL_SPACES
    uint32_t nPages = 
      (thisPtr->smallPTE) ? SMALL_SPACE_PAGES : LARGE_SPACE_PAGES;
#else
    uint32_t nPages = LARGE_SPACE_PAGES;
#endif

    /* Either both should be nonzero or both should be zero. */
    assert( (thisPtr->smallPTE && thisPtr->bias) ||
	    (!thisPtr->smallPTE && !thisPtr->bias) );

    gdt_SetupPageSegment(seg_DomainCode, thisPtr->bias, nPages-1);
    gdt_SetupPageSegment(seg_DomainData, thisPtr->bias, nPages-1);
    gdt_SetupByteSegment(seg_DomainPseudo, 
			 KVAtoV(uva_t, &thisPtr->pseudoRegs) - KUVA,
			 sizeof(thisPtr->pseudoRegs)-1);
  }
  
#ifdef EROS_HAVE_FPU
  /* This is the right place to enable, but probably not the right
     place to *disable*. */
  if (proc_fpuOwner == thisPtr) {
    mach_EnableFPU();
  }
  else if (proc_fpuOwner) {
    mach_DisableFPU();
  }
#endif
  
  thisPtr->cpuStack = mach_GetCPUStackTop();

#ifdef V86_SUPPORT
  if ((thisPtr->fixRegs.EFLAGS & EFLAGS::Virt8086) == 0)
    resume_process(thisPtr);
  else
    resume_v86_process(thisPtr);
#else
  resume_process(thisPtr);
#endif
}

extern uint32_t cycnt0;
extern uint32_t cycnt1;
extern uint32_t rdtsr();

/* #define SND_TIMING
 * #define RCV_TIMING
 */

/* There are three PTE's that must be valid to have a valid mapping:
 * the address space pointer, the page table pointer, and the page
 * pointer.  It is conceivable that with sufficient bad fortune, the
 * first two passes through DoPageFault will end up invalidating the
 * depend entries for one of the other entries.  If we can't get it in
 * 3 tries, however, the depend table design is scrod, and we should
 * panic.
 */

PTE*
proc_BuildMapping(Process *p, ula_t ula, bool writeAccess, bool prompt)
{
  uint32_t retry;
  PTE* pte = 0;

  for (retry = 0; retry < 4; retry++) {
    if (proc_DoPageFault(p, ula, writeAccess, prompt) == false)
      return 0;

    pte = proc_TranslatePage(p, ula, PTE_V|PTE_USER, writeAccess);
#if 0
  dprintf(true, "Resulting PTE* is 0x%08x\n", pte);
#endif
    if (pte)
      return pte;
  }

  fatal("Too many retries\n");
  return 0;
}
