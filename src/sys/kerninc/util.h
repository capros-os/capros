#ifndef __UTIL_H__
#define __UTIL_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

void halt(char c) NORETURN;
void pause();
void abort(void) NORETURN;

  /* void Stall(uint32_t howLong); */

char hexdigit(uint8_t);
int charToHex(char c);
uint64_t strToUint64(const char * * pp);

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

INLINE unsigned int
boolToBit(bool b)
{
  return b != 0;
}

size_t strlen(const char *c1);
int strcmp(const char *c1, const char *c2);
char *strcpy(char *c1, const char *c2);
void * memset(void * ptr, int val, size_t len);

INLINE void
kzero(void * ptr, size_t len)
{
  /* libc has bzero, but its implementation is much worse than memset. */
  memset(ptr, 0, len);
}

extern void qsort(void *a, size_t n, size_t es, int (*cmp)(void *, void *));

#endif /* __UTIL_H__ */
