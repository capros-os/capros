#ifndef __DEBUG_H__
#define __DEBUG_H__

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

#define dbg_driver_none   0x000u
#define dbg_video_init    0x001u
#define dbg_video_cmds    0x002u
#define dbg_video_bar     0x004u
#define dbg_video_caps    0x008u
#define dbg_video_fb      0x010u
#define dbg_video_fifo    0x020u
#define dbg_set_pixel     0x040u
#define dbg_msg_trunc     0x080u
#define dbg_drawable_cmds 0x100u
#define dbg_redraw        0x200u
#define dbg_clip          0x400u
#define dbg_video_addrspc 0x800u
#define dbg_pixmap        0x1000u
#define dbg_fbm_cmds      0x2000u

#define dbg_driver_flags  (dbg_driver_none)

#define CND_DEBUG(x) (dbg_##x & dbg_driver_flags)
#define DEBUG(x) if (CND_DEBUG(x))
#define DEBUG2(x,y) if (((dbg_##x|dbg_##y) & dbg_driver_flags) == \
                                            (dbg_##x|dbg_##y))

#endif
