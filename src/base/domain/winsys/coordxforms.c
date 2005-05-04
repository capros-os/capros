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

/* Various functions for converting from Window coords to Root coords
   and vice-versa. */

#include "coordxforms.h"
#include "winsyskeys.h"

#include <domain/domdbg.h>

#define PARANOIA_CHECK(x,y) { \
                  if (x == NULL) \
                    return false;  \
                                   \
                  if (y == NULL)   \
                    return false; }

#define N 3
typedef int32_t scalar;
typedef scalar vector[N];
typedef scalar matrix[N][N];

void
matrix_dump(matrix M)
{
  uint32_t u, v;

  if (M == NULL) {
    kprintf(KR_OSTREAM, "  <null pointer>");
    return;
  }

  for (u = 0; u < N; u++)
    for (v = 0; v < N; v++)
      kprintf(KR_OSTREAM, "M[%u][%u] = %d", u, v, M[u][v]);
}

#if 0
static void
vector_dump(vector V)
{
  uint32_t u;

  if (V == NULL) {
    kprintf(KR_OSTREAM, "  <null pointer>");
    return;
  }

  for (u = 0; u < N; u++)
    kprintf(KR_OSTREAM, "V[%u] = %d", V[u]);
}
#endif

static void
apply_transform(vector v, matrix T, vector result)
{
  uint32_t i, j;

  for (i = 0; i < N; i++) {
    result[i] = 0;
    for (j = 0; j < N; j++) {
      result[i] += v[j]*T[j][i];
    }
  }
}

static void
make_homogeneous(point_t in, /* out */ vector out)
{
  out[0] = in.x;
  out[1] = in.y;
  out[2] = 1;
}

static void
make_identity(/* in/out */ matrix ident)
{
  uint32_t u, v;

  for (u = 0; u < N; u++)
    for (v = 0; v < N; v++)
      if (u == v)
	ident[u][v] = 1;
      else
	ident[u][v] = 0.0;
}

static void
make_translation(point_t translation, /* out */ matrix T)
{
  make_identity(T);

  T[2][0] = translation.x;
  T[2][1] = translation.y;
}

bool
xform_point(Window *w, point_t in, uint32_t how, /* out */ point_t *out)
{
  Window *p = NULL;
  point_t tmp;

  PARANOIA_CHECK(w, out);

  p = w->parent;
  tmp = w->origin;

  /* first compute translation from Window to Root */
  while (p != NULL) {
    tmp.x += p->origin.x;
    tmp.y += p->origin.y;
    p = p->parent;
  }

  if (how == ROOT2WIN) {
    /* Need to negate tmp */
    tmp.x = -tmp.x;
    tmp.y = -tmp.y;
  }
  else if (how != WIN2ROOT)
    return false;


  {
    matrix T;
    vector V      = {0, 0, 0};
    vector result = {0, 0, 0};

    /* Make a translation matrix and compute translation */
    make_translation(tmp, T);

    make_homogeneous(in, V);
    apply_transform(V, T, result);

    out->x = result[0];
    out->y = result[1];
  }

  return true;
}

bool
xform_rect(Window *w, rect_t in, uint32_t how, /* out */ rect_t *out)
{
  point_t tl, br;

  PARANOIA_CHECK(w, out);

  if (!xform_point(w, in.topLeft, how, &tl))
    return false;

  if (!xform_point(w, in.bottomRight, how, &br))
    return false;

  out->topLeft = tl;
  out->bottomRight = br;

  return true;
}

bool
xform_win2rect(Window *w, uint32_t how, /* out */ rect_t *out)
{
  rect_t r = { {0, 0}, {0, 0} };

  PARANOIA_CHECK(w, out);

  r.bottomRight = w->size;

  return xform_rect(w, r, how, out);
}

bool
xform_line(Window *w, line_t in, uint32_t how, /* out */ line_t *out)
{
  point_t pt0, pt1;

  PARANOIA_CHECK(w, out);

  if (!xform_point(w, in.pt[0], how, &pt0))
    return false;

  if (!xform_point(w, in.pt[1], how, &pt1))
    return false;

  out->pt[0] = pt0;
  out->pt[1] = pt1;

  return true;
}
