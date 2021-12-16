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


#include <eros/target.h>

int
compare(uint32_t *k1, uint32_t *k2)
{
  if (k1[0] < k2[0]) {
    return -1;
  } else if (k1[0] > k2[0]) {
    return 1;
  }

  if (k1[1] < k2[1]) {
    return -1;
  } else if (k1[1] > k2[1]) {
    return 1;
  }

  if (k1[2] < k2[2]) {
    return -1;
  } else if (k1[2] > k2[2]) {
    return 1;
  }

  if (k1[3] < k2[3]) {
    return -1;
  } else if (k1[3] > k2[3]) {
    return 1;
  }

  return 0; /* equal */
}
