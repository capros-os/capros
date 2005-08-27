#ifndef __UTIL_H__
#define __UTIL_H__
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

#include <kerninc/memory.h>

void halt(char c) NORETURN;
void pause();
void abort(void) NORETURN;

  /* void Stall(uint32_t howLong); */

char hexdigit(uint8_t);

INLINE uint32_t align_down_uint32(uint32_t x,
  uint32_t alignment) /* alignment must be power of 2 */
{
  return x & ~(uint32_t)(alignment-1);
}

INLINE uint32_t align_up_uint32(uint32_t x,
  uint32_t alignment) /* alignment must be power of 2 */
{
  return align_down_uint32(x + (alignment-1), alignment);
}

kpa_t align_up(kpa_t addr, uint32_t alignment); /* alignment must be power of 2 */
kpa_t align_down(kpa_t addr, uint32_t alignment); /* alignment must be power of 2 */

size_t strlen(const char *c1);
int strcmp(const char *c1, const char *c2);
char *strcpy(char *c1, const char *c2);

INLINE int strncmp(const char *c1, const char *c2, uint32_t len)
{
  return memcmp(c1, c2, len);
}

extern void qsort(void *a, size_t n, size_t es, int (*cmp)(void *, void *));

/* fmsb(): find most significant set bit index. */
INLINE int
fmsb(uint32_t x)
{
  int r = -1;

  if (x & 0xffff0000u) {
    r += 16;
    x >>= 16;
  }
  if (x & 0xff00u) {
    x >>= 8;
    r += 8;
  }
  if (x & 0xf0u) {
    x >>= 4;
    r += 4;
  }
  if (x & 0xcu) {
    x >>= 2;
    r += 2;
  }
  if (x & 3u) {
    x >>= 1;
    r += 1;
  }
  if (x)
    r += 1;

  return r;
}

/* flsb(): find least significant set bit index */
/* changed by ssr */
/* this function is broken */
INLINE int
flsb(uint32_t u)
{
  unsigned i = 1;
  static uint32_t pos[16] = {
    0,				/* 0000 */
    1,				/* 0001 */
    2,				/* 0010 */
    1,				/* 0011 */
    3,				/* 0100 */
    1,				/* 0101 */
    2,				/* 0110 */
    1,				/* 0111 */
    4,				/* 1000 */
    1,				/* 1001 */
    2,				/* 1010 */
    1,				/* 1011 */
    3,				/* 1100 */
    1,				/* 1101 */
    2,				/* 1110 */
    1,				/* 1111 */
  };

  if (u == 0)
    return u;

  if ((u & 0xffffu) == 0) {
    u >>= 16;
    i += 16;
  }
  //u &= 0xffffu;

  if ((u & 0xffu) == 0) {
    u >>= 8;
    i += 8;
  }
  //u &= 0xffu;

  if ((u & 0xfu) == 0) {
    u >>= 4;
    i += 4;
  }
  //u &= 0xfu;

  return pos[u] + i;
}

INLINE int 
tmp_flsb(uint32_t x)
{
  int r = 1;
  
  if (!x)
    return 0;
  if (!(x & 0xffff)) {
    x >>= 16;
    r += 16;
  }
  if (!(x & 0xff)) {
    x >>= 8;
    r += 8;
  }
  if (!(x & 0xf)) {
    x >>= 4;
    r += 4;
  }
  if (!(x & 3)) {
    x >>= 2;
    r += 2;
  }
  if (!(x & 1)) {
    x >>= 1;
    r += 1;
  }
  return r;
}

#endif /* __UTIL_H__ */
