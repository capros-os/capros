#ifndef __VIDEO_H__
#define __VIDEO_H__

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

/* Declarations for code that is implemented by each video driver */


/* Error codes */
#define  RC_Video_AccelError       100
#define  RC_Video_BusError         101 /* error with the bus address
					    register */
#define  RC_Video_HWError          102 /* general error with device itself */
#define  RC_Video_MemMapFailed     103 /* couldn't map the device's memory */
#define  RC_Video_NotInitialized   104 /* attempt to command device
			                  before it's initialized */
#define  RC_Video_HWInitFailed     105 /* error initializing
			                   underlying hardware systems */
#define  RC_Video_BadCursorID      106 /* invalid cursor id */
#define  RC_Video_NotSupported     107 /* hardware doesn't support
					  requested operation */
#define  RC_Video_BadID            108 /* hardware has a maximum id
					  and you exceeded it */

/* The following flags represent what the underlying hardware is
   capable of.  Each graphics device driver should report its
   functionality in terms of these macros. 

   VIDEO_RECT_FILL:  fill a rectangular region with a specified color

   VIDEO_RECT_COPY:  copy a rectangular region from one location to another

   VIDEO_RECT_PAT_FILL: fill a rectangular region with a specified pattern 

   VIDEO_OFFSCREEN:  hardware has "offscreen" memory for storing pixmaps,
                     bitmaps, cursors, etc.

   VIDEO_RASTER_OP: hardware supports using a logical raster op
                     in conjunction with some or all of its accelerated 
                     commands

   VIDEO_HW_CURSOR:  device supports a hardware cursor

   VIDEO_ALPHA_CURSOR: device supports an alpha hardware cursor
*/

#define	VIDEO_RECT_FILL	       0x0001
#define	VIDEO_RECT_COPY	       0x0002
#define	VIDEO_RECT_PAT_FILL    0x0004
#define	VIDEO_OFFSCREEN        0x0008
#define	VIDEO_RASTER_OP	       0x0010
#define	VIDEO_HW_CURSOR	       0x0020
#define VIDEO_ALPHA_CURSOR     0x0200

#include <graphics/rect.h>
#include <graphics/color.h>

uint32_t video_initialize(uint32_t num_data_elements, uint32_t *data_elements,
			  /* out */ uint32_t *next_available_addrspace_slot);

bool video_get_resolution(/* out */ uint32_t *width,
			  /* out */ uint32_t *height,
			  /* out */ uint32_t *depth);

uint32_t video_set_resolution(uint32_t width,
			      uint32_t height,
			      uint32_t depth);

bool video_max_resolution(/* out */ uint32_t *width,
			      /* out */ uint32_t *height,
			      /* out */ uint32_t *depth);

uint32_t video_functionality(void);

uint32_t video_define_cursor(uint32_t cursor_id,
			     point_t hotspot,
			     point_t size,
			     uint32_t depth,
			     uint8_t *bits,
			     uint8_t *mask_bits);

uint32_t video_define_alpha_cursor(uint32_t cursor_id,
				   point_t hotspot,
				   point_t size,
				   uint32_t *bits);

uint32_t video_show_cursor_at(uint32_t cursor_id,
			      point_t location);


uint32_t video_define_pixmap(uint32_t pixmap_id,
			     point_t pixmap_size,
			     uint32_t pixmap_depth,
			     uint8_t *pixmap_bits);

uint32_t video_show_pixmap(uint32_t pixmap_id, point_t src, rect_t dst);

/* Framebuffer manager uses this to copy from shared client memory to
   the actual framebuffer */
bool video_copy_to_fb(uint32_t *buffer, rect_t area, point_t dst,
			  uint32_t buffer_width);

/* Winsys uses this to copy area from one part of framebuffer to
   another quickly.  This is used for moving windows efficiently. */
uint32_t video_copy_rect(rect_t src, rect_t dst);

uint32_t video_rectfill(rect_t r, color_t c, uint32_t raster_op);

#endif
