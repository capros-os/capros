#ifndef __WINDOW_IDS_H__
#define __WINDOW_IDS_H__

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

/* This package provides the logic for mapping a Window object to a
   unique, opaque window id that can be given to a window system
   client.  The window system client uses its SessionKey plus a window
   id to request window operations (such as drawing, clipping, etc.).
*/

#include "Session.h"

/* Return a Window pointer given a Session and window id.  Return NULL
   if no such window id exists for that Session. */
Window *winid_to_window(Session *s, uint32_t winid);

uint32_t winid_new(Session *s, Window *w);

void winid_free(uint32_t id);

#endif
