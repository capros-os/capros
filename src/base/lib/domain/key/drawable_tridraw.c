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
drawable_tridraw(uint32_t drawableKey, uint32_t pt1x, uint32_t pt1y, 
		  uint32_t pt2x, uint32_t pt2y, uint32_t pt3x, 
		  uint32_t pt3y, uint32_t brd1, uint32_t brd2, uint32_t brd3, 
		  uint32_t color, uint32_t raster_op)
{
  Message m;
  uint32_t send_data[11];

  m.snd_invKey = drawableKey;
  m.snd_code = OC_Drawable_TriDraw;
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

  send_data[0] = pt1x;
  send_data[1] = pt1y;
  send_data[2] = pt2x;
  send_data[3] = pt2y;
  send_data[4] = pt3x;
  send_data[5] = pt3y;

  send_data[6] = brd1;
  send_data[7] = brd2;
  send_data[8] = brd3;

  send_data[9] = color;
  send_data[10] = raster_op;

  return CALL(&m);
}
