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

#include <kerninc/BootInfo.h>
#include "boot-asm.h"
#include "boot.h"
#include "debug.h"

extern int printf(const char*, ...);
extern void halt();

static uint8_t* heap = PA2BOOT(BOOT2_HEAP_PADDR, uint8_t *);
static uint8_t* pfree = 0;
int avail = BOOT2_HEAP_SZ;

struct free_block {
  size_t sz;
  struct free_block *next;
} ;
typedef struct free_block free_block;

free_block *free_list = 0;

/* This is a cheap, butt-ugly HACK.  We take shameless advantage of
   the fact that our only client here is zlib, which allocates and
   deallocates the same sized stuff over and over.  We therefore do no
   free list merging at all. */
void
free(void *buf)
{
  free_block *fb;
  uint32_t* wbuf = (uint32_t *) buf;
  wbuf--;
  fb = (free_block *) wbuf;

  fb->next = free_list;
  free_list = fb;
}

void *
malloc(size_t sz)
{
  size_t truesz;

  free_block *prev_free = 0;
  free_block *cur_free = free_list;
  free_block *found = 0;
  
  sz += 3;
  sz -= (sz % 4);
  
  truesz = sz + 4;			/* to hold sz of allocated region */
  
  /* check for a member on the free chain of just the right size... */
  while (cur_free) {
    DEBUG(heap) printf("check cur_free=0x%lx, sz=%d\n",
		       (unsigned long )cur_free, cur_free->sz);
    if (cur_free->sz == sz) {
      if (prev_free)
	prev_free->next = cur_free->next;
      else
	free_list = cur_free->next;

      found = cur_free;
      DEBUG(heap) printf("  found from free chain\n");
      break;
    }

    prev_free = cur_free;
    cur_free = cur_free->next;
  }

  /* next pointer field becomes first word of content */
  if (!found) {
    if (pfree == 0)
      pfree = heap;

    if (avail < truesz) {
      printf("bootstrap heap exhausted -- req %d avail %d\n",
	     sz, avail);
      halt();
    }
    
    found = (free_block *) pfree;
    found->sz = sz;

    pfree += truesz;
    avail -= truesz;
  }
  
  return &found->next;
}

void
zcfree(void *opaque, void *vp)
{
  free_block *fb;
  uint32_t* wbuf = (uint32_t *) vp;
  wbuf--;
  fb = (free_block *) wbuf;

  DEBUG(heap) printf("zfree(0x%lx [%d bytes])\n",
		     (unsigned long)vp, fb->sz);
  /*   waitkbd(); */
  free(vp);
}

void *
zcalloc(void *opaque, unsigned items, unsigned size)
{
  DEBUG(heap) printf("zcalloc(...,%d,%d [tot=%d, avail=%d]"
		     " pfree=0x%lx)\n", items, size,
		     items*size, avail, (unsigned long) pfree);
  return malloc(items * size);
}
