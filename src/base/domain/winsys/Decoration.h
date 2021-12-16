#ifndef __DECORATION_H__
#define __DECORATION_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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

#include "Button.h"

typedef struct Decoration Decoration;
typedef struct HotSpot HotSpot;

enum HotSpotEnum {
  TITLEBAR,
  TOP_LEFT,
  TOP,
  TOP_RIGHT,
  RIGHT,
  BOTTOM_RIGHT,
  BOTTOM,
  BOTTOM_LEFT,
  LEFT,
  TOTAL_HOT_SPOTS,
};

struct HotSpot {
  rect_t bounds;
  void (*execute)(Decoration *d, uint32_t buttons, point_t location,
		  bool dragging);
};

struct Decoration {
  Window win;

  Button *killButton;

  /* List of hot spots on the decoration window */
  HotSpot hotspots[TOTAL_HOT_SPOTS];

  int32_t active_hotspot;

  /* Hotspots are generally more than one pixel in width/height. To
     make hotspot actions as visually pleasing as possible, we compute
     the offset (however small) between the actual cursor location to
     the appropriate part of the hotspot when first activated. */
  point_t offset;

  /* Use a boolean to determine if offset has been computed */
  bool offset_computed;

  /* Support for drawing ghost outlines for moving and resizing */
  bool undraw_old_ghost;
  point_t ghost_orig_delta;
  point_t ghost_size_delta;
};

Decoration *decoration_create(Window *parent, point_t origin, point_t size, 
			      void *session);

#endif
