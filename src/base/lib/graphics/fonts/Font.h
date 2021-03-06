#ifndef __FONT_STRUCT_H__
#define __FONT_STRUCT_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime distribution,
 * and is derived from the EROS Operating System runtime distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <graphics/color.h>
#include <graphics/point.h>

/* Bit operations for blt'ing font bitmaps */
#define FONT_NORMAL  0
#define FONT_XOR     1
#define FONT_UNDERLINE 2

/* A dispatching structure for different fonts. */
typedef const struct FontType FontType;

/* All fonts */
extern FontType *all_fonts[];

/* Individual fonts */
extern FontType DefaultFont,
                Sun12x22;

struct FontType {
  char * name;
  uint32_t width;
  uint32_t height;
  uint8_t *bitmap;
#if 0
  uint32_t (*DrawString)(uint8_t *buffer, uint32_t buffer_width,
			 point_t start, color_t fg, color_t bg, uint16_t flag,
			 uint8_t *string);
#endif
};

FontType *font_find_font(char * name);

uint32_t font_num_fonts();

void font_render(FontType *font, uint8_t *buffer, uint32_t buffer_width,
		 point_t start, color_t fg, color_t bg, uint16_t flag,
		 char * string);

#endif
