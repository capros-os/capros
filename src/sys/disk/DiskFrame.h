#ifndef __DISK_DISKFRAME_H__
#define __DISK_DISKFRAME_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System.
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

#define FRM_TYPE_ZDPAGE		0 /* zero page/empty frame */
#define FRM_TYPE_DPAGE		1
#define FRM_TYPE_ZNODE		2 /* zero node -- only in ckpt area */
#define FRM_TYPE_NODE		3
#define FRM_TYPE_INVALID	0xf

#endif /* __DISK_DISKFRAME_H__ */
