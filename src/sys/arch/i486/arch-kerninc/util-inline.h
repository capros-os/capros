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

/* Unfortunately, using the GNU inliner has the side effect of
   defeating our ability to use __builtin_constant_p.  This makes
   quite a difference, so bcopy() is implemented as a macro.
 */

INLINE void
__bcopy_by4(const void * from, const void * to, size_t len)
{
  __asm__ __volatile__ ("cld\n\t"
			"rep\n\t"
			"movsl\n\t"
			: /* no output */
			:"c" (len>>2),"S" (from),"D" (to)
			:"cx","si","di","memory");
}

INLINE void
__bcopy_by2(const void * from, const void * to, size_t len)
{
  __asm__ __volatile__ ("cld\n\t"
			"rep\n\t"
			"movsw\n\t"
			: /* no output */
			:"c" (len>>1),"S" (from),"D" (to)
			:"cx","si","di","memory");
}

#if 0
INLINE void
__bcopy_g(const void * from, const void * to, size_t n)
{
  __asm__ __volatile__ (
			"cld\n\t"
			"shrl $1,%%ecx\n\t"
			"jnc 1f\n\t"
			"movsb\n"
			"1:\tshrl $1,%%ecx\n\t"
			"jnc 2f\n\t"
			"movsw\n"
			"2:\trep\n\t"
			"movsl"
			: /* no output */
			:"c" (n),"D" ((long) to),"S" ((long) from)
			:"cx","di","si","memory");
}
#endif

INLINE void
__bcopy_g(const void *from, void *to, size_t len)
{
  __asm__ __volatile__ ("cld\n\t"
			"rep\n\t"
			"movsw\n\t"
			: /* no output */
			:"c" (len),"S" (from),"D" (to)
			:"cx","si","di","memory");
}

#define __bcopy_const(s,d,count) \
((count%4==0) ? \
 __bcopy_by4((s),(d),(count)) : \
 ((count%2==0) ? \
  __bcopy_by2((s),(d),(count)) : \
  __bcopy_g((s),(d),(count))))

#define bcopy(s,d,count) \
(__builtin_constant_p(count) ? \
 __bcopy_c((s),(d),(count)) : \
 __bcopy_g((s),(d),(count)))

