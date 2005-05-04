#ifndef __XCLIPPING_H__
#define __XCLIPPING_H__

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

#include <graphics/clip.h>
#include "Window.h"

/* flags for computing window subregions */

/* Find those parts of the window that are obstructed by siblings and
   decorations */
#define OBSTRUCTED           1

/* Find those parts of the window that are currently visible on the
   display */
#define UNOBSTRUCTED         2

/* Find those parts of the window that are obstructing other windows */
#define OBSTRUCTING_OTHERS   3

/* Special case to prevent drawing over a client window's children */
#define INCLUDE_CHILDREN     0x08u

/* Generate a vector of subregions within the specified Window
according to how the Window is overlapped or overlaps other windows.
The 'flag' parameter specifies whether the caller is interested in
currently OBSTRUCTED subregions, currently UNOBSTRUCTED subregions, or
subregions that are OBSTRUCTING_OTHERS */
clip_vector_t *window_get_subregions(Window *w, uint32_t flag);

/* Use this version if you need to start with a subset of the entire
   window */
clip_vector_t *window_compute_clipping(Window *w, clip_vector_t **unobstructed,
				       uint32_t flag);

/* Used for moving and resizing windows */
clip_vector_t *window_newly_exposed(Window *w, rect_t new_boundary);

void
vector_dump(clip_vector_t *list);

#endif
