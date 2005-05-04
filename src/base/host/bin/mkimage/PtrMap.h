#ifndef __PTRMAP_H__
#define __PTRMAP_H__
/*
 * Copyright (C) 1998, 1999, 2002, Jonathan S. Shapiro.
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

/* Versions of the non-inline routines exist in the library to map
 * pointers to Words, to uint64_ts, to DiskKeys, and to void *
 */

typedef struct PtrMap PtrMap;

#ifdef __cplusplus
extern "C" {
#endif

PtrMap *ptrmap_create();
void ptrmap_destroy(PtrMap *pm);

bool ptrmap_Contains(PtrMap *pm, const void *ptr);
bool ptrmap_Lookup(PtrMap *pm, const void *ptr, KeyBits *value);
void ptrmap_Add(PtrMap *pm, const void *ptr, KeyBits value);

#ifdef __cplusplus
}
#endif

#endif /* __PTRMAP_H__ */
