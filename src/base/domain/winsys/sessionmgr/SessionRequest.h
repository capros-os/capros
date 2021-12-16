#ifndef __SESSION_REQUEST_H__
#define __SESSION_REQUEST_H__

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
#include "Session.h"

/* Main dispatching code */
bool SessionRequest(Message *msg);

/* Prototypes */
uint32_t session_NewWindow(Session *s, uint32_t client_bank_slot,
			   Window *parent,
			   point_t orig, point_t size,
			   uint32_t decorations,
			   /* out */ uint32_t *winid,
			   uint32_t new_window_slot);

uint32_t session_NewInvisibleWindow(Session *s, Window *parent, point_t origin,
				    point_t size, uint32_t qualifier,
				    /* out */ uint32_t *winid);

uint32_t session_NextEvent(Session *s,
			   /* out */ Event *event);

uint32_t session_Close(Session *s);

uint32_t session_WinMap(Window *w);

uint32_t session_WinUnmap(Window *w);

uint32_t session_WinKill(Window *w);

uint32_t session_WinGetSize(Window *w, 
			    /* out */ uint32_t *width, 
			    /* out */ uint32_t *height);

uint32_t session_WinSetClipRegion(Window *w, rect_t clipRect);

uint32_t session_WinSetTitle(Window *w, uint8_t *title);

uint32_t session_WinRedraw(Window *w, rect_t bounds);

uint32_t session_WinResize(Window *w, point_t delta);

uint32_t session_NewSessionCreator(Session *s, Window *container,
				   cap_t kr_new_creator);

#endif
