/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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
#include <kerninc/IRQ.h>
#include <kerninc/Activity.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Debug.h>
#include <kerninc/SysTimer.h>
#include <kerninc/Process.h>
#include <kerninc/KernStats.h>
#include <eros/arch/i486/io.h>
#include "lostart.h"
#include "IDT.h"
#include "GDT.h"

/* #define TIMING_DEBUG */

extern void _start();
extern void etext();

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
  if (eip >= (uint32_t)_start && eip < (uint32_t)etext)
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
  uint32_t oDisableDepth = irq_DISABLE_DEPTH();

#endif
  
  uint32_t vecNumber = saveArea->ExceptNo;

  KernStats.nInter++;
      

  assert( irq_DISABLE_DEPTH() == 1 || vecNumber < iv_IRQ0 );

  assert ( (GetFlags() & MASK_EFLAGS_Interrupt) == 0 );

#ifndef NDEBUG
  /* If we interrupted an invocation, there is no guarantee that
   * there exists a current activity -- we may have interrupted the
   * invocation path just after the current activity has been deleted.
   */

  curActivity = act_Current();

  
  assert(curActivity || !sa_IsProcess(saveArea));
#endif

#ifndef NDEBUG
  if (dbg_inttrap)
    Debugger();
#endif

#ifndef NDEBUG
  /* NOTE: There was a bug here in which a timer interrupt that nailed
   * the kernel in nested fashion could trigger a context check while
   * something critical was being updated. */
  
  if (   (irq_DISABLE_DEPTH() == 1)	/* avoid recursive fault */
      && (sa_IsProcess(saveArea)
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

  if ( saveArea == 0 ||
       ( sa_IsKernel(saveArea) && !ValidEIP(saveArea->EIP) ) ) {
    /* halt('e'); */
    fatal("Bogus save area 0x%08x vecno %d\n"
		  "   EIP=0x%08x CurActivity = %s ctxt=0x%08x\n",
		  saveArea, vecNumber,
		  saveArea ? saveArea->EIP : 0,
    
                  act_Name(curActivity), act_CurContext());
    
  }
#endif
  

  /* If we interrupted a activity, remember where the saved context
   * was.  For user activities, this is redundant, because it is the same
   * as the context that is already saved.  For kernel activities, this
   * is vital, as without it we won't be able to restart the activity.
   * Careful, though -- if this is a nested fault we don't want to
   * overwrite the old value.
   */
  
  if (sa_IsProcess(saveArea)) {
#ifndef NDEBUG
    
    savearea_t *oldsa = proc_UnsafeSaveArea(act_CurContext());
    
    
    if ( oldsa != saveArea ) {
      printf("ex=0x%x err=0x%x, eip=0x%08x\n",
	     saveArea->ExceptNo,
	     saveArea->Error,
	     saveArea->EIP);
      fatal("in: CurActivity is 0x%08x old saveArea 0x%08x, "
		    "saveArea = 0x%08x\n",
		 curActivity, oldsa, saveArea);
    }
#endif
    
    proc_SetSaveArea(act_CurContext(), saveArea);
  }
  
 
  assert( irq_DISABLE_DEPTH() == 1 || vecNumber < iv_IRQ0 );
 

  /* We have now done all of the processing that must be done with
   * interrupts disabled.  Re-enable interrupts here:
   */

#ifndef NESTED_INTERRUPT_SUPPRESS
 
  irq_ENABLE();
  
  assert( irq_DISABLE_DEPTH() == 0 || vecNumber < iv_IRQ0 );
 
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

#ifndef NESTED_INTERRUPT_SUPPRESS
 
  assert( irq_DISABLE_DEPTH() == 0 || vecNumber < iv_IRQ0 );
 
#endif

 
  assert ( act_Current() || !sa_IsProcess(saveArea));
 

  /* We are going to process all pending interrupts and then return to
   * the activity.  We need to make sure that we do not lose any, thus
   * from this point down interrupts must be disabled.
   */
  
#ifndef NESTED_INTERRUPT_SUPPRESS
 
  irq_DISABLE();
#endif
  assert( irq_DISABLE_DEPTH() == 1 || vecNumber < iv_IRQ0 );
 
  
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
  
  assert ( (GetFlags() & MASK_EFLAGS_Interrupt) == 0 );
  assert( irq_DISABLE_DEPTH() == 1 || vecNumber < iv_IRQ0 );

  
  if (sa_IsProcess(saveArea)) {
    act_Reschedule();

    /* We succeeded (wonder of wonders) -- release pinned resources. */
    objH_ReleasePinnedObjects();
    
#if 0
    /* Since we succeeded, there are no uncommitted I/O page frames: */
    ObjectCache::ReleaseUncommittedIoPageFrames();
#endif

#if 0
    printf("Return from resched\n");
#endif
 
    assert ( act_Current());
 
    saveArea = proc_UnsafeSaveArea(act_CurContext());
 

    if (saveArea == 0)
      fatal("Activity 0x%08x is not runnable (no saveArea)\n",
	    act_Current()); 
  }
  
  assert( irq_DISABLE_DEPTH() == 1 || vecNumber < iv_IRQ0 );

#ifndef NDEBUG
  if ( saveArea == 0 ) {
    printf("Restore from invalid save area 0x%08x\n"
           "   EIP=0x%08x CurActivity = %s ctxt=0x%08x\n",
               saveArea, saveArea ? saveArea->EIP : 0,
               act_Name(act_Current()),
          
               act_CurContext());
   
    printf("  CS=0x%02x, int#=0x%x, err=0x%x, flg=0x%08x\n",
                 saveArea->CS,
		 saveArea->ExceptNo,
		 saveArea->Error,
		 saveArea->EFLAGS);
    debug_Backtrace(0, true);
  }
#endif
    
  /* We are returning to a previous interrupt or to a activity, and we
   * need to restore the interrupt level that was effective in that
   * context.  CATCH: if interrupts were enabled in that context we do
   * not want them to get enabled here; we'ld rather wait until the
   * RETI which will enable them when EFLAGS is restored.  We
   * therefore call setspl() rather than splx().  setspl() adjusts the
   * PIC masks appropriately [or it eventually will], but does not
   * enable interrupts.
   * 
   * Note that a yielding activity should always be running at
   * splyield() [all interrupts enabled], so the fact that we may not
   * be returning to the same activity is not a problem in restoring the
   * current spl.
   */

  /* We are about to do a return from interrupt, which path must not
   * be interrupted.  Disable interrupts prior to return:
   */
  assert ( (GetFlags() & MASK_EFLAGS_Interrupt) == 0 );
  assert( irq_DISABLE_DEPTH() == 1 || vecNumber < iv_IRQ0 );

#ifndef NDEBUG
  assert ( oDisableDepth == irq_DISABLE_DEPTH() );
#endif

  /* Otherwise resume interrupted activity: */
  if (sa_IsProcess(saveArea)) {
    act_Resume(act_Current());
  }
  else
    resume_from_kernel_interrupt(saveArea);
}

