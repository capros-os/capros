#ifndef __I486_CAP_INSTR_H__
#define __I486_CAP_INSTR_H__

/*
 * Copyright (C) 1998, 1999, 2001, 2002 Jonathan S. Shapiro.
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

/*
 * Routines to support direct capability register manipulation.
 * All of these are kernel-implemented pseudo-instructions.
 */

INLINE void
COPY_KEYREG(unsigned from, unsigned to)
{
  __asm__ __volatile__ (
			"movl $0,%%eax\n\t"
			"int $0x32\n\t"
			: /* no outputs */
			: "b" (from), "c" (to) /* inputs */
			: "ax" /* clobbered */
			);
}
     
INLINE void
XCHG_KEYREG(unsigned cr0, unsigned cr1)
{
  __asm__ __volatile__ (
			"movl $1,%%eax\n\t"
			"int $0x32\n\t"
			: /* no outputs */
			: "b" (cr0), "c" (cr1) /* inputs */
			: "ax" /* clobbered */
			);
}
     
#endif /* __I486_CAP_INSTR_H__ */
