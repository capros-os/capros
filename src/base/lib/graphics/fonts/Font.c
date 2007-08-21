/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime distribution.
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


#include <eros/target.h>
#include <eros/stdarg.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>
#include <string.h>
#include <stdlib.h>

#include "Font.h"

/* Bitmaps for the fonts */
#include "cons_font_8x16.h"
#include "sun12x22.h"

FontType *all_fonts[] = {
    &DefaultFont,
    &Sun12x22Font,
};

#define num_fonts (sizeof(all_fonts)/sizeof(*all_fonts))
#define MAX_FONT_WIDTH  20
#define MAX_FONT_HEIGHT 25
static color_t glyph[MAX_FONT_WIDTH * MAX_FONT_HEIGHT] = { 0x0u };

static void
do_bitblt(uint8_t *pixel_buffer, uint32_t buffer_width, 
	  point_t start, uint32_t width,
	  uint32_t height, color_t *data, uint16_t flag, color_t bg)
{
  uint32_t x, y;
  uint32_t count = 0;

  for (y = start.y; y < start.y + height; y++) {
    for (x = start.x; x < start.x + width; x++) {

      uint32_t offset = x + buffer_width * y;

      if (x < buffer_width) {
	if (flag == FONT_XOR) {
	  if (((color_t *)pixel_buffer)[offset] == bg)
	    ((color_t *)pixel_buffer)[offset] = data[count];
	  else
	    ((color_t *)pixel_buffer)[offset] = bg;
	}
	else			/* default case */
	  ((color_t *)pixel_buffer)[offset] = data[count];
      }

      count++;
    }
  }
}

static void
get_glyph(FontType *font, uint8_t c, color_t fg, color_t bg, color_t *glyph)
{
  uint32_t xx;
  uint32_t yy;
  uint32_t byte_index;
  uint32_t cc = 0;
  uint32_t bytes_per_row = 0;

  if (font == NULL)
    return;

  if (glyph == NULL)
    return;

  bytes_per_row = (font->width % 8) ? (font->width/8 + 1) : (font->width/8);
  for (yy = 0; yy < font->height; yy++) {

    for (xx = 0; xx < font->width; xx++) {
      uint16_t shift = (7 - (xx % 8));

      byte_index = (xx / 8);

      /* Buffer overflow check */
      if (cc >= font->width * font->height)
	continue;

      if (font->bitmap[(c*font->height + yy)*bytes_per_row + byte_index] & 
	  (1 << shift))
	(glyph)[cc++] = fg;
      else
	(glyph)[cc++] = bg;

    }
  }
}

void 
font_render(FontType *font, uint8_t *pixel_buffer, uint32_t buffer_width,
	    point_t start, color_t fg, color_t bg,
	    uint16_t flag, char * string)
{
  point_t cursor = start;
  char * m = string;
  uint32_t cnt = 0;
  uint32_t len = strlen(string);

  if (font == NULL)
    return;

  while(cnt < len) {

    if (cursor.x > buffer_width)
      return;

    /* Preprocess tab, etc. */
    if (*m == '\t')
      cursor.x += 8 * font->width;
    else if (*m == '\r') {
      cursor.x = start.x;
    }
    else if (*m == '\n') {
      cursor.x = start.x;
      cursor.y += font->height;
    }
    else {
      get_glyph(font, *m, fg, bg, glyph);

      /* Handle underline here */
      if (flag == FONT_UNDERLINE) {
	uint32_t x;

	for (x = 0; x < font->width; x++)
	  glyph[x + font->width*(font->height-1)] = fg;
      }

      do_bitblt(pixel_buffer, buffer_width, cursor, 
		font->width, font->height, glyph, flag, bg);
      cursor.x += font->width;
    }
    m++;
    cnt++;
  }
  return;
}

FontType *
font_find_font(char * name)
{
   uint32_t i;

   for (i = 0; i < num_fonts; i++)
      if (!strcmp(all_fonts[i]->name, name))
	  return all_fonts[i];
   return NULL;
}

uint32_t
font_num_fonts(void)
{
  return (uint32_t)num_fonts;
}
