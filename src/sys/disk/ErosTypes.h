#ifndef __EROSTYPES_H__
#define __EROSTYPES_H__
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

#include <eros/target.h>

typedef uint32_t ObCount;
typedef uint64_t OID;

/* #define NULL_LOGLOC   0 */
#define ZERO_LID   0		/* object is a zero object */
#define UNDEF_LID  1		/* no assigned location yet */
#define DEAD_LID   2		/* storage was released */
#define VIRGIN_LID 3		/* dirent under construction */

/* Return true if lid represents an actual location */
#define CONTENT_LID(x) (x >= (0x1u * EROS_OBJECTS_PER_FRAME))

typedef uint32_t logframe_t;
typedef uint32_t lid_t;

#endif /* ErosTypes.h */
