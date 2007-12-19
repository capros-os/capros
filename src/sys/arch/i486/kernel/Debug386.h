#ifndef __DEBUG386_H__
#define __DEBUG386_H__
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

extern bool kdb_trap(int type, int code, struct savearea * regs);

#if 0
#include <kerninc/kernel.hxx>

struct Thread;

struct Watchpoint {
  enum WatchType {
    fetch = 0x0u,
    write = 0xdu,		/* len=11 ty=01 */
    io = 0x2u,			/* len=00 ty=10 */
    readwrite = 0xfu,		/* len=11 ty=11 */
  };

  static bool IsTracePt[4];
  static Thread* goodThreads[4];
  
};

INLINE void 
watchP_StopOn(uint32_t n, kva_t watchAddr, WatchType ty)
{
  uint32_t dbCtrlReg;
  
  assert (n < 4);
  assert (ty < 16);
  
  __asm__("movl %%dr7,%0"
	  : "=r" (dbCtrlReg)
	  : /* no inputs */
	  );

  uint32_t mask = 0xfu << (16 + 4 * n);
  mask |= (0x3u << n);
  dbCtrlReg &= ~mask;

  dbCtrlReg |= (ty << (16 + 4 * n));	/* all word sizes, r/w bpts */
  dbCtrlReg |= (0x3u << n);

  switch(n) {
  case 0:
    __asm__("movl %0,%%dr0"
	    : /* no outputs */
	    : "r" (watchAddr)
	    );
    break;
  case 1:
    __asm__("movl %0,%%dr1"
	    : /* no outputs */
	    : "r" (watchAddr)
	    );
    break;
  case 2:
    __asm__("movl %0,%%dr2"
	    : /* no outputs */
	    : "r" (watchAddr)
	    );
    break;
  case 3:
    __asm__("movl %0,%%dr3"
	    : /* no outputs */
	    : "r" (watchAddr)
	    );
    break;
  }

  __asm__("movl %0,%%dr7"
	  : /* no outputs */
	  : "r" (dbCtrlReg)
	  );

  /* Issue lldt to make sure the processor notices the changes: */
  __asm__("xorl %%eax,%%eax;lldt %%eax"
	  : /* no outputs */
	  : /* no inputs */
	  : "eax"
	  );

  IsTracePt[n] = false;
  goodThreads[n] = 0;
}

INLINE void 
watchP_TraceOn(uint32_t n, kva_t watchAddr, WatchType ty)
{
  StopOn(n, watchAddr, ty);
  IsTracePt[n] = true;
  goodThreads[n] = 0;
}

INLINE void 
watchP_TraceThread(uint32_t n, kva_t watchAddr, WatchType ty,
                        Thread *thread)
{
  StopOn(n, watchAddr, ty);
  IsTracePt[n] = true;
  goodThreads[n] = thread;
}

INLINE void 
watchP_Clear(uint32_t n)
{
  uint32_t dbCtrlReg;
  
  assert (n < 4);
  
  __asm__("movl %%dr7,%0"
	  : "=r" (dbCtrlReg)
	  : /* no inputs */
	  );

  dbCtrlReg &= ~(0x3u << n);

  __asm__("movl %0,%%dr7"
	  : /* no outputs */
	  : "r" (dbCtrlReg)
	  );

  /* Issue lldt to make sure the processor notices the changes: */
  __asm__("xorl %%eax,%%eax;lldt %%eax"
	  : /* no outputs */
	  : /* no inputs */
	  : "eax"
	  );
}

#endif
#endif /* __DEBUG386_H__ */
