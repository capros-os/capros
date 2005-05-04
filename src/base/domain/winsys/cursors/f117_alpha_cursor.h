#ifndef _F117_ALPHA_CURSOR_H__
#define _F117_ALPHA_CURSOR_H__
/*
 * Copyright (C) 2003 Jonathan S. Shapiro.
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
#include <graphics/color.h>

#define f117_alpha_cursor_width  18
#define f117_alpha_cursor_height 16
#define f117_alpha_cursor_hot_x   0
#define f117_alpha_cursor_hot_y   0

#define TOTALBLEND 0x00000000
#define NOBLEND    0xFF000000
#define DARKERSHADOW 0x44000000
#define LIGHTERSHADOW 0x25000000
#define LIGHTESTSHADOW 0x14000000

#define BDR    (NOBLEND | WHITE)
#define OUT    (TOTALBLEND | BLACK)
#define IN     (NOBLEND | BLACK)
#define SHD    (DARKERSHADOW | BLACK)
#define SH2    (LIGHTERSHADOW | BLACK)
#define SH3    (LIGHTESTSHADOW | BLACK)

static uint32_t alpha_cursor_bits[] = {
  BDR,BDR,SHD,SH2,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,
  BDR, IN,BDR,BDR,SHD,SH2,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,
  OUT,BDR, IN, IN,BDR,BDR,BDR,SHD,SH2,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,OUT,
  OUT,BDR, IN, IN, IN, IN, IN,BDR,BDR,SHD,SH2,OUT,OUT,OUT,OUT,OUT,OUT,OUT,
  OUT,OUT,BDR, IN, IN, IN, IN, IN, IN,BDR,BDR,BDR,SHD,SH2,OUT,OUT,OUT,OUT,
  OUT,OUT,BDR, IN, IN, IN, IN, IN, IN, IN, IN, IN,BDR,BDR,SHD,SH2,OUT,OUT,
  OUT,OUT,BDR, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN,BDR,BDR,SH3,OUT,
  OUT,OUT,OUT,BDR, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN, IN,BDR,SH3,SH3,
  OUT,OUT,OUT,BDR, IN, IN, IN, IN, IN, IN, IN,BDR,BDR,BDR,BDR,BDR,SH3,SH3,
  OUT,OUT,OUT,OUT,BDR, IN, IN, IN, IN, IN, IN,BDR,SHD,SHD,SH2,SH2,SH2,OUT,
  OUT,OUT,OUT,OUT,BDR, IN, IN, IN, IN, IN, IN,BDR,SH2,SH2,SH3,SH3,SH3,OUT,
  OUT,OUT,OUT,OUT,BDR, IN, IN, IN,BDR,BDR,BDR,BDR,BDR,BDR,BDR,BDR,SHD,OUT,
  OUT,OUT,OUT,OUT,OUT,BDR, IN, IN,BDR,SHD,SH2,BDR, IN, IN, IN,BDR,SH2,OUT,
  OUT,OUT,OUT,OUT,OUT,BDR, IN, IN,BDR,SH2,SH2,BDR, IN,BDR,BDR,BDR,OUT,OUT,
  OUT,OUT,OUT,OUT,OUT,OUT,BDR, IN,BDR,SH2,SH3,BDR, IN,BDR,SH2,SH3,OUT,OUT,
  OUT,OUT,OUT,OUT,OUT,OUT,BDR,BDR,BDR,SH3,OUT,BDR,BDR,BDR,SH3,OUT,OUT,OUT,
};

#endif
