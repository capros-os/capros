/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
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

/* Driver for 386 debugger traps/faults */

#include <kerninc/kernel.h>
#include <kerninc/Activity.h>
#include <kerninc/Debug.h>
#include <kerninc/Machine.h>
/*#include <kerninc/util.h>*/
#include <kerninc/Process.h>
#include "IDT.h"
#include "lostart.h"

#ifdef OPTION_DDB
#include "Debug386.h"
#endif

#define DBG_STATUS_B0   0x1u
#define DBG_STATUS_B1   0x2u
#define DBG_STATUS_B2   0x4u
#define DBG_STATUS_B3   0x8u
#define DBG_STATUS_BD   0x2000u
#define DBG_STATUS_BS   0x4000u
#define DBG_STATUS_BT   0x8000u

#define DBG_CTRL_L0     0x1u
#define DBG_CTRL_G0     0x2u
#define DBG_CTRL_L1     0x4u
#define DBG_CTRL_G1     0x8u
#define DBG_CTRL_L2     0x10u
#define DBG_CTRL_G2     0x20u
#define DBG_CTRL_L3     0x40u
#define DBG_CTRL_G3     0x80u
#define DBG_CTRL_LE     0x100u
#define DBG_CTRL_GE     0x200u
#define DBG_CTRL_GD     0x2000u
#define DBG_CTRL_RW0    0x30000u
#define DBG_CTRL_LEN0   0xc0000u
#define DBG_CTRL_RW1    0x300000u
#define DBG_CTRL_LEN1   0xc00000u
#define DBG_CTRL_RW2    0x3000000u
#define DBG_CTRL_LEN2   0xc000000u
#define DBG_CTRL_RW3    0x30000000u
#define DBG_CTRL_LEN3   0xc0000000u

bool
DebugException(savearea_t* sa)
{
  /* Debug exception.  This can be one of a variety of different
   * things.  Unfortunately, it can mean a variety of things, so some
   * work is needed to puzzle things out.  It might be any of:
   * 
   * 	sstep trap	in which case PC points to the instruction
   *                    FOLLOWING the instruction that was
   *                    single-stepped.
   * 
   *    ifetch fault    Trap was from an instruction fetch watchpoint,
   * 			in which event the PC points to the
   * 			instruction we were about to execute.
   * 
   *    data trap       Trap was from a data reference (either read or
   * 			write), in which event the PC points to the
   * 			instruction AFTER the offending instruction
   * 
   * What a mess!  The Pentium compounds all of this by allowing I/O
   * watchpoints as well, which we do not currently support.
   * 
   * Just to make things really amusing, the machine may report any or
   * all of these exceptions simultaneously, which is truly gross.
   * If I read the manual correctly, the machine may simultaneously
   * report data and single step faults on one instruction and an
   * instruction fetch fault on the next.
   * 
   * All I have to say about that is YUCK.
   * 
   * In all of this, there is one saving grace.  We don't yet support
   * the debug registers for user domains, and the only kernel use of
   * them is for watchpoints in support of my general paranoia.
   */

  uint32_t dbStatus, dbControl, watch[4];
  __asm__("movl %%dr6,%0"
	  : "=r" (dbStatus)
	  : /* no inputs */
	  );
  __asm__("movl %%dr7,%0"
	  : "=r" (dbControl)
	  : /* no inputs */
	  );

  __asm__("movl %%dr0,%0"
	  : "=r" (watch[0])
	  : /* no inputs */
	  );
  __asm__("movl %%dr1,%0"
	  : "=r" (watch[1])
	  : /* no inputs */
	  );
  __asm__("movl %%dr2,%0"
	  : "=r" (watch[2])
	  : /* no inputs */
	  );
  __asm__("movl %%dr3,%0"
	  : "=r" (watch[3])
	  : /* no inputs */
	  );

#if 0
  if ( sa_IsKernel(sa) ) {
    /* Figure out if this is just a trace point: */
    bool justTrace = true;
    bool shouldShow = false;
  
    for (uint32_t i = 0; i < 4; i++) {
      bool fired =
	(dbStatus & (1 << i)) && ((dbControl >> (2 * i)) & 0x3);
  
      if (fired && Watchpoint::IsTracePt[i] == false)
	justTrace = false;

      if (fired && Watchpoint::goodThreads[i] == 0)
	shouldShow = true;
    
      if (fired && Watchpoint::goodThreads[i] &&
	  Watchpoint::goodThreads[i] != Thread::Current())
	justTrace = false;
    }

    if (justTrace == false)
      shouldShow = true;
    
    ArchContext* ctxt = (ArchContext*) Thread::CurContext();
  
    /*  if (shouldShow) */
    printf("Debug %s.  (%s) context = 0x%08x, eip = 0x%08x\n",
		   (justTrace ? "trace" : "exception"),
		   Thread::Current()->Name(),
		   ctxt, ctxt->fixRegs.EIP);

    static uint32_t BptLength[4] = { 1, 2, 0 /* undefined */, 4 };
    static char *BptType[4] = { "IF", "Wr", "IO" /* pentium only */, "RW" };

    if (shouldShow) {
      for (uint32_t i = 0; i < 4; i++) {
	bool fired =
	  (dbStatus & (1 << i)) && ((dbControl >> (2 * i)) & 0x3);

	uint32_t bptInfo = ((dbControl >> 16) >> (i * 4)) & 0xfu;
	uint32_t bptType = bptInfo & 3;
	uint32_t bptLen = (bptInfo >> 2) & 3;

	printf("  [%d] %c 0x%08x (%s) len=%d   ",
		       i,
		       (fired ? '*' : ' '),
		       watch[i],
		       BptType[bptType],
		       BptLength[bptLen]);
	if (i % 2 == 1)
	  printf("\n");
      }

      if (dbStatus & 0x8000u)
	printf("  * Single step completed\n");
    }
  }
#endif

#ifdef OPTION_DDB
  if (sa_IsKernel(sa)) {

    extern bool db_in_single_step();
    
    if (db_in_single_step())
      if (kdb_trap(sa->ExceptNo, sa->Error, sa))
	return false;
  }
#endif

#ifdef OPTION_DDB
  if ( sa_IsProcess(sa) ) {
    if (dbStatus & DBG_STATUS_BS) {
      dprintf(true,
	      "User process has single stepped. EIP=0x%08x\n", 
	      sa->EIP);
      sa->EFLAGS &= ~MASK_EFLAGS_Trap;
    }
  }
#endif

  return false;
}
