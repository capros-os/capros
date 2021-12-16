#ifndef __SYMNAMES_H__
#define __SYMNAMES_H__
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

/* Following structure is hacked together using what might charitably
 * be called a *gross* nm hack.
 */

struct FuncSym {
  uint32_t profCount;
  uint32_t address;
  const char *raw_name;
  const char *name;
};

extern uint32_t funcSym_count;
extern struct FuncSym funcSym_table[];	/* defined in symnames.s */

struct LineSym {
  uint32_t address;
  uint32_t line;
  const char *file;
};

extern uint32_t lineSym_count;
extern struct LineSym lineSym_table[];	/* defined in symnames.s */

#endif /* __SYMNAMES_H__ */
