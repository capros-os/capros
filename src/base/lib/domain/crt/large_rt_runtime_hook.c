/*
 * Copyright (C) 2007, The EROS Group, LLC.
 *
 * This file is part of the Coyotos Operating System.
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

/* No runtime hook is required for large spaces at the moment. */

#include <stdint.h>
#include <stddef.h>

/** Default runtime hook for large spaces. Declare this to be weak so
    that it can be overridden by truly intrepid programmers. */
const uintptr_t __rt_runtime_hook __attribute__((weak)) = 0; 

/** @bug hack to make sure sbrk() is included */
extern char *sbrk(size_t);
char *(*__rt_sbrk_ptr)(size_t) = sbrk;
