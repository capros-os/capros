#ifndef __IO_H__
#define __IO_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
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
