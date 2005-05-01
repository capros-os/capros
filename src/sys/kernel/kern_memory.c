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

#include <kerninc/kernel.h>
#include <kerninc/memory.h>


void
memset(void *vp, int c, size_t len)
{
  size_t i;
  uint8_t * bp = (uint8_t *) vp;
  
  for (i = 0; i < len; i++)
    bp[i] = c;
}

int
memcmp(const void* p1, const void* p2, size_t len)
{
  uint8_t *bp1 = (uint8_t *) p1;
  uint8_t *bp2 = (uint8_t *) p2;

  while (len) {
    if (*bp1 < *bp2)
      return -1;
    if (*bp1 > *bp2)
      return 1;

    len--;
    bp1++;
    bp2++;
  }

  return 0;
}

#ifdef __generic_bzero
/* bzero - declared in lostart.hxx */
void
generic_bzero(void *vp, size_t len)
{
  if (len % 4 == 0) {
    uint32_t *wp = (uint32_t *) vp;
    len >>= 2;
    
#if 0
    /* Length is now in WORDS.  Most modern cache lines are 32 bytes
     * or better.  Following loop is therefore hand-unrolled:
     */
    while (len >= 8) {
      wp[0] = 0;
      wp[1] = 0;
      wp[2] = 0;
      wp[3] = 0;
      wp[4] = 0;
      wp[5] = 0;
      wp[6] = 0;
      wp[7] = 0;
      len -= 8;
      wp += 8;
    }
#endif
    
    while(len--)
      *wp++ = 0;
  }
  else {
    uint8_t *bp = (uint8_t *) vp;
    
    while(len--)
      *bp++ = 0;
  }
}
#endif

#ifdef __generic_bcopy
void
generic_bcopy(const void *from, void *to, size_t len)
{
  if (len % 4 == 0) {
    len >>= 2;
    
    uint32_t *fp = (uint32_t *) from;
    uint32_t *tp = (uint32_t *) to;
    
    while(len--)
	*tp++ = *fp++;
  }
  else {
    uint8_t *fp = (uint8_t *) from;
    uint8_t *tp = (uint8_t *) to;
    
    while(len--)
	*tp++ = *fp++;
  }
}
#endif
