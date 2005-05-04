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

#include <stdlib.h>
#include <string.h>

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/eros/key.h>
#include "video.h"
#include "drivers.h"

#define VMWARE_VENDOR_ID 0x15AD
#define BOCHS_VENDOR_ID  0x1234

struct graphics_driver *current_graphics_driver = NULL;


extern uint32_t vmware_video_initialize(uint32_t num_data_elements, uint32_t *data_elements,
			  /* out */ uint32_t *next_available_addrspace_slot);
extern bool vmware_video_get_resolution(/* out */ uint32_t *width,
			  /* out */ uint32_t *height,
			  /* out */ uint32_t *depth);
extern uint32_t vmware_video_set_resolution(uint32_t width,
			      uint32_t height,
			      uint32_t depth);
extern bool vmware_video_max_resolution(/* out */ uint32_t *width,
			      /* out */ uint32_t *height,
			      /* out */ uint32_t *depth);
extern uint32_t vmware_video_functionality(void);
extern uint32_t vmware_video_define_cursor(uint32_t cursor_id,
			     point_t hotspot,
			     point_t size,
			     uint32_t depth,
			     uint8_t *bits,
			     uint8_t *mask_bits);
extern uint32_t vmware_video_define_alpha_cursor(uint32_t cursor_id,
						 point_t hotspot,
						 point_t size,
						 uint32_t *bits);
extern uint32_t vmware_video_show_cursor_at(uint32_t cursor_id,
			      point_t location);
extern uint32_t vmware_video_define_vram_pixmap(uint32_t pixmap_id,
			     point_t pixmap_size,
			     uint32_t pixmap_depth,
			     uint8_t *pixmap_bits);
extern uint32_t vmware_video_show_vram_pixmap(uint32_t pixmap_id, point_t src, rect_t dst);
extern bool vmware_video_copy_to_fb(uint32_t *buffer, rect_t area, point_t dst,
			  uint32_t buffer_width);
extern uint32_t vmware_video_copy_rect(rect_t src, rect_t dst);

extern uint32_t vmware_video_rectfill(rect_t r, color_t c, uint32_t raster_op);
extern uint32_t bochs_video_initialize(uint32_t num_data_elements, uint32_t *data_elements,
			  /* out */ uint32_t *next_available_addrspace_slot);
extern bool bochs_video_get_resolution(/* out */ uint32_t *width,
			  /* out */ uint32_t *height,
			  /* out */ uint32_t *depth);
extern uint32_t bochs_video_set_resolution(uint32_t width,
			      uint32_t height,
			      uint32_t depth);
extern bool bochs_video_max_resolution(/* out */ uint32_t *width,
			      /* out */ uint32_t *height,
			      /* out */ uint32_t *depth);
extern uint32_t bochs_video_functionality(void);
extern uint32_t bochs_video_define_cursor(uint32_t cursor_id,
			     point_t hotspot,
			     point_t size,
			     uint32_t depth,
			     uint8_t *bits,
			     uint8_t *mask_bits);
extern uint32_t bochs_video_define_alpha_cursor(uint32_t cursor_id,
						point_t hotspot,
						point_t size,
						uint32_t *bits);
extern uint32_t bochs_video_show_cursor_at(uint32_t cursor_id,
			      point_t location);
extern uint32_t bochs_video_define_vram_pixmap(uint32_t pixmap_id,
			     point_t pixmap_size,
			     uint32_t pixmap_depth,
			     uint8_t *pixmap_bits);
extern uint32_t bochs_video_show_vram_pixmap(uint32_t pixmap_id, point_t src, rect_t dst);
extern bool bochs_video_copy_to_fb(uint32_t *buffer, rect_t area, point_t dst,
			  uint32_t buffer_width);
extern uint32_t bochs_video_copy_rect(rect_t src, rect_t dst);
extern uint32_t bochs_video_rectfill(rect_t r, color_t c, uint32_t raster_op);

struct graphics_driver graphics_driver_table[] = {
  {vmware_video_initialize,
   vmware_video_get_resolution,
   vmware_video_set_resolution,
   vmware_video_max_resolution,
   vmware_video_functionality,
   vmware_video_define_cursor,
   vmware_video_define_alpha_cursor,
   vmware_video_show_cursor_at,
   vmware_video_define_vram_pixmap,
   vmware_video_show_vram_pixmap,
   vmware_video_copy_to_fb,
   vmware_video_copy_rect,
   vmware_video_rectfill,
   VMWARE_VENDOR_ID,
   "VMWare virtual graphics device"},
  {bochs_video_initialize,
   bochs_video_get_resolution,
   bochs_video_set_resolution,
   bochs_video_max_resolution,
   bochs_video_functionality,
   bochs_video_define_cursor,
   bochs_video_define_alpha_cursor,
   bochs_video_show_cursor_at,
   bochs_video_define_vram_pixmap,
   bochs_video_show_vram_pixmap,
   bochs_video_copy_to_fb,
   bochs_video_copy_rect,
   bochs_video_rectfill,
   BOCHS_VENDOR_ID,
  "Bochs virtual graphics device"},
  {NULL}
};

#define MAX_SOFT_PIXMAPS 32
struct ram_pixmap {
  point_t size;
  uint32_t depth;
  uint32_t *bits;
};
static struct ram_pixmap ram_pixmaps[MAX_SOFT_PIXMAPS] = {};

uint32_t video_initialize(uint32_t num_data_elements, uint32_t *data_elements,
			       /* out */ uint32_t *next_available_addrspace_slot)
{
  return current_graphics_driver->video_initialize(num_data_elements, data_elements,
						   next_available_addrspace_slot);
}

bool video_get_resolution(/* out */ uint32_t *width,
			  /* out */ uint32_t *height,
			  /* out */ uint32_t *depth)
{
  return current_graphics_driver->video_get_resolution(width,height,depth);
}

uint32_t video_set_resolution(uint32_t width,
			      uint32_t height,
			      uint32_t depth)
{
  return current_graphics_driver->video_set_resolution(width,height,depth);
}

bool video_max_resolution(/* out */ uint32_t *width,
			  /* out */ uint32_t *height,
			  /* out */ uint32_t *depth)
{
  return current_graphics_driver->video_max_resolution(width,height,depth);
}

uint32_t video_functionality(void)
{
  return current_graphics_driver->video_functionality();
}

uint32_t video_define_cursor(uint32_t cursor_id,
			     point_t hotspot,
			     point_t size,
			     uint32_t depth,
			     uint8_t *bits,
			     uint8_t *mask_bits)
{
  return current_graphics_driver->video_define_cursor(cursor_id,hotspot,size,depth,
						      bits, mask_bits);
}

uint32_t video_define_alpha_cursor(uint32_t cursor_id,
				   point_t hotspot,
				   point_t size,
				   uint32_t *bits)
{
  return current_graphics_driver->video_define_alpha_cursor(cursor_id,
							    hotspot,
							    size,
							    bits);
}

uint32_t video_show_cursor_at(uint32_t cursor_id,
			      point_t location)
{
  return current_graphics_driver->video_show_cursor_at(cursor_id,location);
}

/* Load a pixmap and return a unique id for that pixmap */
uint32_t video_define_pixmap(uint32_t pixmap_id,
			     point_t pixmap_size,
			     uint32_t pixmap_depth,
			     uint8_t *pixmap_bits)
{
  if (pixmap_id < MAX_SOFT_PIXMAPS && ram_pixmaps[pixmap_id].bits) {
    free(ram_pixmaps[pixmap_id].bits);
    ram_pixmaps[pixmap_id].bits = NULL;
  }

  if (RC_OK == current_graphics_driver->
                 video_define_vram_pixmap(pixmap_id, pixmap_size, pixmap_depth, pixmap_bits))
    return RC_OK;

  /* ok, the card couldn't cache it in vram, so we need to malloc() a buffer for it. */
  if (pixmap_id < MAX_SOFT_PIXMAPS) {
    uint32_t size = pixmap_size.x*pixmap_size.y*(pixmap_depth+7)/8;
    ram_pixmaps[pixmap_id].bits = malloc(size);
    if (ram_pixmaps[pixmap_id].bits == NULL)
      return RC_eros_key_RequestError; /* FIX: do something better */

    ram_pixmaps[pixmap_id].size = pixmap_size;
    ram_pixmaps[pixmap_id].depth = pixmap_depth;
    memcpy (ram_pixmaps[pixmap_id].bits, pixmap_bits, size);
  }
  return RC_OK;
}

uint32_t video_show_pixmap(uint32_t pixmap_id, point_t src, rect_t dst)
{
  if (pixmap_id < MAX_SOFT_PIXMAPS && ram_pixmaps[pixmap_id].bits) {

    rect_t area;
    area.topLeft = src;
    area.bottomRight.x = dst.bottomRight.x - dst.topLeft.x;
    area.bottomRight.y = dst.bottomRight.y - dst.topLeft.y;

    return video_copy_to_fb(ram_pixmaps[pixmap_id].bits, area,
			    dst.topLeft, ram_pixmaps[pixmap_id].size.x)
      ? RC_OK : RC_eros_key_RequestError;
  } else
    return current_graphics_driver->video_show_vram_pixmap(pixmap_id,src,dst);
}

bool video_copy_to_fb(uint32_t *buffer, rect_t area, point_t dst,
		      uint32_t buffer_width)
{
  return current_graphics_driver->video_copy_to_fb(buffer,area,dst,buffer_width);
}

uint32_t video_copy_rect(rect_t src, rect_t dst)
{
  return current_graphics_driver->video_copy_rect(src, dst);
}

uint32_t video_rectfill(rect_t r, color_t c, uint32_t raster_op)
{
  return current_graphics_driver->video_rectfill(r,c,raster_op);
}
