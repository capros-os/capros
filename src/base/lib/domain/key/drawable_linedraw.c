/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
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
#include <eros/Invoke.h>
#include <domain/DrawableKey.h>

uint32_t
drawable_linedraw(uint32_t drawableKey, uint32_t x1, uint32_t y1,
		  uint32_t x2, uint32_t y2, uint32_t width, 
		  uint32_t color, uint32_t raster_op)
{
  Message m;
  uint32_t send_data[7];

  m.snd_invKey = drawableKey;
  m.snd_code = OC_Drawable_LineDraw;
  m.snd_key0 = KR_VOID;
  m.snd_key1 = KR_VOID;
  m.snd_key2 = KR_VOID;
  m.snd_rsmkey = KR_VOID;
  m.snd_data = send_data;
  m.snd_len = sizeof(send_data);
  m.snd_w1 = 0;
  m.snd_w2 = 0;
  m.snd_w3 = 0;

  m.rcv_rsmkey = KR_VOID;
  m.rcv_key0 = KR_VOID;
  m.rcv_key1 = KR_VOID;
  m.rcv_key2 = KR_VOID;
  m.rcv_data = 0;
  m.rcv_limit = 0;
  m.rcv_code = 0;
  m.rcv_w1 = 0;
  m.rcv_w2 = 0;
  m.rcv_w3 = 0;

  send_data[0] = x1;
  send_data[1] = y1;
  send_data[2] = x2;
  send_data[3] = y2;
  send_data[4] = width;
  send_data[5] = color;
  send_data[6] = raster_op;

  return CALL(&m);
}
