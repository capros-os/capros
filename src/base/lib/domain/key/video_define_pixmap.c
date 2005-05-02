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

#include <idl/eros/key.h>

/* From svga_reg.h */
#define PIXMAP_SCANLINE_SIZE(w,d) (( ((w)*(d))+31 ) >> 5)

/*  Call this macro repeatedly to convert 8-bit pixel data to an array
    of 32-bit values.  After each use, the pixel data can be
    extracted  */
#define HEADER_PIXEL(data,pixel) {\
  pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
  pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
  pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
  data += 4; \
}

uint32_t
video_define_pixmap(uint32_t video_key, 
		    uint32_t pixmap_id, 
		    uint32_t pixmap_width,
		    uint32_t pixmap_height,
		    uint32_t pixmap_depth,
		    uint8_t *pixmap_data)
{
  Message m;
  uint32_t size = PIXMAP_SCANLINE_SIZE(pixmap_width, pixmap_depth); 

  uint32_t scanline[size + 5];
  uint32_t line_no = 0;
  uint32_t result = RC_eros_key_RequestError; /* until proven otherwise */

  m.snd_invKey = video_key;
  m.snd_code = OC_Video_DefinePixmapLine;
  m.snd_key0 = KR_VOID;
  m.snd_key1 = KR_VOID;
  m.snd_key2 = KR_VOID;
  m.snd_rsmkey = KR_VOID;
  m.snd_data = scanline;
  m.snd_len = sizeof(scanline);
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

  scanline[0] = pixmap_id;
  scanline[1] = pixmap_width;
  scanline[2] = pixmap_height;
  scanline[3] = pixmap_depth;

  /* Repeat for all scanlines */
  for (line_no = 0; line_no < pixmap_height; line_no++) {
    uint32_t u;
    uint8_t p[3];

    scanline[4] = line_no;
    for (u = 5; u < size+5; u++) {
      HEADER_PIXEL(pixmap_data, p);
      scanline[u] = (p[0] << 16) + (p[1] << 8) + p[2];
    }

    result = CALL(&m);
    if (result != RC_OK)
      return result;
  }
  return result;
}
