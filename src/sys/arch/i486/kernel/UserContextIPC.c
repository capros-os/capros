/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
#include <kerninc/Activity.h>
#include <kerninc/ObjectCache.h>
#include <kerninc/Machine.h>
#include <kerninc/Invocation.h>
#include <kerninc/Process.h>
#include <arch-kerninc/PTE.h>
#include "TSS.h"
#include "GDT.h"
#include <eros/Invoke.h>
#include <kerninc/Invocation.h>
#include <kerninc/Machine.h>
#include <arch-kerninc/Process.h>
#include <arch-kerninc/IRQ-inline.h>
#include "Process486.h"

/* #define MSGDEBUG
 * #define RESUMEDEBUG
 * #define XLATEDEBUG
 */

void 
ExitTheKernel_MD(Process * thisPtr)
{
  assert(proc_IsRunnable(act_CurContext()));

  /* Need to have a valid directory or the machine reboots.  It's
   * possible that we yielded while the mapping table entry was set to
     PTE_IN_PROGRESS during a page fault,
   * in which case we will rebuild it next time through. For now,
   * simply make sure the mapping table value at least maps the kernel.
   */
 
  if (thisPtr->md.MappingTable == PTE_IN_PROGRESS) {
    thisPtr->md.MappingTable = KernPageDir_pa;
#ifdef OPTION_SMALL_SPACES
    proc_InitSmallSpace(thisPtr);	// always start out with a small space
#endif
  }

  /* We will be reloading the user segment descriptors on the way out.
   * Set up here for those to have the proper base and bound.
   */
  {
#ifdef OPTION_SMALL_SPACES
    uint32_t nPages = 
      (thisPtr->md.smallPTE) ? SMALL_SPACE_PAGES : LARGE_SPACE_PAGES;
#else
    uint32_t nPages = LARGE_SPACE_PAGES;
#endif

    /* Either both should be nonzero or both should be zero. */
    assert( (thisPtr->md.smallPTE && thisPtr->md.bias) ||
	    (!thisPtr->md.smallPTE && !thisPtr->md.bias) );

    gdt_SetupPageSegment(seg_DomainCode, thisPtr->md.bias, nPages-1);
    gdt_SetupPageSegment(seg_DomainData, thisPtr->md.bias, nPages-1);
    gdt_SetupByteSegment(seg_DomainPseudo, 
			 KVAtoV(uva_t, &thisPtr->pseudoRegs) - KUVA,
			 sizeof(thisPtr->pseudoRegs)-1);
  }
  
#ifdef EROS_HAVE_FPU
  if (proc_fpuOwner == thisPtr) {
    mach_EnableFPU();
  }
  else
    mach_DisableFPU();
#endif
  
  thisPtr->md.cpuStack = mach_GetCPUStackTop();
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

/* May Yield. */
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
