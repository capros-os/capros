#ifndef __MACHINE_H__
#define __MACHINE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group.
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

/* This class captures the coupling between machine dependent and
 * machine independent initialization.
 */
#include <eros/TimeOfDay.h>


enum mach_BusType {
  bt_Unknown,			/* unknown bus type */
  bt_ISA,			/* x86 ISA bus */
  bt_EISA,			/* x86 EISA bus */
  bt_MCA,			/* x86 MCA bus */
  bt_PCI,			/* PCI bus */
  bt_VME,			/* VME bus */
  bt_SBUS,			/* sun SBUS */
} ;


void mach_BootInit();	/* machine DEPENDENT */
void mach_InitCoreSpace();	/* machine DEPENDENT */
void mach_InitHardClock();	/* machine DEPENDENT */

/* Hardware clock support: */
uint64_t mach_MillisecondsToTicks(uint64_t ms);
uint64_t mach_TicksToMilliseconds(uint64_t ticks);
void mach_SpinWaitUs(uint32_t us);

#ifdef EROS_HAVE_FPU
/* FPU support */
void mach_InitializeFPU();
void mach_DisableFPU();
void mach_EnableFPU();
#endif

/*typedef struct TimeOfDay TimeOfDay;*/

void mach_GetHardwareTimeOfDay(TimeOfDay*);

void mach_HardReset();

/* Mounting a disk volume is machine specific: */
typedef struct DiskUnit DiskUnit;
void mach_MountDisk(DiskUnit*);

void mach_EnsureHeap(kva_t target,
       kpa_t (*acquire_heap_page)(void) );

void mach_ZeroUnmappedPage(kpa_t page);

#include <arch-kerninc/Machine-inline.h>

#endif /* __MACHINE_H__ */
