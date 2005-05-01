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

/* Real versions of debugger interfaces, so that we don't have
 * to do a full recompile to get a debugging-enabled kernel:
 */

#include <kerninc/kernel.h>
#include <kerninc/util.h>
#include <kerninc/Debug.h>
#include <kerninc/Activity.h>
#include <kerninc/SymNames.h>


extern int _start;
extern int etext;
extern int end;
extern void intr_entry();


static const struct FuncSym*
GetSymName(uint32_t address)
{
  uint32_t offset = ~0u;		/* maxword */
  const struct FuncSym *pEntry = 0;
  uint32_t i = 0;
  uint32_t newOffset = 0;

  if ( (address < (uint32_t) &_start) || (address >= (uint32_t) &etext) )
    return 0;
  
  /* Fix: this should eventually sort the name table the first time it
   * is called.
   */
  
  for (i = 0; i < funcSym_count; i++) {
    if (funcSym_table[i].address > address)
      continue;
    
    newOffset = address - funcSym_table[i].address;

    if (newOffset < offset) {
      pEntry = &funcSym_table[i];
      offset = newOffset;
    }
  }

  return pEntry;
}


void 
debug_Backtrace(const char *msg, bool shouldHalt)
{
  uint32_t *pFrame;
  uint32_t i = 0;
  
  /* ASM hack to capture the stack pointer: */
  __asm__("movl %%ebp,%0"
		: "=g" (pFrame)
	        : /* no inputs */);

  if (msg) {
    printf("%s\n", msg);
  }

  else if ( act_IsUser(act_Current()) ) {
    printf("Backtrace() called -- curthread (0x%08x) = %s\n",
		 act_Current(), act_Name(act_Current()));
  }

  else {
#if 0
    KernThread *pkt = (KernThread*) Thread::Current();
    Word* tStack = pkt->threadStack;
    Word* tStackTop = tStack + pkt->threadStackSize;
#endif


    printf("Backtrace() on 0x%08x (%s)\n",
		 act_Current(),
		 act_Name(act_Current()));

  }

  /*
   * pStack now points to the word in the stack containing the
   * return EBP for the 'backtrace()' procedure.  The word above
   * that ought to be the return PC.  Suppress the display of the
   * backtrace procedure itself...
   */

  for (i = 0; i < 7; i++) {
    uint32_t address = pFrame[1];
    const struct FuncSym *pSymEnt = GetSymName(address);

    if ( (address < (uint32_t) &_start) || (address >= (uint32_t) &etext) ) {
      printf("Non-text frame 0x%08x!\n", address);
      break;
    }

    if (pSymEnt) {
      printf(" %d: 0x%08x+%08x %s... ebp=0x%x\n",
		   i,
		   pSymEnt->address,
		   address - pSymEnt->address,
		   pSymEnt->name,
		   pFrame[0]);

      /* Walking back past _start is probably a bad idea... */
      if (pSymEnt->address == (uint32_t) &_start)
	break;

      /* If the return PC is in intr_entry, then we need to handle
       * things a bit differently, because the interrupt stack frame
       * is not like the others.  If this is the case, the stack looks
       * like:
       * 
       *  2:   pSaveArea  (arg to OnTrapOrInterrupt)
       *  1:   ret PC     (in intr_entry)
       *  0:   saved frame pointer to interrupt "frame"
       */

      if (pSymEnt->address == (uint32_t) intr_entry) {
	savearea_t *pSaveArea = (savearea_t*)  pFrame[2];
        static uint32_t forged_frame[2];

	printf("      sa=0x%08x cs=0x%02x eip=0x%08x fva=0x%08x\n"
		     "      int#=0x%x error=0x%x\n",
		     pSaveArea, pSaveArea->CS,
		     pSaveArea->EIP,
		     pSaveArea->ExceptAddr,
		     pSaveArea->ExceptNo,
		     pSaveArea->Error);
	
	if ( (pSaveArea->EFLAGS & MASK_EFLAGS_Virt8086) ||
	     ((pSaveArea->CS & ~0x3u) == 0x18 ) ) {
	  /* returning to non-kernel context */
	  printf("Interrupted user context\n");
	  break;
	}

	
	forged_frame[1] = pSaveArea->EIP;
	forged_frame[0] = pSaveArea->EBP;

	pFrame = forged_frame;
      }
      else {
	pFrame = (uint32_t *) pFrame[0];
      }

      if ( ((uint32_t)pFrame < (uint32_t) &etext) || ((uint32_t)pFrame >= (uint32_t) &end) ) {
	printf("Non-kernel frame 0x%08x!\n", pFrame);
	break;
      }


    }
    else {
      printf(" %d: 0x%08x+??? ??\?(...)...\n",
		   i, address);
      break;
    }
  }

  if (shouldHalt)
    halt('a');
}


