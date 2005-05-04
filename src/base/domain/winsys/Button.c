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

#include <stdlib.h>
#include <string.h>
#include <eros/target.h>
#include <domain/domdbg.h>
#include "Button.h"
#include "winsyskeys.h"
#include "sessionmgr/Session.h"
#include "coordxforms.h"
#include "debug.h"
#include "fbmgr/fbm_commands.h"
#include "fbmgr/drivers/video.h"
#include "global.h"
#include "xclipping.h"

/* Handler for client-related mouse events */
static void
button_deliver_mouse_event(Window *w, uint32_t buttons, point_t location,
			   bool dragging)
{
  Button *b = (Button *)w;

  if (buttons & MOUSE_LEFT) {
    b->pressed = true;
    w->render_focus(w, true);
  }
  else {
    /* User just pressed the kill button, so execute the action
       callback */
    if (b->pressed) {
      rect_t rect = { {0, 0}, w->size };
      rect_t win_rect_root;

      /* First, re-render the button with its not-pushed-in pixmap */
      b->pressed = false;
      w->render_focus(w, true);

      /* Then, do the callback as long as cursor is still within
	 bounds of this button.  (Cursor position is in Root coords so
	 xform appropriately.) */
      xform_rect(w, rect, WIN2ROOT, &win_rect_root);
      if (rect_contains_pt(&win_rect_root, &location))
	b->action_cb(w);
    }
  }
}

/* Handler for client-related key events */
static void
button_deliver_key_event(Window *w, uint32_t keycode)
{
}

/* Handler for visualizing receiving/relinquishing focus */
static void
button_render_focus(Window *w, bool hasFocus)
{
  uint32_t u;
  clip_vector_t *dec_pieces = NULL;

  if(w->type != WINTYPE_BUTTON)
    kdprintf(KR_OSTREAM, "Predicate failure in button_render_focus().\n");

  dec_pieces = window_get_subregions(w, UNOBSTRUCTED);

  for (u = 0; u < dec_pieces->len; u++)
    w->draw(w, dec_pieces->c[u].r);
}

/* Handler for receiving/relinquishing focus */
static void
button_set_focus(Window *thisWindow, bool hasFocus)
{
  Button *b = (Button *)thisWindow;

  /* If we just lost focus, reset pressed state */
  if (!hasFocus) {
    if (b->pressed) {
      b->pressed = false;
      thisWindow->render_focus(thisWindow, false);
    }
  }

  /* Make sure title bar is rendered appropriately */
  thisWindow->parent->render_focus(thisWindow->parent, hasFocus);
}

static void
button_draw(Window *w, rect_t region)
{
  Button *b = (Button *)w;
  rect_t rect = { {0,0}, w->size}; 
  rect_t reconcile;
  rect_t region_c;

  xform_rect(w, region, ROOT2WIN, &region_c);
  /* Determine exactly what part of this button to draw */
  if (!rect_intersect(&region_c, &rect, &reconcile))
    return;

  /* Put 'reconcile' back in root coords */
  xform_rect(w, reconcile, WIN2ROOT, &rect);

  if (b->pressed)
    video_show_pixmap(b->in_pixmap, reconcile.topLeft, rect);
  else
    video_show_pixmap(b->out_pixmap, reconcile.topLeft, rect);
}

/* A button never moves/resizes directly.  It only moves as its parent
   (decoration) gets resized.  Thus, ignore the 'orig_delta' arg for
   now. */
static void
button_move(Window *w, point_t orig_delta, point_t size_delta)
{
  Button *button = (Button *)w;

  button->win.origin.x += size_delta.x;
}

Button *
button_create(Window *parent, point_t origin, point_t size, 
	      void *session, CallbackFn fct)
{
  Button *b = malloc(sizeof(Button));
  
  memset(b, 0, sizeof(Button));
  window_initialize(&(b->win), parent, origin, size, session, WINTYPE_BUTTON);

  b->win.deliver_mouse_event = button_deliver_mouse_event;
  b->win.deliver_key_event = button_deliver_key_event;
  b->win.draw = button_draw;
  b->win.set_focus = button_set_focus;
  b->win.render_focus = button_render_focus;
  b->win.move = button_move;

  b->pressed = false;
  b->action_cb = fct;

  return b;
}

