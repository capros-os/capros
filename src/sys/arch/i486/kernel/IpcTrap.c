/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

/* This file contains the functions that are called in the IPC path,
 * in an attempt (probably vain) to minimize I-TLB misses.
 */

/* Define this so that all of the functions that are called from the
 * IPC path will be exposed by their respective header files to be
 * inlined: 
 */

#if !defined(NDEBUG) && !defined(OPTION_DDB)
#define IPC_INTERRUPT_SUPPRESS
#endif

#define IPC_INLINES

#include <kerninc/kernel.h>
#include <kerninc/Check.h>
#include <kerninc/Machine.h>
#include <kerninc/Activity.h>
#include <kerninc/Debug.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Process.h>
#include <kerninc/KernStats.h>
#include <kerninc/IRQ.h>
#include <eros/arch/i486/io.h>
#include "lostart.h"
#include "IDT.h"
#include "GDT.h"

/* #define TIMING_DEBUG */

/* Declared in IPC-vars.cxx */
#if 0
extern void Invoke();
#endif

/* Called from the interrupt entry point with interrupts disabled.
   The interrupt handler assembly code has also incremented
   DisableDepth, so we are running as though we had already called
   IRQ::DISABLE().  Do any processing that must be done with
   interrupts disabled here and then call IRQ::ENABLE() to allow
   nested interrupts (when we get that working).

   The saveArea pointer is passed in solely for use by the kernel
   debugger in back-walking the stack.
   */

/* Yields, does not return. */
void
idt_OnKeyInvocationTrap(savearea_t * saveArea)
{
  assert(saveArea == &(act_CurContext()->trapFrame));
#ifndef NDEBUG

  uint32_t vecNumber = act_CurContext()->trapFrame.ExceptNo;

  assert (vecNumber == iv_InvokeKey);
#endif

  assert(local_irq_disabled());

#ifndef IPC_INTERRUPT_SUPPRESS
  /* We have now done all of the processing that must be done with
   * interrupts disabled.  Re-enable interrupts here:
   */

  irq_ENABLE();  
#endif

  assert (act_Current());

#ifndef NDEBUG
  if (dbg_inttrap)
    Debugger();
#endif

#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("Before Invoke");
#endif

  {
    Process* sndContext = act_CurContext();

    /* If IPC block validation is running in the IPC assembly path,
       that path may have set a fault code, in which case we need to
       bypass the actual invocation and let the thread scheduler
       invoke the domain keeper. */
    if (sndContext->faultCode == capros_Process_FC_NoFault)
      BeginInvocation();
  
      objH_BeginTransaction();

      /* Roll back the invocation PC in case we need to restart this operation */
      proc_AdjustInvocationPC(sndContext);

      proc_SetupEntryBlock(sndContext, &inv);

      proc_DoKeyInvocation(sndContext);
  }
  
#ifdef DBG_WILD_PTR
  if (dbg_wild_ptr)
    check_Consistency("After Invoke()");
#endif

#ifndef IPC_INTERRUPT_SUPPRESS
  /* On return from Invoke() we might NOT have a current thread
   * because the invocation may have been a return to a kernel key.
   * assert ( Thread::Current() );
   */
  irq_DISABLE();
  assert(local_irq_disabled());
#endif
    
  /* 
   * If the thread is yielding voluntarily, it MUST be rescheduled.
   * 
   * If the current thread is a user thread, it is possible that
   * having completed the invocation means that the current thread
   * needs to be reprepared, or that the thread has faulted. If the
   * thread has faulted, it has not yielded, as we need to know in
   * order to migrate the thread to the keeper.
   * 
   * It is also possible that in attempting to reprepare the current
   * thread, we will discover that the thread has DIED.  This can
   * happen if a domain rescinds itself, or if it returns to a kernel
   * key.
   * 
   * Rather than try to deal with all of this in multiple places, we
   * unconditionally call Thread::Resched().  If appropriate,
   * Thread::Resched() will simply return the current thread in
   * prepared form, and we will return to it.  If the thread should
   * yield unconditionally, we tell Thread::Resched() so.
   */
  
  assert(local_irq_disabled());

  ExitTheKernel();
}
