#ifndef __EROSGL_H__
#define __EROSGL_H__

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

/* Simple graphics library */
#include <graphics/line.h>
#include <graphics/rect.h>
#include <graphics/color.h>

/* Raster ops */
enum RasterOps {
  ROP_COPY,
};

typedef void (*UpdateFn)(rect_t area);

typedef struct GLContext GLContext;
struct GLContext {
  color_t color;
  uint32_t line_width;
  point_t dimensions;
  rect_t clip_region;
  uint32_t raster_op;
  uint32_t *base;
  rect_t cumulative_damage_area;
  UpdateFn update;
};

GLContext *erosgl_new_context(uint32_t *base, UpdateFn update);

void erosgl_free_context(GLContext *gc);

void erosgl_gc_set_color(GLContext *gc, color_t color);

void erosgl_gc_set_line_width(GLContext *gc, uint32_t width);

void erosgl_gc_set_dimensions(GLContext *gc, int32_t x, int32_t y);

void erosgl_gc_set_clipping(GLContext *gc, rect_t clip);

void erosgl_gc_set_raster_op(GLContext *gc, uint32_t rop);

void erosgl_clear(GLContext *gc, color_t color);

void erosgl_line(GLContext *gc, line_t line);

void erosgl_rectfill(GLContext *gc, rect_t rect);

void erosgl_rectfillborder(GLContext *gc, rect_t rect, color_t border);

void erosgl_trifill(GLContext *gc, point_t pt1, point_t pt2, point_t pt3);

void erosgl_update(GLContext *gc);

#endif /* __GRAPHICS_GRAPHICS_H__ */
