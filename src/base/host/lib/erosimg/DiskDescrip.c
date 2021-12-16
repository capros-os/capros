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


#include <string.h>
#include <erosimg/DiskDescrip.h>

static DiskDescrip Descriptors[] = {
				/* first entry is default! */
    { "fd144",    512, 2, 18, 80, "fdboot" },
    { "fd",       512, 2, 18, 80, "fdboot" }, /* fd ==> fd144 */
    { "badfd144", 512, 2, 18, 80, "fdboot" },
};

#define NDESCRIP (sizeof(Descriptors) / sizeof(DiskDescrip))

const DiskDescrip*
dd_GetDefault()
{
  return &Descriptors[0];
}

const DiskDescrip*
dd_Lookup(const char* name)
{
  uint32_t i;

  for (i = 0; i < NDESCRIP; i++) {
    if (!strcmp(name, Descriptors[i].diskName))
      return &Descriptors[i];
  }

  return 0;
}


