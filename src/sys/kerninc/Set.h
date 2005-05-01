#ifndef __SET_H__
#define __SET_H__
/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

/* Word-sized set manipulation operations */
#define WSET_TEST(w, flg)   (w & (flg))
#define WSET_IS(w, flg)     (WSET_TEST(w, flg) == (flg))
#define WSET_ISNOT(w, flg)  ((w & (flg)) == 0)
#define WSET_SET(w, flg)    (w |= (flg))
#define WSET_CLR(w, flg)    (w &= ~(flg))

#endif /* __SET_H__ */
