#ifndef misc_h
#define misc_h
/*
 * Copyright (C) 1998, 1999, Jonathan Adams.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <disk/ErosTypes.h>	// define OID

#if CONVERSION
enum {
  false = 0,
  true = 1
} ;

typedef uint8_t bool;
#endif

/* DIVRNDUP -- takes two integers (x,y) and returns x/y rounded up */
#define DIVRNDUP(x,y) (((x) + (y) - 1)/(y))

/* FLOOR2 -- gives (x - (x%y)). Assumes y is a power of two */
#define FLOOR2(x,y)  ( (x) & ~((y)-1) )

/* MOD2 -- gives (x%y). Assumes y is a power of two */
#define MOD2(x,y)  ( (x) & ((y)-1) )

#define MAX(x,y) ((x) < (y) ? (y) : (x))
#define MIN(x,y) ((x) <= (y) ? (x) : (y))
#endif
