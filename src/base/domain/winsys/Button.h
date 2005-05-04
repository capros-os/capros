#ifndef __BUTTON_H__
#define __BUTTON_H__

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

typedef struct Button Button;
typedef void (*CallbackFn)(Window *);

struct Button {
  Window win;

  /* button-specific fields */
  uint32_t in_pixmap;		/* pixmap id for button pushed in */
  uint32_t out_pixmap;		/* pixmap id for button out */

  bool pressed;			/* whether button is pressed in */

  CallbackFn action_cb;
};

Button *button_create(Window *parent, point_t origin, point_t size, 
		      void *session, CallbackFn fct);

#endif
