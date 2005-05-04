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
#include <domain/domdbg.h>
#include "Window.h"
#include "winsyskeys.h"
#include "sessionmgr/Session.h"
#include "coordxforms.h"
#include "debug.h"
#include "fbmgr/fbm_commands.h"
#include "global.h"

extern Window *focus;

/* Handler for client-related mouse events */
void
client_deliver_mouse_event(Window *w, uint32_t buttons, point_t location,
			   bool dragging)
{
  if (w == NULL)
    return;

  session_queue_mouse_event((Session *)(w->session), w->window_id,
			    buttons, location);
}

/* Handler for client-related key events */
void
client_deliver_key_event(Window *w, uint32_t keycode)
{
  if (w == NULL)
    return;

  session_queue_key_event((Session *)(w->session), w->window_id, keycode);
}

/* Handler for visualizing receiving/relinquishing focus */
void
client_render_focus(Window *w, bool hasFocus)
{
}

/* Handler for receiving/relinquishing focus */
void
client_set_focus(Window *thisWindow, bool hasFocus)
{
  thisWindow->hasFocus = hasFocus;

  /* If this window now has focus, then we need to put it in front of
     all its siblings.  Furthermore, bring all parents up the ancestry
     chain to the front with respect to their siblings. */
  if (hasFocus) {
    Window *parent = thisWindow->parent;

    //    window_bring_to_front(thisWindow);
    while(!IS_ROOT(parent)) {
      window_bring_to_front(parent);
      parent = parent->parent;
    }
  }

  if (thisWindow->parent->type == WINTYPE_DECORATION)
    thisWindow->parent->render_focus(thisWindow->parent, hasFocus);
}

void
client_draw(Window *w, rect_t region)
{
  rect_t out;
  rect_t win_rect = { {0,0}, w->size };
  rect_t win_rect_root;
  rect_t region_clipped;

  /* Get 'w' rectangle in root coords */
  xform_rect(w, win_rect, WIN2ROOT, &win_rect_root);

  /* Get 'region' rectangle in window coords */
  xform_rect(w, region, ROOT2WIN, &out);

  /* Clip both 'region' (which is in Root coords) and 'out' to current 'w'
  bounds.  This is necessary so the 'doRedraw' call below doesn't
  render outside 'w'... */
  if (!rect_intersect(&out, &win_rect, &out))
    return;

  if (!rect_intersect(&region, &win_rect_root, &region_clipped))
    return;

  DEBUG(map) 
  {
    kprintf(KR_OSTREAM, "client_draw(): region to draw: [(%d,%d)(%d,%d)]\n",
	    region_clipped.topLeft.x, region_clipped.topLeft.y, 
	    region_clipped.bottomRight.x, region_clipped.bottomRight.y);

    kprintf(KR_OSTREAM, "client_draw(): conv to WIN: [(%d,%d)(%d,%d)]\n",
	    out.topLeft.x, out.topLeft.y, out.bottomRight.x,
	    out.bottomRight.y);
  }

  w->userClipRegion = out;
  fbm_doRedraw(w->size.x, w->buffer_id, out, region_clipped.topLeft);
}

/* Moving a client is only interesting if its size changes.  Thus,
   ignore cases where size_delta is zero. */
static void
client_move(Window *w, point_t orig_delta, point_t size_delta)
{
  point_t new_size = {w->size.x + size_delta.x,
		      w->size.y + size_delta.y};

  if (size_delta.x == 0 && size_delta.y == 0)
    return;

  w->size = new_size;

  /* queue resize event for client's session */
  session_queue_resize_event((Session *)(w->session), 
			     w->window_id, w->origin, new_size);
}

Window *
client_create(Window *parent, point_t origin, point_t size, void *session)
{
  Window *thisWindow = window_create(parent, origin, size, session, 
				     WINTYPE_CLIENT);
  DEBUG(session_cmds) kprintf(KR_OSTREAM, "client_create() "
			    "allocated client Window at 0x%08x w/sess 0x%08x", 
			      ADDRESS(thisWindow),
			      ADDRESS(thisWindow->session));

  thisWindow->deliver_mouse_event = client_deliver_mouse_event;
  thisWindow->deliver_key_event = client_deliver_key_event;
  thisWindow->draw = client_draw;
  thisWindow->set_focus = client_set_focus;
  thisWindow->render_focus = client_render_focus;
  thisWindow->move = client_move;

  return thisWindow;
}
