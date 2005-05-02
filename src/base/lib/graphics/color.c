/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

#include "color.h"

static unsigned short
color_weight(unsigned long mask)
{
  unsigned short weight = 0u;

  while(mask) {
    weight++;
    mask >>= 1;
  }
  return weight;
}

unsigned short
makeColor16(color_t color, unsigned long red_mask, unsigned long green_mask,
	    unsigned long blue_mask)
{
  unsigned char red = (unsigned char)((color & 0xFF0000) >> 16);
  unsigned char green = (unsigned char)((color & 0xFF00) >> 8);
  unsigned char blue = (unsigned char)(color & 0xFF);
  unsigned short result = 0u;

  result = ((unsigned short)(red & 0x1F) << color_weight(green_mask)) |
    ((unsigned short)(green & 0x3F) << color_weight(blue_mask)) |
    (unsigned short)(blue & 0x1F);

  return result;
}
