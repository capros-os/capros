#ifndef __WINDOW_H__
#define __WINDOW_H__

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

#include <graphics/line.h>
#include <graphics/rect.h>
#include <graphics/color.h>
#include "Link.h"

typedef struct Window Window;

/* Max length for window title */
#define MAX_TITLE 100

/* Can't request windows smaller than this and can't resize existing
   windows smaller than this: */
#define MIN_WINDOW_WIDTH 50
#define MIN_WINDOW_HEIGHT 50

/* Need to distinguish decoration windows from client windows.  The
window system does not hand out capabilities to the decorations, since
it has sole responsibility for them.  However, the decoration windows
still need to be considered for clipping, picking, etc. */
#define WINTYPE_ROOT               0
#define WINTYPE_CLIENT             1
#define WINTYPE_DECORATION         2
#define WINTYPE_INVISIBLE          3
#define WINTYPE_BUTTON             4

struct Window {
  /* Doubly linked list of siblings whose "head" is our
     parent. Siblings in the /prev/ direction are in "front" of us,
     though they may not actually overlap us. Siblings in the /next/
     direction are "behind" us */
  Link sibchain;

  /* List of this window's child windows. */
  Link children;

  /* Ptr back to parent is very convenient: */
  Window *parent;

  /* Serves as a hint to the current "look" object as to how to render
     this window.  */
  bool hasFocus;

  /* The 'type' field distinguishes decoration windows from client
     windows.  This comes into play for "picking". */
  uint32_t type;

  rect_t     userClipRegion;	/* alleged clip region */
  rect_t     drawClipRegion;	/* active clip region */

  point_t origin;		/* in PARENT coordinates */
  point_t size;			/* in pixels */

  /* window attributes */
  void *session;   		/* pointer to this window's Session */
  bool mapped;			/* indicates whether client wants
				   window displayed */
  uint8_t name[MAX_TITLE];	/* string to display in title bar
				   (only displayed if title bar is
				   specified as part of decorations
				   below) */
  /* Uniquely identifies this Window within its Session */
  uint32_t window_id;	

  /* Uniquely identifies the buffer containing the contents of this
     Window */
  uint32_t buffer_id;

  /* window-specific event handlers */
  void (*deliver_mouse_event)(Window *w, uint32_t buttons, point_t location,
			      bool dragging);
  void (*deliver_key_event)(Window *w, uint32_t keycode);

  /* window-specific drawing handler */
  void (*draw)(Window *w, rect_t region);

  /* window-specific focus logic */
  void (*set_focus)(Window *w, bool hasFocus);

  /* window-specific logic for visualizing focus changes. (Note that a
     window such as a decoration may need to be rendered as if it had
     focus when, in fact, its child has the focus.) */
  void (*render_focus)(Window *w, bool hasFocus);

  /* move/resize logic:  we really only need one method for this with
     two parameters: one for moving origin and one for changing size */
  void (*move)(Window *w, point_t orig_delta, point_t size_delta);
};

/* Some useful macros */
#define WINDOWS_EQUAL(w1,w2) ((ADDRESS(w1) == ADDRESS(w2)))
#define WINIDS_EQUAL(win1, win2)  (win1->window_id == win2->window_id)
#define IS_TRUSTED(win) (((Session *)(win->session))->trusted)
#define IS_ROOT(win)  (win->type == WINTYPE_ROOT)
#define IS_CLIENT(win)  (win->type == WINTYPE_CLIENT)

/* Use window_create() to allocate memory for the window.  It will call
   window_initialize(). */
Window *window_create(Window *parent, point_t origin, point_t size, 
		      void *session, uint32_t type);

/* Use this one if you already have the memory allocated and just need
   to initialize the window innards (eg. Button creation) */
void window_initialize(Window *w, Window *parent, point_t origin,
		       point_t size, void *session, uint32_t type);

/* Destroying a window needs to return an error code. The 'unmap' arg
   is an attempt to prevent redundant hiding of windows:  It's
   sufficient to just hide the parent in order to hide all its
   children.  (Each child doesn't need to be hidden separately, as
   that involves calling the clipping algorithm.) */
uint32_t window_destroy(Window *w, bool unmap);

void window_show(Window *w);
void window_hide(Window *w);
void window_draw(Window *w, rect_t region);

/* Reordering windows with respect to their siblings */
void window_bring_to_front(Window *thisWindow);
void window_move_to_back(Window *thisWindow);

/* Miscellaneous */
void window_set_name(Window *w, uint8_t *name);
bool window_ancestors_mapped(Window *w);
bool window_clip_to_ancestors(Window *w, /* out */ rect_t *r);

/* Debugging */
void window_dump(Window *w);

/* Methods that are specific to certain window types: */

/* CLIENT WINDOWS */
Window *client_create(Window *parent, point_t origin, point_t size, 
		      void *session);

/* THE ROOT WINDOW */
Window *root_create(uint32_t width, uint32_t height);

#endif
