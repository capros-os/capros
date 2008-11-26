/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
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

#include <kerninc/kernel.h>
#include <kerninc/Machine.h>
#include <kerninc/IRQ.h>
#include <kerninc/dma.h>
#include <kerninc/Key.h>
#include <kerninc/SysTimer.h>
#include <kerninc/KernStream.h>
#include <kerninc/Process-inline.h>
#include <kerninc/mach-rtc.h>
#include <eros/arch/arm/mach-ep93xx/ep9315-syscon.h>
#include <arch-kerninc/kern-target-asm.h>
#include <arch-kerninc/PTE.h>

#define SYSCON (SYSCONStruct(APB_VA + SYSCON_APB_OFS))

void InterruptInit(void);
void mach_InitHardClock(void);

void
mach_FlushTLBsCaches(void)
{
  mach_DoMapWork(MapWork_UserTLBWrong
                 | MapWork_UserCacheWrong | MapWork_UserDirtyWrong);
}

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
  
  irq_ENABLE_for_IRQ();
  
  mach_InitHardClock();

  int RtcInit(void);
  int err = RtcInit();
  (void)err;
  assert(!err);
  
#ifdef OPTION_DDB
  kstream_dbg_stream->EnableDebuggerInput();
#endif

  printf("Interrupts initialized\n");
}

void
mach_HardReset()
{
  uint32_t dc = SYSCON.DeviceCfg;
  SYSCON.SysSWLock = 0xaa;	/* unlock */
  SYSCON.DeviceCfg = dc | SYSCONDeviceCfg_SWRST;
  SYSCON.SysSWLock = 0xaa;	/* unlock */
  SYSCON.DeviceCfg = dc & ~SYSCONDeviceCfg_SWRST;
}

bool	// returns true if successful
mach_LoadAddrSpace(Process * proc)
{
  kpa_t newTTBR = proc->md.firstLevelMappingTable;
  uint32_t pid = proc->md.pid;
  uint32_t dacr = proc->md.dacr;

  // Validate everything, because proc is a user-supplied address.

  if (newTTBR & 0x3fff) {	// not aligned
    printf("TTBR invalid.\n");
    return false;
  }
  uint32_t km = FLPT_FCSEVA[KTextVA >> L1D_ADDR_SHIFT];
  // Must point to good memory.
  uint32_t * newTTBRVA = KPAtoP(uint32_t *, newTTBR);
  uint32_t * ttbrEntVA = &newTTBRVA[KTextVA >> L1D_ADDR_SHIFT];
  uint8_t b;
  if (! SafeLoadByte((uint8_t *)ttbrEntVA, &b)	// Must point to good memory.
      || *ttbrEntVA != km ) {	// Must map the kernel.
    printf("TTBR invalid.\n");
    return false;
  }

  if (pid & ~ PID_MASK
      || pid >= (NumSmallSpaces << PID_SHIFT)) {
    printf("PID invalid.\n");
    return false;
  }

  if (dacr & 0xaaaaaaaa		// only client or no access
      || ! (dacr & 0x1) ) {	// must have client access to domain 0
    printf("DACR invalid.\n");
    return false;
  }

  kpa_t oldTTBR = mach_ReadTTBR();
  if (newTTBR != oldTTBR) {
    mach_LoadTTBR(newTTBR);
    mach_FlushTLBsCaches();
  }
  mach_LoadPID(pid);
  mach_LoadDACR(dacr);
  return true;
}
