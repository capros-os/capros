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

#define CONVERT_BYTE(byte) ((byte & 0x01) << 7 | (byte & 0x02) << 5 | \
                            (byte & 0x04) << 3 | (byte & 0x08) << 1 | \
                            (byte & 0x10) >> 1 | (byte & 0x20) >> 3 | \
                            (byte & 0x40) >> 5 | (byte & 0x80) >> 7)

uint32_t
video_define_cursor(uint32_t video_key, 
		    uint32_t cursor_id, 
		    uint32_t hotspot_x,
		    uint32_t hotspot_y,
		    uint32_t cursor_width,
		    uint32_t cursor_height,
		    uint32_t cursor_depth,
		    uint8_t *cursor_bits,
		    uint8_t *cursor_mask_bits)
{
  Message m;

  uint32_t send[(2 * cursor_width) + 7];
  uint32_t u, v;
  uint32_t and[cursor_width];
  uint32_t xor[cursor_width];
  uint32_t bits, mask;

  m.snd_invKey = video_key;
  m.snd_code = OC_Video_DefineCursor;
  m.snd_key0 = KR_VOID;
  m.snd_key1 = KR_VOID;
  m.snd_key2 = KR_VOID;
  m.snd_rsmkey = KR_VOID;
  m.snd_data = send;
  m.snd_len = sizeof(send);
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

  send[0] = cursor_id;
  send[1] = hotspot_x;
  send[2] = hotspot_y;
  send[3] = cursor_width;
  send[4] = cursor_height;
  send[5] = cursor_depth;
  send[6] = cursor_depth;

  /* The following is needed to put the bitmap pixel data in the
     correct byte/bit order for the vmare fifo. */
  for (u = 0; u < cursor_width; u++) {
    bits = (CONVERT_BYTE(cursor_bits[u*2+1]) << 8) |
      (CONVERT_BYTE(cursor_bits[u*2]));

    mask = (CONVERT_BYTE(cursor_mask_bits[u*2+1]) << 8) |
      (CONVERT_BYTE(cursor_mask_bits[u*2]));

    /* FIX: I think these two lines are only valid for 1-bit depth!
       (i.e. bitmaps as opposed to pixmaps) */
    and[u] = 0xFFFF0000 | mask;
    xor[u] = 0x00000000 | bits;
  }

  for (u = 7; u < cursor_width + 7; u++)
    send[u] = and[u-7];

  for (v = u; v < (2 * cursor_width) + 7; v++)
    send[v] = xor[v - u];

  return CALL(&m);
}
