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
#include <domain/drivers/VideoDriverKey.h>

uint32_t
video_set_resolution(uint32_t video_key,
		     uint32_t width,
		     uint32_t height,
		     uint32_t depth)
{
  Message m;

  m.snd_invKey = video_key;
  m.snd_code = OC_Video_SetMode;
  m.snd_key0 = KR_VOID;
  m.snd_key1 = KR_VOID;
  m.snd_key2 = KR_VOID;
  m.snd_rsmkey = KR_VOID;
  m.snd_data = 0;
  m.snd_len = 0;
  m.snd_w1 = width;
  m.snd_w2 = height;
  m.snd_w3 = depth;

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

  return CALL(&m);
}
