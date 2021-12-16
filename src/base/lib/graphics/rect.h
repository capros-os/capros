#ifndef __GRAPHICS_RECT_H__
#define __GRAPHICS_RECT_H__

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
#include <graphics/point.h>

typedef struct rect_t rect_t;
struct rect_t {
  point_t topLeft;
  point_t bottomRight;
};

int rect_assemble(point_t topLeft, point_t size, rect_t *out);
int rect_eq(rect_t *r1, rect_t *r2);
int rect_ne(rect_t *r1, rect_t *r2);
int rect_isempty(rect_t *r1);

int rect_intersect(rect_t *r1, rect_t *r2, rect_t *dest);
void rect_union(rect_t *r1, rect_t *r2, rect_t *dest);

/* rect_overlapy is an unorthodox operation. If /r1/ and /r2/ intersect, this
 * operation produces a new rectangle /dest/ which is /r1/ trimmed in
 * the vertical dimension so that the intersection or disjunction in
 * the vertical dimension is exact.
 *
 * rect_overlapx performs the same operation in the horizontal
 * dimension. */
int rect_overlapx(rect_t *r1, rect_t *r2, rect_t *dest);
int rect_overlapy(rect_t *r1, rect_t *r2, rect_t *dest);

int rect_contains_pt(rect_t *r, point_t *pt);

#endif /* __GRAPHICS_RECT_H__ */
