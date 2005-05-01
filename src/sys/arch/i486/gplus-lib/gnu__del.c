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

/* The following goop is needed to keep g++ happy.
 * Sigh. EROS does *no* dynamic allocation, so this is safe.
 * For that matter, it would be safe to elide it alltogether.
 */

void
__builtin_delete (ptr)
     void *ptr;
{
#if 0
    /* This is here to show you what the "official" one does.  If EROS
       ever calls it we're in deep trouble. */
    if (ptr)
	free (ptr);
#endif
}
