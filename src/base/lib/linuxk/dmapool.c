/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linuxk/linux-emul.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <asm/io.h>		/* Needed for i386 to build */
#include <asm/scatterlist.h>	/* Needed for i386 to build */
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/sched.h>

/*
 * Pool allocator ... wraps the dma_alloc_coherent page allocator, so
 * small blocks are easily used by drivers for bus mastering controllers.
 * This should probably be sharing the guts of the slab allocator.
 */

// Choice of lock:
// Spinlock is required, because dma_pool_alloc is called from
// under a spinlock (for example, in ed_get/ed_alloc in drivers/usb/host).
#define USE_SPINLOCK 1
#if USE_SPINLOCK
#define poolLockDecl spinlock_t lock;
#define poolLockInit(pool) spin_lock_init(&(pool)->lock);
#define poolLockFlagsDecl unsigned long flags;
#define poolLockLock(pool) spin_lock_irqsave(&(pool)->lock, flags);
#define poolLockUnlock(pool) spin_unlock_irqrestore(&(pool)->lock, flags);
#else
#include <linux/mutex.h>
#define poolLockDecl struct mutex mutx;
#define poolLockInit(pool) mutex_init(&(pool)->mutx);
#define poolLockFlagsDecl
#define poolLockLock(pool) mutex_lock(&(pool)->mutx);
#define poolLockUnlock(pool) mutex_unlock(&(pool)->mutx);
#endif

struct dma_pool {	/* the pool */
	struct list_head	page_list;
	poolLockDecl
	size_t			blocks_per_page;
	size_t			size;
	struct device		*dev;
	size_t			allocation;
	char			name [32];
	wait_queue_head_t	waitq;
};

struct dma_page {	/* cacheable header for 'allocation' bytes */
	struct list_head	page_list;
	void			*vaddr;
	dma_addr_t		dma;
	unsigned		in_use;
	unsigned long		bitmap [0];
};

#define	POOL_TIMEOUT_JIFFIES	((100 /* msec */ * HZ) / 1000)

/**
 * dma_pool_create - Creates a pool of consistent memory blocks, for dma.
 * @name: name of pool, for diagnostics
 * @dev: device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @allocation: returned blocks won't cross this boundary (or zero)
 * Context: !in_interrupt()
 *
 * Returns a dma allocation pool with the requested characteristics, or
 * null if one can't be created.  Given one of these pools, dma_pool_alloc()
 * may be used to allocate memory.  Such memory will all have "consistent"
 * DMA mappings, accessible by the device and its driver without using
 * cache flushing primitives.  The actual size of blocks allocated may be
 * larger than requested because of alignment.
 *
 * If allocation is nonzero, objects returned from dma_pool_alloc() won't
 * cross that size boundary.  This is useful for devices which have
 * addressing restrictions on individual DMA transfers, such as not crossing
 * boundaries of 4KBytes.
 */
struct dma_pool *
dma_pool_create (const char *name, struct device *dev,
	size_t size, size_t align, size_t allocation)
{
	struct dma_pool		*retval;

	if (align == 0)
		align = 1;
	if (size == 0)
		return NULL;
	else if (size < align)
		size = align;
	else if ((size % align) != 0) {
		size += align - 1;
		size &= ~(align - 1);
	}
	// size is now a multiple of align.

	if (allocation == 0) {
		if (PAGE_SIZE < size)
			allocation = size;
		else
			allocation = PAGE_SIZE;
		// FIXME: round up for less fragmentation
	} else if (allocation < size)
		return NULL;

	if (!(retval = kmalloc (sizeof *retval, GFP_KERNEL)))
		return retval;

	strlcpy (retval->name, name, sizeof retval->name);

	retval->dev = dev;

	INIT_LIST_HEAD (&retval->page_list);
	poolLockInit(retval)
	retval->size = size;
	retval->allocation = allocation;
	retval->blocks_per_page = allocation / size;
	init_waitqueue_head (&retval->waitq);

	return retval;
}


static struct dma_page *
pool_alloc_page (struct dma_pool *pool, gfp_t mem_flags)
{
	struct dma_page	*page;
	int		mapsize;

	mapsize = pool->blocks_per_page;
	mapsize = (mapsize + BITS_PER_LONG - 1) / BITS_PER_LONG;
	mapsize *= sizeof (long);

	page = kmalloc(mapsize + sizeof *page, mem_flags);
	if (!page)
		return NULL;
	page->vaddr = dma_alloc_coherent (pool->dev,
					    pool->allocation,
					    &page->dma,
					    mem_flags);
	if (page->vaddr) {
		memset (page->bitmap, 0xff, mapsize);	// bit set == free
#ifdef	CONFIG_DEBUG_SLAB
		memset (page->vaddr, POOL_POISON_FREED, pool->allocation);
#endif
		list_add (&page->page_list, &pool->page_list);
		page->in_use = 0;
	} else {
		kfree (page);
		page = NULL;
	}
	return page;
}


static inline int
is_page_busy (int blocks, unsigned long *bitmap)
{
	while (blocks > 0) {
		if (*bitmap++ != ~0UL)
			return 1;
		blocks -= BITS_PER_LONG;
	}
	return 0;
}

static void
pool_free_page (struct dma_pool *pool, struct dma_page *page)
{
	dma_addr_t	dma = page->dma;

#ifdef	CONFIG_DEBUG_SLAB
	memset (page->vaddr, POOL_POISON_FREED, pool->allocation);
#endif
	dma_free_coherent (pool->dev, pool->allocation, page->vaddr, dma);
	list_del (&page->page_list);
	kfree (page);
}


/**
 * dma_pool_destroy - destroys a pool of dma memory blocks.
 * @pool: dma pool that will be destroyed
 * Context: !in_interrupt()
 *
 * Caller guarantees that no more memory from the pool is in use,
 * and that nothing will try to use the pool after this call.
 */
void
dma_pool_destroy (struct dma_pool *pool)
{
	while (!list_empty (&pool->page_list)) {
		struct dma_page		*page;
		page = list_entry (pool->page_list.next,
				struct dma_page, page_list);
		if (is_page_busy (pool->blocks_per_page, page->bitmap)) {
			if (pool->dev)
				dev_err(pool->dev, "dma_pool_destroy %s, %p busy\n",
					pool->name, page->vaddr);
			else
				printk (KERN_ERR "dma_pool_destroy %s, %p busy\n",
					pool->name, page->vaddr);
			/* leak the still-in-use consistent memory */
			list_del (&page->page_list);
			kfree (page);
		} else
			pool_free_page (pool, page);
	}

	kfree (pool);
}


/**
 * dma_pool_alloc - get a block of consistent memory
 * @pool: dma pool that will produce the block
 * @mem_flags: GFP_* bitmask
 * @handle: pointer to dma address of block
 *
 * This returns the kernel virtual address of a currently unused block,
 * and reports its dma address through the handle.
 * If such a memory block can't be allocated, null is returned.
 */
void *
dma_pool_alloc (struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
	poolLockFlagsDecl
	struct dma_page		*page;
	int			map, block;
	size_t			offset;
	void			*retval;

	poolLockLock(pool)
	list_for_each_entry(page, &pool->page_list, page_list) {
		int		i;
		/* only cachable accesses here ... */
		for (map = 0, i = 0;
				i < pool->blocks_per_page;
				i += BITS_PER_LONG, map++) {
			if (page->bitmap [map] == 0)
				continue;
			block = ffz (~ page->bitmap [map]);
			offset = i + block;
			if (offset < pool->blocks_per_page) {
				clear_bit (block, &page->bitmap [map]);
				offset *= pool->size;
				goto ready;
			}
		}
	}
	if (!(page = pool_alloc_page(pool, mem_flags))) {
		retval = NULL;
		goto done;
	}

	clear_bit (0, &page->bitmap [0]);
	offset = 0;
ready:
	page->in_use++;
	retval = offset + page->vaddr;
	*handle = offset + page->dma;
#ifdef	CONFIG_DEBUG_SLAB
	memset (retval, POOL_POISON_ALLOCATED, pool->size);
#endif
done:
	poolLockUnlock(pool)
	return retval;
}


static struct dma_page *
pool_find_page (struct dma_pool *pool, dma_addr_t dma)
{
	poolLockFlagsDecl
	struct dma_page		*page;

	poolLockLock(pool)
	list_for_each_entry(page, &pool->page_list, page_list) {
		if (dma < page->dma)
			continue;
		if (dma < (page->dma + pool->allocation))
			goto done;
	}
	page = NULL;
done:
	poolLockUnlock(pool)
	return page;
}


/**
 * dma_pool_free - put block back into dma pool
 * @pool: the dma pool holding the block
 * @vaddr: virtual address of block
 * @dma: dma address of block
 *
 * Caller promises neither device nor driver will again touch this block
 * unless it is first re-allocated.
 */
void
dma_pool_free (struct dma_pool *pool, void *vaddr, dma_addr_t dma)
{
	struct dma_page		*page;
	poolLockFlagsDecl
	int			map, block;

	if ((page = pool_find_page (pool, dma)) == 0) {
		if (pool->dev)
			dev_err(pool->dev, "dma_pool_free %s, %p/%lx (bad dma)\n",
				pool->name, vaddr, (unsigned long) dma);
		else
			printk (KERN_ERR "dma_pool_free %s, %p/%lx (bad dma)\n",
				pool->name, vaddr, (unsigned long) dma);
		return;
	}

	block = dma - page->dma;
	block /= pool->size;
	map = block / BITS_PER_LONG;
	block %= BITS_PER_LONG;

#ifdef	CONFIG_DEBUG_SLAB
	if (((dma - page->dma) + (void *)page->vaddr) != vaddr) {
		if (pool->dev)
			dev_err(pool->dev, "dma_pool_free %s, %p (bad vaddr)/%Lx\n",
				pool->name, vaddr, (unsigned long long) dma);
		else
			printk (KERN_ERR "dma_pool_free %s, %p (bad vaddr)/%Lx\n",
				pool->name, vaddr, (unsigned long long) dma);
		return;
	}
	if (page->bitmap [map] & (1UL << block)) {
		if (pool->dev)
			dev_err(pool->dev, "dma_pool_free %s, dma %Lx already free\n",
				pool->name, (unsigned long long)dma);
		else
			printk (KERN_ERR "dma_pool_free %s, dma %Lx already free\n",
				pool->name, (unsigned long long)dma);
		return;
	}
	memset (vaddr, POOL_POISON_FREED, pool->size);
#endif

	poolLockLock(pool)
	page->in_use--;
	set_bit (block, &page->bitmap [map]);
	if (waitqueue_active (&pool->waitq))
		wake_up (&pool->waitq);
	/*
	 * Resist a temptation to do
	 *    if (!is_page_busy(bpp, page->bitmap)) pool_free_page(pool, page);
	 * Better have a few empty pages hang around.
	 */
	poolLockUnlock(pool)
}

#if 0	// don't need this
/*
 * Managed DMA pool
 */
static void dmam_pool_release(struct device *dev, void *res)
{
	struct dma_pool *pool = *(struct dma_pool **)res;

	dma_pool_destroy(pool);
}

static int dmam_pool_match(struct device *dev, void *res, void *match_data)
{
	return *(struct dma_pool **)res == match_data;
}

/**
 * dmam_pool_create - Managed dma_pool_create()
 * @name: name of pool, for diagnostics
 * @dev: device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @allocation: returned blocks won't cross this boundary (or zero)
 *
 * Managed dma_pool_create().  DMA pool created with this function is
 * automatically destroyed on driver detach.
 */
struct dma_pool *dmam_pool_create(const char *name, struct device *dev,
				  size_t size, size_t align, size_t allocation)
{
	struct dma_pool **ptr, *pool;

	ptr = devres_alloc(dmam_pool_release, sizeof(*ptr), GFP_KERNEL);
	if (!ptr)
		return NULL;

	pool = *ptr = dma_pool_create(name, dev, size, align, allocation);
	if (pool)
		devres_add(dev, ptr);
	else
		devres_free(ptr);

	return pool;
}

/**
 * dmam_pool_destroy - Managed dma_pool_destroy()
 * @pool: dma pool that will be destroyed
 *
 * Managed dma_pool_destroy().
 */
void dmam_pool_destroy(struct dma_pool *pool)
{
	struct device *dev = pool->dev;

	dma_pool_destroy(pool);
	WARN_ON(devres_destroy(dev, dmam_pool_release, dmam_pool_match, pool));
}

EXPORT_SYMBOL (dma_pool_create);
EXPORT_SYMBOL (dma_pool_destroy);
EXPORT_SYMBOL (dma_pool_alloc);
EXPORT_SYMBOL (dma_pool_free);
EXPORT_SYMBOL (dmam_pool_create);
EXPORT_SYMBOL (dmam_pool_destroy);
#endif
