/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

#include "video.h"

struct graphics_driver {
  
  uint32_t (*video_initialize)(uint32_t num_data_elements, uint32_t *data_elements,
			       /* out */ uint32_t *next_available_addrspace_slot);

  bool (*video_get_resolution)(/* out */ uint32_t *width,
			       /* out */ uint32_t *height,
			       /* out */ uint32_t *depth);

  uint32_t (*video_set_resolution)(uint32_t width,
				   uint32_t height,
				   uint32_t depth);

  bool (*video_max_resolution)(/* out */ uint32_t *width,
			       /* out */ uint32_t *height,
			       /* out */ uint32_t *depth);

  uint32_t (*video_functionality)(void);

  uint32_t (*video_define_cursor)(uint32_t cursor_id,
				  point_t hotspot,
				  point_t size,
				  uint32_t depth,
				  uint8_t *bits,
				  uint8_t *mask_bits);

  uint32_t (*video_define_alpha_cursor)(uint32_t cursor_id,
					point_t hotspot,
					point_t size,
					uint32_t *bits);

  uint32_t (*video_show_cursor_at)(uint32_t cursor_id,
				   point_t location);

  uint32_t (*video_define_vram_pixmap)(uint32_t pixmap_id,
				  point_t pixmap_size,
				  uint32_t pixmap_depth,
				  uint8_t *pixmap_bits);
  uint32_t (*video_show_vram_pixmap)(uint32_t pixmap_id, point_t src, rect_t dst);

  bool (*video_copy_to_fb)(uint32_t *buffer, rect_t area, point_t dst,
			   uint32_t buffer_width);

  uint32_t (*video_copy_rect)(rect_t src, rect_t dst);

  uint32_t (*video_rectfill)(rect_t r, color_t c, uint32_t raster_op);

  uint16_t vendor_id;
  const char *name_string;
};

/* terminated by an entry with NULL pointer for video_initialize */
extern struct graphics_driver graphics_driver_table[];

extern struct graphics_driver *current_graphics_driver;
