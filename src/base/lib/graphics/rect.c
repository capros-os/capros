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

#include <eros/target.h>

#include <stdlib.h>
#include <graphics/rect.h>

#include <domain/domdbg.h>
extern uint32_t ostream_key;

#define max(a,b) ((a) >= (b) ? (a) : (b))
#define min(a,b) ((a) <= (b) ? (a) : (b))

int
rect_assemble(point_t topLeft, point_t size, rect_t *out)
{
  if (out == NULL)
    return 0;

  out->topLeft = topLeft;
  out->bottomRight.x = out->topLeft.x + size.x;
  out->bottomRight.y = out->topLeft.y + size.y;
  return !rect_isempty(out);
}

int
rect_eq(rect_t *r1, rect_t *r2)
{
  return (point_eq(&r1->topLeft, &r2->topLeft) &&
	  point_eq(&r1->bottomRight, &r2->bottomRight));
}

int 
rect_ne(rect_t *r1, rect_t *r2)
{
  return !rect_eq(r1, r2);
}

/* The point functions don't quite cover all possibilities here, so we
   need the extra checks: */
int
rect_isempty(rect_t *r1)
{
  if (point_le(&r1->bottomRight, &r1->topLeft))
    return true;

  if (r1->bottomRight.y <= r1->topLeft.y)
    return true;

  if (r1->bottomRight.x <= r1->topLeft.x)
    return true;

  return false;
}

int
rect_intersect(rect_t *r1, rect_t *r2, rect_t *dest)
{
  rect_t out;

  out.topLeft.x = max(r1->topLeft.x, r2->topLeft.x);
  out.topLeft.y = max(r1->topLeft.y, r2->topLeft.y);
  out.bottomRight.x = min(r1->bottomRight.x, r2->bottomRight.x);
  out.bottomRight.y = min(r1->bottomRight.y, r2->bottomRight.y);

  if (dest) *dest = out;
  return !rect_isempty(&out);
}

void
rect_union(rect_t *r1, rect_t *r2, rect_t *dest)
{
  dest->topLeft.x = min(r1->topLeft.x, r2->topLeft.x);
  dest->topLeft.y = min(r1->topLeft.y, r2->topLeft.y);
  dest->bottomRight.x = max(r1->bottomRight.x, r2->bottomRight.x);
  dest->bottomRight.y = max(r1->bottomRight.y, r2->bottomRight.y);
}

int
rect_overlapx(rect_t *r1, rect_t *r2, rect_t *dest)
{
  rect_t out;

  /* Until proven otherwise: */
  *dest = *r1;

  if (rect_intersect(r1, r2, &out)) {
    if (out.topLeft.x > r1->topLeft.x)
      dest->bottomRight.x = r2->topLeft.x;
    else
      dest->bottomRight.x = r2->bottomRight.x;
    return 1;
  }

  return 0;
}

int
rect_overlapy(rect_t *r1, rect_t *r2, rect_t *dest)
{
  rect_t out;

  /* Until proven otherwise: */
  *dest = *r1;

  if (rect_intersect(r1, r2, &out)) {
    if (out.topLeft.y > r2->topLeft.y)
      dest->bottomRight.y = r2->topLeft.y;
    else
      dest->bottomRight.y = r2->bottomRight.y;

    return 1;
  }

  return 0;
}

/* NOTE: rectangles *include* the left edge, but *not* the right edge */
int
rect_contains_pt(rect_t *r, point_t *p)
{
  if (rect_isempty(r))
    return 0;

  return (p->x >= r->topLeft.x) && (p->x < r->bottomRight.x) &&
    (p->y >= r->topLeft.y) && (p->y < r->bottomRight.y);
}
