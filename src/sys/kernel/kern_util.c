/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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

#include <kerninc/kernel.h>
#include <kerninc/util.h>

int
__assert(const char *expression, const char *file, int line)
{
  fatal("%s:%d: failed assertion `%s'\n", file, line, expression);
  return 0;
}

int
__assertex(const void *ptr, const char *expression, const char *file, int line)
{
  printf("Ptr 0x%08x\n", ptr);
  fatal("%s:%d: failed assertion `%s'\n", file, line, expression);
  return 0;
}

void
__require(const char *expression, const char *file, int line)
{
  dprintf(true, "%s:%d: failed rqt `%s'\n", file, line, expression);
}

static char hexdigits[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

char
hexdigit(uint8_t b)
{
  return hexdigits[b];
}

int
charToHex(char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'a') {
    return c - 'a' + 10;
  } else return -1;
}

/* *pp is a pointer to a string containing "0xnnnnnnnnnnnnnnnn".
   This procedure returns the hex value, and sets *pp to point
   to the next character. */
uint64_t
strToUint64(const char * * pp)
{
  int i;
  const char * p = *pp;
  uint64_t val64 = 0;

  assert(*p == '0');
  p++;
  assert(*p == 'x');
  p++;
  for (i = 0; i < 16; i++, p++) {
    assert(*p != 0);    /* ensure against a short string */
    val64 = (val64 << 4) + charToHex(*p);
  }

  *pp = p;
  return val64;
}

int
strcmp(const char *p1, const char *p2)
{
  while (*p1 && *p2) {
    if (*p1 < *p2)
      return -1;
    if (*p1 > *p2)
      return 1;

    p2++;
    p1++;
  }

  if (*p1 == *p2)
    return 0;
  if (*p1 == 0)
    return -1;
  return 1;
}

char *
strcpy(char *to, const char *from)
{
  char c;
  char *dest = to;
  
  do {
    c = *from++;
    *to++ = c;
  } while(c);

  return dest;
}

size_t
strlen(const char *s)
{
  const char *end = s;

  while (*end)
    end++;

  return end - s;
}

kpa_t
align_up(kpa_t addr, uint32_t alignment)
{
    addr += alignment - 1;
    return addr & ~(kpa_t)(alignment - 1);
}

kpa_t
align_down(kpa_t addr, uint32_t alignment)
{
    return addr & ~(kpa_t)(alignment - 1);
}

