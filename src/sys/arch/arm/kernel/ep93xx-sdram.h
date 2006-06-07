#ifndef __EP9315_SDRAM_H_
#define __EP9315_SDRAM_H_
/*
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

#include <stdint.h>
#include "ep9315.h"

/* Declarations for the Cirrus EP9315 SDRAM. */

/* Bits for */

typedef struct SDRAMRegisters {
  uint32_t unused0;
  uint32_t GlConfig;
  uint32_t RefrshTimr;
  uint32_t BootSts;
  uint32_t SDRAMDevCfg0;
  uint32_t SDRAMDevCfg1;
  uint32_t SDRAMDevCfg2;
  uint32_t SDRAMDevCfg3;
} SDRAMRegisters;

#define SDRAM (*(volatile struct SDRAMRegisters *)SDRAM_BASE)

#endif /* __EP9315_SDRAM_H_ */
