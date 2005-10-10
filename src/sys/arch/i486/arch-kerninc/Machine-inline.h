/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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
#ifndef __MACHINE_INLINE_H__
#define __MACHINE_INLINE_H__

INLINE void
mach_FlushTLB()
{
  __asm__ __volatile__("mov %%cr3,%%eax;mov %%eax,%%cr3"
		       : /* no output */
		       : /* no input */
		       : "eax");
}

#ifdef __cplusplus
extern "C" {
#endif
  extern uint32_t CpuType;
#ifdef __cplusplus
};
#endif

INLINE void
mach_FlushTLBWith(klva_t lva)
{
#ifdef SUPPORT_386
  if (CpuType > 3)
    __asm__ __volatile__("invlpg  (%0)"
			 : /* no output */
			 : "r" (lva)
			 );
  else
    mach_FlushTLB();
#else
  __asm__ __volatile__("invlpg  (%0)"
		       : /* no output */
		       : "r" (lva)
		       );
#endif
}



INLINE uint16_t
mach_htonhw(uint16_t hw)
{
  uint8_t *bhw = (uint8_t *) &hw;
  uint8_t tmp = bhw[0];
  bhw[0] = bhw[1];
  bhw[1] = tmp;

  return hw;
}



INLINE uint16_t
mach_ntohhw(uint16_t hw)
{
  uint8_t *bhw = (uint8_t *) &hw;
  uint8_t tmp = bhw[0];
  bhw[0] = bhw[1];
  bhw[1] = tmp;

  return hw;
}



INLINE uint32_t
mach_htonw(uint32_t w)
{
  uint8_t *bw = (uint8_t *) &w;
  uint8_t tmp = bw[0];
  bw[0] = bw[3];
  bw[3] = tmp;
  tmp = bw[1];
  bw[1] = bw[2];
  bw[2] = tmp;

  return w;
}



INLINE uint32_t
mach_ntohw(uint32_t w)
{
  uint8_t *bw = (uint8_t *) &w;
  uint8_t tmp = bw[0];
  bw[0] = bw[3];
  bw[3] = tmp;
  tmp = bw[1];
  bw[1] = bw[2];
  bw[2] = tmp;

  return w;
}



INLINE int
mach_FindFirstZero(uint32_t w)
{
  __asm__("bsfl %1,%0"
	  :"=r" (w)
	  :"r" (~w));
  return w;
}



/* On the x86, this is handled by the object cache logic. */
INLINE void
mach_MarkMappingsForCOW()
{
}

INLINE kva_t
mach_GetCPUStackTop()
{
  extern uint32_t kernelStack[];
  return (kva_t)&kernelStack + EROS_KSTACK_SIZE;
}

#endif/*__MACHINE_INLINE_H__*/
