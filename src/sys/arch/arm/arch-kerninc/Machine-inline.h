#ifndef __MACHINE_INLINE_H__
#define __MACHINE_INLINE_H__
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

#include <kerninc/ObjectHeader.h>

/* sysT_Now is not inline in this architecture. */
uint64_t sysT_Now(void);

INLINE kva_t
mach_GetCPUStackTop()
{
  extern kva_t kernelStackBot;
  return kernelStackBot;
}

INLINE void
mach_InvalidateProducts(ObjectHeader * thisPtr)
{
  if (thisPtr->ssid) {
    fatal("Unimplemented mach_InvalidateProducts\n");
  }
}

#endif/*__MACHINE_INLINE_H__*/
