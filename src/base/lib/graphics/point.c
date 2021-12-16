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

#include "point.h"

int 
point_eq(const point_t *p1, const point_t *p2)
{
  return ((p1->x == p2->x) && (p1->y == p2->y));
}

int 
point_ne(const point_t *p1, const point_t *p2)
{
  return !point_eq(p1, p2);
}

/* Less in BOTH dimensions */
int 
point_lt(const point_t *p1, const point_t *p2)
{
  return ((p1->x < p2->x) && (p1->y < p2->y));
}

int 
point_le(const point_t *p1, const point_t *p2)
{
  return ((p1->x <= p2->x) && (p1->y <= p2->y));
}

int 
point_gt(const point_t *p1, const point_t *p2)
{
  return ((p1->x > p2->x) && (p1->y > p2->y));
}

int 
point_ge(const point_t *p1, const point_t *p2)
{
  return ((p1->x >= p2->x) && (p1->y >= p2->y));
}
