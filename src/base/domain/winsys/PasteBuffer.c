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

#include <eros/target.h>
#include "PasteBuffer.h"

/* FIX: Handle rollover? */
uint32_t
pastebuffer_do_cut(PasteBufferState *state)
{
   state->highest_seq += 2;
   return state->highest_seq;
}

uint32_t
pastebuffer_do_paste(PasteBufferState *state)
{
  return state->highest_seq;
}

void
pastebuffer_initialize(PasteBufferState *state)
{
  state->cur_seq = 0;
  state->highest_seq = 1;
  state->caps_confined = false;	/* until proven otherwise! */
}

void 
pastebuffer_set_confined(PasteBufferState *state, bool confined)
{
  state->caps_confined = confined;
}
