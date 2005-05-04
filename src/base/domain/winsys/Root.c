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

#include <stdlib.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/domdbg.h>
#include "Window.h"
#include "winsyskeys.h"
#include "pixmaps/ids.h"
#include "pixmaps/eros_background.h"
#include "fbmgr/drivers/video.h"

static bool bg_pixmap_defined = false;

/* Handler for root-related mouse events */
static void
root_deliver_mouse_event(Window *w, uint32_t buttons, point_t location,
			 bool dragging)
{
}

/* Handler for root-related key events */
static void
root_deliver_key_event(Window *w, uint32_t keycode)
{
}

/* Handler for visualizing receiving/relinquishing focus */
static void
root_render_focus(Window *w, bool hasFocus)
{
}

static void
root_move(Window *w, point_t orig_delta, point_t size_delta)
{
  kdprintf(KR_OSTREAM, "FATAL: Someone tried to move Root!\n");
}

/* Handler for receiving/relinquishing focus */
static void
root_set_focus(Window *w, bool hasFocus)
{
  w->hasFocus = hasFocus;
}

static void
root_draw(Window *w, rect_t region)
{
  rect_t rect = { {0,0}, w->size};
  rect_t reconcile;

  w->userClipRegion = region;

  /* Load the pixmap, if needed */
  if (!bg_pixmap_defined) {
    point_t size = { LOGO_WIDTH, LOGO_HEIGHT };

    if (video_define_pixmap(DefaultRootBackgroundID, size, 
			    LOGO_DEPTH, LOGO_DATA) == RC_OK)
    {
      bg_pixmap_defined = true;
    }
  }

  /* The requested subregion is the clip region;  Determine and render
     the intersection of the clip region and the entire Root */
  if (rect_intersect(&rect, &(w->userClipRegion), &reconcile)) {
    point_t src = { reconcile.topLeft.x % LOGO_WIDTH,
		    reconcile.topLeft.y % LOGO_HEIGHT };

    video_show_pixmap(DefaultRootBackgroundID, src, reconcile);
  }
}

Window *
root_create(uint32_t width, uint32_t height)
{
  Window *w = (Window *)malloc(sizeof(struct Window));
  point_t dims = {width, height};
  rect_t clipRect = { {0,0}, dims};

  link_Init(&(w->sibchain), 0);
  link_Init(&(w->children), 0);
  w->parent = NULL;
  w->type = WINTYPE_ROOT;
  w->userClipRegion = clipRect;
  w->origin.x = 0;
  w->origin.y = 0;
  w->size = dims;
  w->session = NULL;
  w->mapped = true;
  w->deliver_mouse_event = root_deliver_mouse_event;
  w->deliver_key_event = root_deliver_key_event;
  w->draw = root_draw;
  w->render_focus = root_render_focus;
  w->set_focus = root_set_focus;
  w->move = root_move;

  return w;
}

