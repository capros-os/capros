/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <kerninc/IRQ.h>
#include <kerninc/dma.h>
#include <kerninc/Key.h>
#include <kerninc/SysTimer.h>
#include "ep9315-syscon.h"

void map_HeapInit(void);
void InterruptInit(void);
void mach_InitHardClock(void);

/* mach_BootInit()
 * 
 * On entry:
 We have a stack (created in lostart.S).
 Interrupts are disabled.
 Static constructors have been run - not.
 The MMU and cache are enabled, and we are running with a map
   built by lostart.S. TTBR has FLPT_FCSE.
 Kernel streams are initialized (printf to console works).
 * 
 * Controllers and devices may be in arbitrarily inconsistent states.
 * 
 * On exit from this routine, the kernel
 should have performed enough
 * initialization to have a minimal interrupt table installed....
 * Interrupts are disabled on entry, and should be enabled on exit.
 * Note that on exit from this procedure the controller entry points
 * have not yet been established (that will happen next).
 * 
 * The implicit assumption here is that interrupt handling happens
 * in two tiers, and that the procesor interrupts can be enabled
 * without enabling all device interrupts.
 */

void 
mach_BootInit()
{
  map_HeapInit();
  
  InterruptInit();
  
  init_dma();
  
  assert(sizeof(uint16_t) == 2);
  assert(sizeof(uint32_t) == 4);
  assert(sizeof(void *) == 4);
  assert(sizeof(Key) == 16);
  assert(offsetof(KeyBits, keyType) == 12);

  /* Verify the queue key representation pun: */
  assert(sizeof(StallQueue) == 2 * sizeof(uint32_t));
  
  /*  printf("Pre enable: intdepth=%d\n", IDT::intdepth); */
  
#ifdef OPTION_KERN_PROFILE
  /* This must be done before the timer interrupt is enabled!! */
  extern void InitKernelProfiler();
  InitKernelProfiler();
#endif

  /* We enable IRQ here.  Note that at this
   * point all of the individual device interrupts are disabled.
   */
  
  irq_ENABLE();
  
  mach_InitHardClock();
  
#ifdef OPTION_DDB
  kstream_dbg_stream->EnableDebuggerInput();
#endif

  printf("Interrupts initialized\n");
}

void
mach_HardReset()
{
  uint32_t dc = SYSCON.DeviceCfg | SYSCONDeviceCfg_SWRST;
  SYSCON.SysSWLock = 0xaa;	/* unlock */
  SYSCON.DeviceCfg = dc;
}
