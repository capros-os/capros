#ifndef __MEMORY_H__
#define __MEMORY_H__
/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

#include <arch-kerninc/memory.h>

#ifdef __cplusplus
extern "C" {
#endif
int memcmp(const void *m1, const void *m2, size_t len);
#ifdef __cplusplus
}
#endif

#ifndef bzero
extern void generic_bzero(void * where, size_t len);

#define __generic_bzero(d, len) \
    generic_bzero(d, len)
#define bzero(d, len) __generic_bzero(d, len)

#endif

#ifndef bcopy

extern void generic_bcopy(const void *from, void *to, size_t len);

#define __generic_bcopy(s, d, len) \
    generic_bcopy(s, d, len)
#define bcopy(s, d, len) __generic_bcopy(s, d, len)

#endif
#endif /* __MEMORY_H__ */
