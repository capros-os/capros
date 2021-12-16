/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
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


#define RESTRICT_VOID      0x1
#define RESTRICT_NUMBER    0x2
#define RESTRICT_SCHED     0x4
#define RESTRICT_SEGMODE   0x8
#define RESTRICT_KEYREGS   0x10
#define RESTRICT_GENREGS   0x20
#define RESTRICT_START     0x40
#define RESTRICT_IOSPACE   0x80

typedef struct ParseType ParseType;
struct ParseType {
  KeyBits           key;
  const char *      is;
  uint32_t          w;
  uint64_t          oid;
  struct RegDescrip *rd;
  uint32_t          slot;
  uint32_t          restriction;
} ;
