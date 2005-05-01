#ifndef __IO_H__
#define __IO_H__
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

/* This file included from both C  and C++ code */

/* Port 0x80 is an unused port.  We read/write it to slow down the
 * I/O.  Without the slow down, the NVRAM chip get's into horrible
 * states.
 */

/* If your compiler complains that GNU_INLINE_ASM is not defined, you
 * will need to move these to a library somewhere. 
 */
INLINE void
outb(uint8_t value, uint16_t port)
{
  GNU_INLINE_ASM ("outb %b0,%w1"
	      : /* no outputs */
	      :"a" (value),"d" (port));
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#endif
}

#ifdef __KERNEL__

INLINE void
old_outb(uint16_t port, uint8_t value)
{
  outb(value, port);
}

#if 0
INLINE void
outw(uint16_t value, uint16_t port)
{
  outw(value, port);
}

INLINE void
outl(uint32_t value, uint16_t port)
{
  outl(value, port);
}
#endif

#endif

INLINE uint8_t
inb(uint16_t port)
{
  unsigned int _v;
  GNU_INLINE_ASM ("inb %w1,%b0"
	      :"=a" (_v):"d" (port),"0" (0));
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#endif
  return _v;
}

INLINE void
outw(uint16_t value, uint16_t port)
{
  GNU_INLINE_ASM ("outw %w0,%w1"
			: /* no outputs */
			:"a" (value),"d" (port));
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#endif
}

INLINE uint16_t
inw(uint16_t port)
{
  unsigned int _v;

  GNU_INLINE_ASM ("inw %w1,%w0"
			:"=a" (_v):"d" (port),"0" (0));
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#endif
  return _v;
}

INLINE void
outl(uint32_t value, uint16_t port)
{
  GNU_INLINE_ASM ("outl %0,%w1"
			: /* no outputs */
			:"a" (value),"d" (port));
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#endif
}

INLINE uint32_t
inl(uint16_t port)
{
  unsigned int _v;
  GNU_INLINE_ASM ("inl %w1,%0"
			:"=a" (_v): "Nd" (port),"0" (0));
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#ifdef __SLOW_IO
  GNU_INLINE_ASM ("outb %al,$0x80");	 /* delay */
#endif
  return _v;
}

#if 0
#ifdef GNU_INLINE_ASM
INLINE void
ins32_hot(uint16_t port, void *addr, uint32_t count)
{
  GNU_INLINE_ASM ("cld\n");
  while (count >= 32) {
    uint32_t chunk = 32;
    GNU_INLINE_ASM ("cmpb $0,(%%edi)\n\t"	/* soft probe */
			  "rep\n\t"
			  "insl\n\t"
			  : /* "=D" (addr) */
			  : "d" (port), "D" (addr), "c" (chunk)
			  : "1", "2"); /* value is discarded */
    /* addr is updated by the asm! */
    count -= 32;
  }
  
  GNU_INLINE_ASM ("cmpb $0,(%%edi)\n\t"	/* soft probe */
			"rep\n\t"
			"insl\n\t"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}
#endif /* GNU_INLINE_ASM */
#endif

INLINE void
insb(uint16_t port, void *addr, uint32_t count)
{
  GNU_INLINE_ASM ("cld; rep; insb"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

INLINE void
insw(uint16_t port, void *addr, uint32_t count)
{
  GNU_INLINE_ASM ("cld; rep; insw"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

INLINE void
insl(uint16_t port, void *addr, uint32_t count)
{
  GNU_INLINE_ASM ("cld; rep; insl"
			: "=D" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

INLINE void
outsb(uint16_t port, void *addr, uint32_t count)
{
  GNU_INLINE_ASM ("cld; rep; outsb"
			: "=S" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

INLINE void
outsw(uint16_t port, void *addr, uint32_t count)
{
  GNU_INLINE_ASM ("cld; rep; outsw"
			: "=S" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

INLINE void
outsl(uint16_t port, void *addr, uint32_t count)
{
  GNU_INLINE_ASM ("cld; rep; outsl"
			: "=S" (addr), "=c" (count)
			: "d" (port), "0" (addr), "1" (count));
}

#endif  /* __IO_H__ */
