#ifndef __ARM_CAP_INSTR_H__
#define __ARM_CAP_INSTR_H__
/*
 * Copyright (C) 2006, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

/*
 * Routines to support direct capability register manipulation.
 * All of these are kernel-implemented pseudo-instructions.
 */

INLINE void
COPY_KEYREG(unsigned from, unsigned to)
{
  __asm__ __volatile__ ("swi 2");	/* SWI_CopyKeyReg */
}
     
INLINE void
XCHG_KEYREG(unsigned cr0, unsigned cr1)
{
  __asm__ __volatile__ ("swi 3");	/* SWI_XchgKeyReg */
}
     
#endif /* __ARM_CAP_INSTR_H__ */
