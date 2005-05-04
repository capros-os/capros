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
#include <stddef.h>
#include <eros/target.h>
#include <eros/endian.h>

#include <string.h>

#include "include/mem.h"

struct mem {
  mem_size_t next, prev;
#if MEM_ALIGNMENT == 1
  uint8_t used;
#elif MEM_ALIGNMENT == 2
  uint16_t used;
#elif MEM_ALIGNMENT == 4
  uint32_t used;
#else
#error "unhandled MEM_ALIGNMENT size"
#endif /* MEM_ALIGNMENT */
}; 

static struct mem *ram_end;
static uint8_t ram[MEM_SIZE + sizeof(struct mem) + MEM_ALIGNMENT];

#define MIN_SIZE 12
#define SIZEOF_STRUCT_MEM MEM_ALIGN_SIZE(sizeof(struct mem))
/*#define SIZEOF_STRUCT_MEM (sizeof(struct mem) + \
  (((sizeof(struct mem) % MEM_ALIGNMENT) == 0)? 0 : \
  (4 - (sizeof(struct mem) % MEM_ALIGNMENT))))*/

static struct mem *lfree;   /* pointer to the lowest free block */

static void
plug_holes(struct mem *mem)
{
  struct mem *nmem;
  struct mem *pmem;

  nmem = (struct mem *)&ram[mem->next];
  if (mem != nmem && nmem->used == 0 && 
      (uint8_t *)nmem != (uint8_t *)ram_end) {
    if (lfree == nmem) {
      lfree = mem;
    }
    mem->next = nmem->next;
    ((struct mem *)&ram[nmem->next])->prev = (uint8_t *)mem - ram;
  }
  
  /* plug hole backward */
  pmem = (struct mem *)&ram[mem->prev];
  if (pmem != mem && pmem->used == 0) {
    if (lfree == mem) {
      lfree = pmem;
    }
    pmem->next = mem->next;
    ((struct mem *)&ram[mem->next])->prev = (uint8_t *)pmem - ram;
  }
}


void
mem_init(void)
{
  struct mem *mem;

  memset(ram,0,MEM_SIZE);
  mem = (struct mem *)ram;
  mem->next = MEM_SIZE;
  mem->prev = 0;
  mem->used = 0;
  ram_end = (struct mem *)&ram[MEM_SIZE];
  ram_end->used = 1;
  ram_end->next = MEM_SIZE;
  ram_end->prev = MEM_SIZE;

  lfree = (struct mem *)ram;
}

void
mem_free(void *rmem)
{
  struct mem *mem;

  if (rmem == NULL)  return;
  
  if ((uint8_t *)rmem < (uint8_t *)ram || 
      (uint8_t *)rmem >= (uint8_t *)ram_end) {
    return;
  }
  mem = (struct mem *)((uint8_t *)rmem - SIZEOF_STRUCT_MEM);

  mem->used = 0;

  if (mem < lfree) {
    lfree = mem;
  }
  
  plug_holes(mem);
}


void *
mem_reallocm(void *rmem, mem_size_t newsize)
{
  void *nmem;
  nmem = mem_malloc(newsize);
  if (nmem == NULL) {
    return mem_realloc(rmem, newsize);
  }
  memcpy(nmem, rmem, newsize);
  mem_free(rmem);
  return nmem;
}


void *
mem_realloc(void *rmem, mem_size_t newsize)
{
  mem_size_t size;
  mem_size_t ptr, ptr2;
  struct mem *mem, *mem2;

  /* Expand the size of the allocated memory region so that we can
     adjust for alignment. */
  if ((newsize % MEM_ALIGNMENT) != 0) {
    newsize += MEM_ALIGNMENT - ((newsize + SIZEOF_STRUCT_MEM) % MEM_ALIGNMENT);
  }
  
  if (newsize > MEM_SIZE) {
    return NULL;
  }
  
  if ((uint8_t *)rmem < (uint8_t *)ram || 
      (uint8_t *)rmem >= (uint8_t *)ram_end) {
    return rmem;
  }
  mem = (struct mem *)((uint8_t *)rmem - SIZEOF_STRUCT_MEM);

  ptr = (uint8_t *)mem - ram;

  size = mem->next - ptr - SIZEOF_STRUCT_MEM;
  
  if (newsize + SIZEOF_STRUCT_MEM + MIN_SIZE < size) {
    ptr2 = ptr + SIZEOF_STRUCT_MEM + newsize;
    mem2 = (struct mem *)&ram[ptr2];
    mem2->used = 0;
    mem2->next = mem->next;
    mem2->prev = ptr;
    mem->next = ptr2;
    if (mem2->next != MEM_SIZE) {
      ((struct mem *)&ram[mem2->next])->prev = ptr2;
    }
    plug_holes(mem2);
  }

  return rmem;
}


void *
mem_malloc(mem_size_t size)
{
  mem_size_t ptr, ptr2;
  struct mem *mem, *mem2;

  if (size == 0)     return NULL;

  /* Expand the size of the allocated memory region so that we can
   * adjust for alignment. */
  if ((size % MEM_ALIGNMENT) != 0) {
    size += MEM_ALIGNMENT - ((size + SIZEOF_STRUCT_MEM) % MEM_ALIGNMENT);
  }
  
  if (size > MEM_SIZE)     return NULL;

  for (ptr = (uint8_t *)lfree - ram; ptr < MEM_SIZE; 
       ptr = ((struct mem *)&ram[ptr])->next) {
    mem = (struct mem *)&ram[ptr];
    if (!mem->used &&
	mem->next - (ptr + SIZEOF_STRUCT_MEM) >= size + SIZEOF_STRUCT_MEM) {
      ptr2 = ptr + SIZEOF_STRUCT_MEM + size;
      mem2 = (struct mem *)&ram[ptr2];
      
      mem2->prev = ptr;      
      mem2->next = mem->next;
      mem->next = ptr2;      
      if (mem2->next != MEM_SIZE) {
        ((struct mem *)&ram[mem2->next])->prev = ptr2;
      }
      
      mem2->used = 0;      
      mem->used = 1;

      if (mem == lfree) {
	/* Find next free block after mem */
        while (lfree->used && lfree != ram_end) {
	  lfree = (struct mem *)&ram[lfree->next];
        }
      }
      return (uint8_t *)mem + SIZEOF_STRUCT_MEM;
    }    
  }
  return NULL;
}
