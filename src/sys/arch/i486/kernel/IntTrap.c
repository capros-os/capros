/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group
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

/* This file contains the functions that are called in the IPC path,
 * in an attempt (probably vain) to minimize I-TLB misses.
 */

/* Define this so that all of the functions that are called from the
 * IPC path will be exposed by their respective header files to be
 * inlined: 
 */

#define IPC_INLINES

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <kerninc/Check.h>
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

#ifdef OPTION_DDB
#include "Debug386.h"
#endif

/* #define TIMING_DEBUG */

/*extern "C" {*/
extern void EnableCounters(uint32_t ctl);
extern void DisableCounters();
/*}*/
extern void halt(char);

#define MAX_TRAP_DEPTH 2

#ifndef NDEBUG
inline static bool
ValidEIP(uint32_t eip)
{
  /* Kernel text is okay: */
  if (eip >= (uint32_t)&_start && eip < (uint32_t)&etext)
    return true;

  /* BIOS prom is okay (PCI BIOS): */
  if (eip >= 0xe0000u && eip < 0xfffff)
    return true;

  return false;
}
#endif

/*extern "C" {*/
extern void resume_from_kernel_interrupt(savearea_t *) NORETURN;
/*};*/

/* Called from the interrupt entry point with interrupts disabled.
   The interrupt handler assembly code has also incremented
   DisableDepth, so we are running as though we had already called
   IRQ::DISABLE().  Do any processing that must be done with
   interrupts disabled here and then call IRQ::ENABLE() to allow
   nested interrupts (when we get that working).

   Note that saveArea does NOT always point to a proper savearea_t
   structure.  If we interrupted a process (user or supervisor), then
   saveArea points to a valid savearea_t structure.  If we interrupted
   the kernel, then saveArea points to a supervisor interrupt frame on
   the kernel interupt stack.  The kernel save frame is a subset of
   the user save frame, and the following code is careful about what
   it references.
   */

void
idt_OnTrapOrInterrupt(savearea_t *saveArea)
{
#ifndef NDEBUG
  register Activity* curActivity = 0;
#endif
  
  uint32_t vecNumber = saveArea->ExceptNo;

  KernStats.nInter++;
      
  assert(local_irq_disabled());

#ifndef NDEBUG
  /* If we interrupted an invocation, there is no guarantee that
   * there exists a current activity -- we may have interrupted the
   * invocation path just after the current activity has been deleted.
   */

  curActivity = act_Current();
  assert(curActivity || !sa_IsProcess(saveArea));
#endif

#ifndef NDEBUG
#ifdef OPTION_DDB
  if (dbg_inttrap) {
    kdb_trap(vecNumber, saveArea->Error, saveArea);
    /* Don't call Debugger(), because it causes a breakpoint,
       which would recurse forever. */
  }
#endif
#endif

#ifndef NDEBUG
  /* NOTE: There was a bug here in which a timer interrupt that nailed
   * the kernel in nested fashion could trigger a context check while
   * something critical was being updated. */
  
  if ((sa_IsProcess(saveArea)
          || ((vecNumber != iv_BreakPoint) && (vecNumber < iv_IRQ0)) )
      && ! check_Contexts("on int entry") ) {
    halt('a');
  }
  
#endif
  
#ifndef NDEBUG
  /* Various paranoia checks: */
  
#if 0
  /* This was obsoleted by the NEW_KSTACK code, and I haven't
     resurrected it yet. */
  if ( ( (uint32_t) &stack < (uint32_t) InterruptStackBottom ) ||
       ( (uint32_t) &stack > (uint32_t) InterruptStackTop ) ) {
    halt('b');
    printf("Interrupt 0x%x, stack is 0x%08x pc is 0x%08x\n",
    	           vecNumber, &stack, saveArea->EIP);
    if (vecNumber == 0xe)
    	printf("fva=0x%08x ESI=0x%08x ECX=0x%08x ctxt ESI=0x%08x\n"
		       "ctxt EBX=0x%08x ctxt EDX=0x%08x\n",
	               saveArea->ExceptAddr, saveArea->ESI,
		       saveArea->ECX,
               
		       ((Process *) act_CurContext())->fixRegs.ESI,
		       ((Process *) act_CurContext())->fixRegs.EBX,
		       ((Process *) act_CurContext())->fixRegs.EDX);

    halt('c');
    debug_Backtrace("Interrupt on wrong stack", true);
    
  }
    
  {
    kva_t InterruptStackLimit = (kva_t)InterruptStackBottom;
    InterruptStackLimit += 128;

    if ( (kva_t) &stack < InterruptStackLimit ) {
      halt('d');
      debug_Backtrace("Stack limit exceeded", true);
    }
  }
#endif /* #if 0 */

  if ( saveArea == 0 ||	// FIXME: can't be zero!?
       ( sa_IsKernel(saveArea) && !ValidEIP(saveArea->EIP) ) ) {
    /* halt('e'); */
    fatal("Bogus save area 0x%08x vecno %d\n"
		  "   EIP=0x%08x CurActivity = %s ctxt=0x%08x\n",
		  saveArea, vecNumber,
		  saveArea ? saveArea->EIP : 0,
                  act_Name(curActivity), act_CurContext());
  }
#endif
  
  /* We have now done all of the processing that must be done with
   * interrupts disabled.  Re-enable interrupts here:
   */

#ifndef NESTED_INTERRUPT_SUPPRESS
  /* This could be either an interrupt or an exception, so use irq_ENABLE. */
  irq_ENABLE();
#endif

#if defined(DBG_WILD_PTR) && (DBG_WILD_PTR > 1)
  if (dbg_wild_ptr)
    check_Consistency("before int dispatch");
#endif
  assert (IntVecEntry[vecNumber]);

#if 0
  /* Count S D R+W miss (1), S I miss (0): */
  EnableCounters(0x0269024E);
#endif

  /* Dispatch to the handler: */
  IntVecEntry[vecNumber](saveArea);

#if defined(DBG_WILD_PTR) && (DBG_WILD_PTR > 1)
  if (dbg_wild_ptr)
    check_Consistency("after int dispatch");
#endif

  assert ( act_Current() || !sa_IsProcess(saveArea));

  /* We are going to process all pending interrupts and then return to
   * the activity.  We need to make sure that we do not lose any, thus
   * from this point down interrupts must be disabled.
   */
  
#ifndef NESTED_INTERRUPT_SUPPRESS
  irq_DISABLE();
#endif
  assert(local_irq_disabled());
  
  /* 
   * If the activity is yielding voluntarily, it MUST be rescheduled.
   * 
   * If the current activity is a user activity, it is possible that
   * having completed the interrupt means that the current activity
   * needs to be reprepared, or that the activity has faulted. If the
   * activity has faulted, it has not yielded, as we need to know in
   * order to migrate the activity to the keeper.
   * 
   * It is also possible that in attempting to reprepare the current
   * activity, we will discover that the activity has died.  This can
   * happen if a domain rescinds itself.
   * 
   * Rather than try to deal with all of this in multiple places, we
   * unconditionally call Activity::Resched().  If appropriate,
   * Activity::Resched() will simply return the current activity in
   * prepared form, and we will return to it.  If the activity should
   * yield unconditionally, we tell Activity::Resched() so.
   * 
   */
  
  assert(local_irq_disabled());

  assert(saveArea);
  if (sa_IsProcess(saveArea)) {
    ExitTheKernel();	// does not return
  }

  assert(local_irq_disabled());

  /* We are about to do a return from interrupt, which path must not
   * be interrupted.  Disable interrupts prior to return:
   */
  assert(local_irq_disabled());

  resume_from_kernel_interrupt(saveArea);
}

