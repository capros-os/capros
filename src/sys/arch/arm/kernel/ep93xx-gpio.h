#ifndef __EP9315_GPIO_H_
#define __EP9315_GPIO_H_
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

/* Declarations for the Cirrus EP9315 GPIO. */

/* Bits for EEDrive */
#define GPIOEEDrive_DATOD 0x2
#define GPIOEEDrive_CLKOD 0x1

struct GPIOEnhanced {
  uint32_t IntType1;
  uint32_t IntType2;
  uint32_t EOI;
  uint32_t IntEn;
  uint32_t IntSts;
  uint32_t RawIntSts;
  uint32_t DB;
};

typedef struct GPIORegisters {
  uint32_t PADR;
  uint32_t PBDR;
  uint32_t PCDR;
  uint32_t PDDR;
  uint32_t PADDR;
  uint32_t PBDDR;
  uint32_t PCDDR;
  uint32_t PDDDR;
  uint32_t PEDR;
  uint32_t PEDDR;
  uint32_t unused0;
  uint32_t unused1;
  uint32_t PFDR;
  uint32_t PFDDR;
  uint32_t PGDR;
  uint32_t PGDDR;
  uint32_t PHDR;
  uint32_t PHDDR;
  uint32_t unused2;
  struct GPIOEnhanced FInt;
  uint32_t unused3[10];
  struct GPIOEnhanced AInt;
  struct GPIOEnhanced BInt;
  uint32_t EEDrive;
} GPIORegisters;

#define GPIOStruct(x) (*(volatile struct GPIORegisters *)(x))

#endif /* __EP9315_GPIO_H_ */
