#ifndef __GRAPHICS_POINT_H__
#define __GRAPHICS_POINT_H__

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

#include <graphics/graphics.h>

typedef struct point_t point_t;
struct point_t{
  coord_t x;
  coord_t y;
};

int point_eq(const point_t *p1, const point_t *p2);
int point_ne(const point_t *p1, const point_t *p2);
int point_lt(const point_t *p1, const point_t *p2);
int point_le(const point_t *p1, const point_t *p2);
int point_gt(const point_t *p1, const point_t *p2);
int point_ge(const point_t *p1, const point_t *p2);

#endif /* __GRAPHICS_POINT_H__ */
