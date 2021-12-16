#ifndef __SESSION_H__
#define __SESSION_H__

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

#include "../Window.h"
#include "../id_tree.h"
#include "EvQueue.h"

typedef struct Session Session;
struct Session {
  /* All windows created via this Session inherit trust from the
     Session */
  bool trusted;

  /* All windows created via a Session key are children of this Window: */
  Window *container;

  /* Need to keep track of all windows within the session.  When
     session "ends" all windows in that session are killed.  */
  TREENODE *windows;

  /* Event queue for this session */
  EvQueue events;

  /* Indicates whether client that holds key to this Session is
     parked, waiting for an event */
  bool waiting;

  /* Support for cut/paste operations */
  uint32_t cut_seq;
  uint32_t paste_seq;
};

Session *session_create(Window *container);

void session_queue_mouse_event(Session *session, uint32_t window, 
			       uint32_t button_mask,
			       point_t cursor);

void session_queue_key_event(Session *session, uint32_t window, 
			     uint32_t scancode);

void session_queue_resize_event(Session *session, uint32_t window,
				point_t new_origin, point_t new_size);
#endif
