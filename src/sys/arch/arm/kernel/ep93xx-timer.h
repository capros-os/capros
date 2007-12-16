#ifndef __EP93XX_TIMER_H_
#define __EP93XX_TIMER_H_
/*
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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

#include <stdint.h>
#include <eros/arch/arm/mach-ep93xx/ep9315.h>

/* Declarations for the Cirrus EP9315 Timer. */

#define TimerControlEnable 0x80
#define TimerControlModePeriodic 0x40
#define TimerControlModeFreeRun  0x00
#define TimerControlClksel508k 0x08
#define TimerControlClksel2k   0x00

struct TimerRegs {
  uint32_t Load;
  uint32_t Value;
  uint32_t Control;
  uint32_t Clear;
};

typedef struct TimersRegisters {
  struct TimerRegs Timer1;
  uint32_t unused0[4];
  struct TimerRegs Timer2;
  uint32_t unused1[12];
  uint32_t Timer4ValueLow;
  uint32_t Timer4ValueHigh;
  uint32_t unused3[6];
  struct TimerRegs Timer3;
} TimersRegisters;

#define TimersStruct(x) (*(volatile struct TimersRegisters *)(x))

#endif /* __EP93XX_TIMER_H_ */
