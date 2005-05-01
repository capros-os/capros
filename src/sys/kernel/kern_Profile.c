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

#ifdef OPTION_KERN_PROFILE

#include <kerninc/kernel.h>
#include <kerninc/PhysMem.h>
#include <kerninc/MsgLog.h>
#include <kerninc/util.h>
#include <eros/memory.h>

extern "C" {
  extern void etext();
#if 0
  extern void start();
#endif
  
  extern uint32_t* KernelProfileTable;
}

void
InitKernelProfiler()
{
  uint32_t kernelCodeLength = (uint32_t) etext;

  /* One profile word for every 16 bytes: */
  
  uint32_t tableSize = (kernelCodeLength >> 4);

  KernelProfileTable = ::new (0) uint32_t[tableSize];
}

#endif
