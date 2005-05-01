#ifndef __LOG386_HXX__
#define __LOG386_HXX__
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


class Log386 {
  static uint16_t *curPos;

  static int column;		/* can be computed, but keeping it */
  static int row;		/* avoids division. */

  static void UpdateCursor();
  static void Putc(const int);
  static void Clear();
  static void Scroll();
  static void CallBack();
public:
  static void Init();
};

#endif /* __LOG386_HXX__ */
