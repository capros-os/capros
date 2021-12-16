#ifndef __INVISIBLE_H__
#define __INVISIBLE_H__

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

#include "Window.h"

/* These flags determine how the invisible window is used.  "IS_COPY"
designates the window as the bonified "cut/copy" select region:  if
user presses left mouse in this window the window system will act as
though CTRL-C were sent. Similar action for "IS_PASTE". */
#define IS_NORMAL 0x0u
#define IS_COPY  0x01u
#define IS_PASTE 0x02u

typedef struct Invisible Invisible;

struct Invisible {
  Window win;

  uint32_t qualifier;
};

Invisible *invisible_create(Window *parent, point_t origin, point_t size, 
		            void *session, uint32_t qualifier);

#endif
