#ifndef __GLOBAL_H__
#define __GLOBAL_H__

/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* Shared memory space dimensions */
#define SHARED_BUFFER_WIDTH   2048
#define SHARED_BUFFER_HEIGHT  1024
/*  (in bytes...) */
#define SHARED_BUFFER_DEPTH      4

/* macro for examining pointer addresses */
#define ADDRESS(w) ((uint32_t)w)

/* This service implements these interfaces: */
#define WINDOW_SYSTEM_INTERFACE   0x000bu
#define SESSION_INTERFACE         0x000cu
#define TRUSTED_SESSION_INTERFACE 0x000du
#define SESSION_CREATOR_INTERFACE 0x000eu
#define TRUSTED_SESSION_CREATOR_INTERFACE 0x000fu

#ifndef min
  #define min(a,b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef max
  #define max(a,b) ((a) >= (b) ? (a) : (b))
#endif

#endif
