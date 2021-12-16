#ifndef __INTERN_H__
#define __INTERN_H__
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

#ifdef __cplusplus
extern "C" {
#endif

/* The intern() function allegedly returns a const char *, but in fact 
   it returns a pointer into the middle of an internal structure! */
const char *intern(const char *);
unsigned intern_gensig(const char *, int len);
const char *internWithLength(const char *, int len);

#ifdef __cplusplus
}
#endif

#endif /* __INTERN_H__ */
