#ifndef __COORDXFORMS_H__
#define __COORDXFORMS_H__

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

#include <stddef.h>
#include <eros/target.h>
#include "Window.h"

#define WIN2ROOT  1
#define ROOT2WIN  2

/* Convert a point to either Window or Root coords */
bool xform_point(Window *w, point_t in, uint32_t how,
		 /* out */ point_t *out);

/* Convert a rect to either Window or Root coords */
bool xform_rect(Window *w, rect_t in, uint32_t how,
		/* out */ rect_t *out);

/* Generate a rect representing Window */
bool xform_win2rect(Window *w, uint32_t how,
		    /* out */ rect_t *out);

/* Convert a line to either Window or Root coords */
bool xform_line(Window *w, line_t in, uint32_t how,
		/* out */ line_t *out);
#endif
