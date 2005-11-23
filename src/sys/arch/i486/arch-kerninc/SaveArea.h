#ifndef __SAVEAREA_H__
#define __SAVEAREA_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include "gen.SaveArea.h" 

#ifdef __KERNEL__
/* Could be either a kernel-mode or a user-mode process. */
INLINE bool
sa_IsProcess(const savearea_t* fx)
{
  /* Get the requested privilege level of the code segment selector.
     0 is for kernel
     1 is for kernel "processes"
     3 is for user code
   */
  if ( ((fx->CS & 0x3u) != 0x0u) || (fx->EFLAGS & MASK_EFLAGS_Virt8086) )
    return true;
  return false;
}

/* Kernel mode or kernel process are in rings 0, 1 */
INLINE bool
sa_IsKernel(const savearea_t* fx)
{
  if ( fx->CS &&
       ((fx->CS & 0x3u) < 0x2u) &&
       ((fx->EFLAGS & MASK_EFLAGS_Virt8086) == 0) )
    return true;
  return false;
}

#ifdef __cplusplus
extern "C" {
#endif
void DumpFixRegs(const savearea_t*);
#ifdef __cplusplus
}
#endif

#endif /* __KERNEL__ */

#endif /* __SAVEAREA_H__ */
