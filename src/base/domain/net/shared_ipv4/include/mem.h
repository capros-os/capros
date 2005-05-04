/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
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
#ifndef _MEM_H_
#define _MEM_H_

#include <eros/target.h>

#define MEM_SIZE         32*1024
#define MEM_ALIGNMENT    2 

//typedef uint32_t mem_size_t;
typedef uint32_t mem_size_t;

void mem_init(void);
void *mem_malloc(mem_size_t size);
void mem_free(void *mem);
void *mem_realloc(void *mem, mem_size_t size);
void *mem_reallocm(void *mem, mem_size_t size);

#define MEM_ALIGN_SIZE(size) (size + \
                             ((((size) % MEM_ALIGNMENT) == 0)? 0 : \
                             (MEM_ALIGNMENT - ((size) % MEM_ALIGNMENT))))

#define MEM_ALIGN(addr) (void *)MEM_ALIGN_SIZE((uint32_t)addr)


#endif /*_MEM_H_*/
