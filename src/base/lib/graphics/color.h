#ifndef __COLOR_H__
#define __COLOR_H__

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

/* Color should be specified as a 32-bit value.  For drawables with
   less than 32-bit depth, color masks will be used. */
typedef unsigned long color_t;

/* Some sample colors (in aRGB format) */
#define BLACK  0x0
#define RED    0xFF0000
#define GREEN  0x00FF00
#define BLUE   0x0000FF
#define WHITE  0xFFFFFF
#define YELLOW 0xFFFF00
#define MAGENTA (RED | BLUE)
#define CYAN    (GREEN | BLUE)
#define GRAY50 0x323232
#define GRAY75 0x4B4B4B
#define GRAY150 0x969696
#define GRAY190 0xBEBEBE

/* Routine for converting 32-bit color to 16-bit */
unsigned short makeColor16(color_t color, unsigned long red_mask, 
			   unsigned long green_mask,
			   unsigned long blue_mask);

#endif
