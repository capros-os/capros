/*
 * Copyright (C) 2008, Strawberry Development Group.
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

/* w1mult.h */

#include <stdint.h>
#include <eros/target.h>

// The family code is the low byte of the ROM ID.
enum {
  famCode_DS18B20 = 0x28,	// temperature
  famCode_DS2409  = 0x1f,	// coupler2438
  famCode_DS2438  = 0x26,	// battery monitor
  famCode_DS2450  = 0x20,	// A-D
  famCode_DS2502  = 0x09,	// a DS9097U has one of these
  famCode_DS9490R = 0x81,	// custom DS2401 in a DS9490R
};

enum {
  branch_main = 0xcc,
  branch_aux  = 0x33
};

struct W1DevConfig {
  short thisIndex;	// -1 means end of entries
  short parentIndex;	// -1 means no parent
  uint8_t mainOrAux;
  uint64_t rom;
};
