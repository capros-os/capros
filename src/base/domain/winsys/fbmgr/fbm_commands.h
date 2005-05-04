#ifndef __FBMREQUEST_H__
#define __FBMREQUEST_H__

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

#include <graphics/rect.h>

uint32_t fbm_MapClient(cap_t kr_bank, cap_t kr_newspace,
		       /* out */uint32_t *buffer_id);

uint32_t fbm_UnmapClient(uint32_t buffer_id);

/* Used by the window system directly */
void fbm_doRedraw(uint32_t window_width, uint32_t buffer_id, rect_t bounds, 
		  point_t fb_start);

/* Used by client domains (via a session request) */
uint32_t fbm_RedrawClient(uint32_t window_width, 
			  uint32_t buffer_id, rect_t bounds, point_t fb_start);

#endif
