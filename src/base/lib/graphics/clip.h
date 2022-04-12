#ifndef __GRAPHICS_CLIP_H__
#define __GRAPHICS_CLIP_H__

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

#include <stdbool.h>
#include "rect.h"

typedef struct clip_region_t {
  rect_t  r;
  bool   in;
} clip_region_t;


typedef struct clip_vector_t {
  int  len;
  int  allocated;  
  struct clip_region_t c[0];
} clip_vector_t;


/* Allocates a clip_vector_t with "len" clip_region_t's
 */
clip_vector_t *make_clip_vector(int len);

/* on failure to allocate space for expansion 
   returns orig. */
clip_vector_t *vector_expand(clip_vector_t **orig, int len);

/* 
 * reallocates vec_a and creates a new clip_region_t with r and in. 
 * the new region is appended to the end of the vector
 */
clip_vector_t *vector_append_rect(clip_vector_t **vec_a, const rect_t *r, bool in);

/*
 * reallocates a and appends b to the end
 */
clip_vector_t *vector_concat(clip_vector_t **a, clip_vector_t *b);

/*
 * returns vector of only a specific "in" value.
 */
clip_vector_t *vector_remove_rects( clip_vector_t **a, bool in );

clip_vector_t *vector_remove_rect_at_index(clip_vector_t **vec, int index);

/**
 * preconditions: r is the rectangle that is to be clipped
 *                c is the vector of rectangles that have
 *		    been developed so far
 * 
 * postconditions: the return value is a new version of the c
 *                 with the addition of the clippings from r.
 *                 clip_vector_t->c[].in is the boolean of
 *                 the intersection of some portion of a rectangle
 *                 in c with a portion of r.
 *
 *  When passing in an zero length vector the returned vector
 *  is the original vector as an out.  This is correct.
 */                 

clip_vector_t *clip(rect_t *r, clip_vector_t *c);

#endif /* __GRAPHICS_CLIP_H__ */
