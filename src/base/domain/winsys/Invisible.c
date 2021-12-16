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
#include <string.h>
#include <eros/target.h>
#include <domain/domdbg.h>
#include "Invisible.h"
#include "winsyskeys.h"
#include "sessionmgr/Session.h"
#include "coordxforms.h"
#include "debug.h"
#include "fbmgr/fbm_commands.h"
#include "global.h"
#include "Link.h"

/* Handler for invisible-related mouse events */
void
invisible_deliver_mouse_event(Window *w, uint32_t buttons, point_t location,
			   bool dragging)
{
  kdprintf(KR_OSTREAM, "FATAL: Invisible window received mouse event!\n");
}

/* Handler for invisible-related key events */
void
invisible_deliver_key_event(Window *w, uint32_t keycode)
{
  kdprintf(KR_OSTREAM, "FATAL: Invisible window received keyb event!\n");
}

/* Handler for visualizing receiving/relinquishing focus */
void
invisible_render_focus(Window *w, bool hasFocus)
{
}

/* Handler for receiving/relinquishing focus */
void
invisible_set_focus(Window *thisWindow, bool hasFocus)
{
  kdprintf(KR_OSTREAM, "FATAL: Invisible window received focus!\n");
}

/* FIX: Not sure how to handle this one.  An invisible window should
be just that, but it's non-invisible children need to be rendered.
Based on the design that a window is rendered before its children
(since children are always "on top"), have an invisible window kick off the drawing for its parent, since there's nothing to actually draw for itself.  The drawback to this is that regions occluded by invisible windows are sometimes drawn twice! */
void
invisible_draw(Window *w, rect_t region)
{
  if (w->parent)
    w->parent->draw(w->parent, region);
}

static void
invisible_move(Window *w, point_t orig_delta, point_t size_delta)
{
}

Invisible *
invisible_create(Window *parent, point_t origin, point_t size, void *session,
		 uint32_t qualifier)
{
  Invisible *inv = malloc(sizeof(Invisible));

  memset(inv, 0, sizeof(Invisible));
  window_initialize(&(inv->win), parent, origin, size, session, 
		    WINTYPE_INVISIBLE);

  inv->qualifier = qualifier;
  inv->win.deliver_mouse_event = invisible_deliver_mouse_event;
  inv->win.deliver_key_event = invisible_deliver_key_event;
  inv->win.draw = invisible_draw;
  inv->win.set_focus = invisible_set_focus;
  inv->win.render_focus = invisible_render_focus;
  inv->win.move = invisible_move;

  return inv;
}
