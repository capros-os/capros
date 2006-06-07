/*
 * Copyright (C) 2006, Strawberry Development Group
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
#include <kerninc/StallQueue.h>
#include <arch-kerninc/IRQ-inline.h>
#include "ep93xx-vic.h"
#include "Interrupt.h"

void InitExceptionHandlers(void);

#define dbg_init	0x1u

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init)

#define DEBUG(x) if (dbg_##x & dbg_flags)

VICIntSource VICIntSources[NUM_INTERRUPT_SOURCES];

typedef struct VICVectoredInt {
  /* source is 
     -1 if disabled
     0 to 63 if assigned to that interrupt source */
  signed char source;
} VICVectoredInt;

/* There is a VICInfo structure for each of the two VICs. */
typedef struct VICInfo {
  /* ISRAddr must be the first item. */
  void (*ISRAddr)(struct VICInfo *); // contains &VICNonvectoredHandler

  volatile struct VICRegisters * regs;
  VICIntSource * sourceArray;
  uint32_t nonvectoredSources;
  VICVectoredInt vectors[16];
} VICInfo;

VICInfo VICInfos[2];	// VICInfo for VIC1 and VIC2

void
VICNonvectoredHandler(VICInfo * vicInfo)
{
#if 1
  printf("Nonvectored interrupt handler\n");
#endif
  int i;
  /* Find the nonvectored source that caused this interrupt.
     If there is more than one, find any one. */
  uint32_t intStatus = vicInfo->regs->IRQStatus & vicInfo->nonvectoredSources;
  for (i = 0; i < 32; i++) {
    if (intStatus & 0x1) {	// found one
      VICIntSource * vis = &vicInfo->sourceArray[i];
      (vis->ISRAddr)(vis);	// Call the ISR
    }
    intStatus >>= 1;
  }
}

/* Handler for interrupts using the DevicePrivs key. */
void
DoUsermodeInterrupt(VICIntSource * vis)
{
  unsigned int sourceNum = vis->sourceNum;
  if (sourceNum >= 32) {	// if on VIC2
    // read VectAddr to mask interrupts of lower or equal priority
    (void)VIC2.VectAddr;	
  }
  irq_ENABLE();
#if 1
  printf("Waking sleeper for int source %d\n", sourceNum);
#endif

  // Disable the interrupt so it does not recur immediately.
  InterruptSourceDisable(sourceNum);

  vis->isPending = true;
  sq_WakeAll(&vis->sleeper, false);

  irq_DISABLE();

  // write VectAddr to reenable interrupts of lower or equal priority
  if (sourceNum >= 32) {	// if on VIC2
    VIC2.VectAddr = 0;;
  } else {
    VIC1.VectAddr = 0;;
  }
}

static void
VICInit(unsigned vicNum, volatile struct VICRegisters * VIC)
{
  VICInfo * info = &VICInfos[vicNum];
  int i;

  info->ISRAddr = &VICNonvectoredHandler;
  info->regs = VIC;
  info->sourceArray = &VICIntSources[vicNum * 32];

  /* All sources are initially considered nonvectored, but since they
     are disabled, we don't need to include them in nonvectoredSources. */
  info->nonvectoredSources = 0;
  for (i=0; i < 16; i++) {
    info->vectors[i].source = -1;
    /* Vectors are disabled at reset. */
  }
  VIC->DefVectAddr = (uint32_t)info;
  // IntSelect defaults to IRQ.
  /* After reset, VIC->Protection defaults to allowing User mode access.
     That's OK, because we use the MMU to control access, not the mode. */
}

void
InterruptInit(void)
{
  int i;
  InitExceptionHandlers();
  VICInit(0, &VIC1);
  VICInit(1, &VIC2);
/* Note, for reference, the following two values enable interrupts
   that are not observed to occur right after reset.
   Other interrupts will occur if enabled.
  VIC1.IntEnable = 0xebfffff0;
  VIC2.IntEnable = 0xfefff8e6;
  */
  for (i=0; i < NUM_INTERRUPT_SOURCES; i++) {
    VICIntSource * vis = &VICIntSources[i];
    vis->priority = PRIO_Unallocated;
    vis->sourceNum = i;
    sq_Init(&vis->sleeper);
  }
}

void
UserIrqInit(void)
{
  // We initialized this as part of InterruptInit above.
}

/* priority is the priority within this source's VIC. */
void
InterruptSourceSetup(unsigned int source, int priority, ISRType handler)
{
  VICIntSource * vicSource = &VICIntSources[source];
  unsigned int source32 = source & 0x1f;
  uint32_t sourceBit = 1ul << source32;
  VICInfo * vicInfo = &VICInfos[source >> 5];

  vicSource->priority = priority;
  if (priority == -1) {	// FIQ
    vicInfo->regs->IntSelect |= sourceBit;
    /* handler is not used */
  } else {		// IRQ
    vicSource->ISRAddr = handler;
    vicInfo->regs->IntSelect &= ~sourceBit;
    if (priority == 16) {	// nonvectored
      vicInfo->nonvectoredSources |= sourceBit;
    } else {		// vectored
      if (vicInfo->vectors[priority].source != -1)
        fatal("Requesting interrupt source %d to vector %d,"
              " already assigned to %d\n", source, priority,
              vicInfo->vectors[priority].source);
#if 0
      printf("Init Vect Int source %d vect %d\n", source, priority);
#endif
      vicInfo->vectors[priority].source = source;
      vicInfo->regs->VectAddrN[priority] = (uint32_t)vicSource;
      vicInfo->regs->VectCntlN[priority] = VIC_VectCntl_Enable + source32;
    }
  }
}

void
InterruptSourceUnset(unsigned int source)
{
  VICIntSource * vicSource = &VICIntSources[source];
  unsigned int source32 = source & 0x1f;
  uint32_t sourceBit = 1ul << source32;
  VICInfo * vicInfo = &VICInfos[source >> 5];

  vicInfo->regs->IntEnClear = sourceBit;	// disable the interrupt

  const int priority = vicSource->priority;
  if (priority < 0) {
    // Was FIQ, nothing to do
  } else if (priority == 16) {	// was nonvectored
    vicInfo->nonvectoredSources &= ~sourceBit;
  } else {				// was vectored
    vicInfo->vectors[priority].source = -1;
    vicInfo->regs->VectCntlN[priority] = 0;	// disable vector
  }
  vicSource->priority = PRIO_Unallocated;
}

void
InterruptSourceEnable(unsigned int source)
{
  unsigned int source32 = source & 0x1f;	// source mod 32
  uint32_t sourceBit = 1ul << source32;
  VICInfo * vicInfo = &VICInfos[source >> 5];

  vicInfo->regs->IntEnable = sourceBit;
}

void
InterruptSourceDisable(unsigned int source)
{
  unsigned int source32 = source & 0x1f;	// source mod 32
  uint32_t sourceBit = 1ul << source32;
  VICInfo * vicInfo = &VICInfos[source >> 5];

  vicInfo->regs->IntEnClear = sourceBit;
}

void
InterruptSourceSoftwareIntGen(unsigned int source)
{
  unsigned int source32 = source & 0x1f;	// source mod 32
  uint32_t sourceBit = 1ul << source32;
  VICInfo * vicInfo = &VICInfos[source >> 5];

  vicInfo->regs->SoftInt = sourceBit;
}

void
InterruptSourceSoftwareIntClear(unsigned int source)
{
  unsigned int source32 = source & 0x1f;	// source mod 32
  uint32_t sourceBit = 1ul << source32;
  VICInfo * vicInfo = &VICInfos[source >> 5];

  vicInfo->regs->SoftIntClear = sourceBit;
}

