#ifndef __MACHINE_INLINE_H__
#define __MACHINE_INLINE_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2007, Strawberry Development Group.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

struct ObjectHeader;
extern volatile uint64_t sysT_now;

#if 0 /* this is unused */
INLINE int
mach_FindFirstZero(uint32_t w)
{
  __asm__("bsfl %1,%0"
	  :"=r" (w)
	  :"r" (~w));
  return w;
}
#endif

INLINE kva_t
mach_GetCPUStackTop()
{
  extern uint32_t kernelStack[];
  return (kva_t)&kernelStack + EROS_KSTACK_SIZE;
}

INLINE uint64_t 
sysT_Now()
{
  uint64_t t1;
  uint64_t t2;
  
  do {
    t1 = sysT_now;
    t2 = sysT_now;
  } while (t1 != t2);
  
  return t1;
}

#endif/*__MACHINE_INLINE_H__*/
