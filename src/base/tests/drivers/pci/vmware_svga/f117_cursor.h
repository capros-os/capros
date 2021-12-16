#ifndef _F117_CURSOR_H__
#define _F117_CURSOR_H__
/*
 * Copyright (C) 2002 Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#define f117_cursor_width  16
#define f117_cursor_height 16
#define f117_cursor_depth   1
#define f117_cursor_hot_x   0
#define f117_cursor_hot_y   0

static uint32_t f117_cursor_id = 3;  /* arbitrary id */

static uint8_t f117_cursor_bits[] = {
   0x03, 0x00, 0x1d, 0x00, 0xe2, 0x00, 0x02, 0x07, 0x02, 0x38, 0x04, 0xc0,
   0x04, 0x80, 0x04, 0xfc, 0x08, 0x04, 0x08, 0x08, 0x88, 0x09, 0x90, 0xfe,
   0x90, 0x88, 0x90, 0xe8, 0xa0, 0x28, 0xe0, 0x38};

static uint8_t f117_cursor_mask_bits[] = {
   0xfc, 0xff, 0xe0, 0xff, 0x01, 0xff, 0x01, 0xf8, 0x01, 0xc0, 0x03, 0x00,
   0x03, 0x00, 0x03, 0x00, 0x07, 0xf8, 0x07, 0xf0, 0x07, 0xf0, 0x0f, 0x01,
   0x0f, 0x07, 0x0f, 0x07, 0x1f, 0xc7, 0x1f, 0xc7};

#endif
