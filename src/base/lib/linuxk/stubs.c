/*
 * Copyright (C) 2007, Strawberry Development Group.

 * This file is part of the CapROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linuxk/linux-emul.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/dmapool.h>
//#include <linux/string.h>
//#include <linux/bitops.h>
#if 0
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/interrupt.h>  /* for in_interrupt() */
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>

#include <asm/io.h>
#include <asm/scatterlist.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>

#include "hcd.h"
#include "usb.h"
#endif
#include <domain/domdbg.h>

#define STUB(proc) kdprintf(KR_OSTREAM, "Called " #proc ", unimplemented.")

void dump_stack(void)
{ STUB(dump_stack); }

void msleep(unsigned int msecs)
{ STUB(msleep); }

unsigned long msleep_interruptible(unsigned int msecs)
{
  STUB(msleep_interruptible);
  return 0;	// no signals in CapROS
}


struct dma_pool * dma_pool_create(const char *name, struct device *dev,
                        size_t size, size_t align, size_t allocation)
{
  STUB(dma_pool_create);
  return NULL;
}

void dma_pool_destroy(struct dma_pool *pool)
{ STUB(dma_pool_destroy); }

void * dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags,
                     dma_addr_t *handle)
{
  STUB(dma_pool_alloc);
  return NULL;
}

void dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t addr)
{ STUB(dma_pool_free); }

void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *handle, gfp_t gfp)
{
  STUB(dma_alloc_coherent);
  return NULL;
}

void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr,
                  dma_addr_t handle)
{ STUB(dma_free_coherent); }
