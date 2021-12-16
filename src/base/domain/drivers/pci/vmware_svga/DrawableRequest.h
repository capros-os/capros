#ifndef __DRAWABLEREQUEST_H__
#define __DRAWABLEREQUEST_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
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

#include <graphics/line.h>
#include <graphics/rect.h>
#include <graphics/color.h>

/* The video driver doesn't use a Drawable structure */
typedef void Drawable;

/* Main dispatching code */
bool DrawableRequest(uint32_t consoleKey, Message *msg);

/* Prototypes */
uint32_t drawable_SetClipRegion(Drawable *d, rect_t clipRect);
uint32_t drawable_SetPixel(Drawable *d, point_t pixel, color_t color);
uint32_t drawable_LineDraw(Drawable *d, line_t line, uint32_t width, 
			   color_t color,  uint32_t raster_op);
uint32_t drawable_RectFill(Drawable *d, rect_t rect, color_t color,
			   uint32_t raster_op);
uint32_t drawable_RectFillBorder(Drawable *d, rect_t rect, color_t color,
			       color_t border_color, uint32_t raster_op);
uint32_t drawable_TriDraw(Drawable *d, point_t *pt, bool *brd, 
			       color_t color, uint32_t raster_op);
uint32_t drawable_TriFill(Drawable *d, point_t *pt, color_t color, 
			  uint32_t raster_op);
uint32_t drawable_Redraw(Drawable *d, rect_t rect);
uint32_t drawable_Clear(Drawable *d, color_t color);
uint32_t drawable_BitBlt(Drawable *d, rect_t area, uint32_t *data);

/* Temporary: supports testing large bitblts ... */
uint32_t drawable_BigBitBltTest(Drawable *d);

#endif
