#ifndef __MACHINE_INLINE_H__
#define __MACHINE_INLINE_H__
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

struct ObjectHeader;

INLINE int
mach_FindFirstZero(uint32_t w)
{
  __asm__("bsfl %1,%0"
	  :"=r" (w)
	  :"r" (~w));
  return w;
}


INLINE kva_t
mach_GetCPUStackTop()
{
  extern uint32_t kernelStack[];
  return (kva_t)&kernelStack + EROS_KSTACK_SIZE;
}

INLINE void
mach_InvalidateProducts(struct ObjectHeader * thisPtr)
{ /* nothing to do */
}

#endif/*__MACHINE_INLINE_H__*/
