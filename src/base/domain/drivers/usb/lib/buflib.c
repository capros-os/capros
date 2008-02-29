/*
 * Copyright (C) 2008, Strawberry Development Group
 *
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

/*
 * DMA buffer management for USB drivers.
 */

//#include <linux/module.h>
#include <linux/kernel.h>
//#include <linux/slab.h>
#include <linux/device.h>
//#include <linux/mm.h>
//#include <asm/io.h>
//#include <asm/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/usb.h>
//#include "hcd.h"
#include <eros/Invoke.h>
#include <asm/USBIntf.h>

#include <domain/assert.h>
#include "usbdev.h"

#define NUM_POOLS 4

struct dma_pool * usbdevPool[NUM_POOLS];
struct device deviceForDMA;

/*
 * DMA-Coherent Buffers
 */

/* FIXME tune these based on pool statistics ... */
static const size_t	pool_max [NUM_POOLS] = {
	/* platforms without dma-friendly caches might need to
	 * prevent cacheline sharing...
	 */
#if 1
	32,
	128,
	512,
	PAGE_SIZE / 2
#else
	32,
	256,
	1024
#endif
	/* bigger --> allocate pages */
};


/* SETUP primitives */

/**
 * usbdev_buffer_create - initialize buffer pools
 *
 * Call this as part of initializing a host controller that uses the dma
 * memory allocators.  It initializes some pools of dma-coherent memory that
 * will be shared by all drivers using that controller, or returns a negative
 * errno value on error.
 *
 * Call usbdev_buffer_destroy() to clean up after using those pools.
 */
int usbdev_buffer_create(void)
{
	char		name[16];
	int 		i, size;

  uint32_t mask;
  result_t result = capros_USBInterface_getDMAMask(KR_USBINTF, &mask);
  assert(result == RC_OK);
  deviceForDMA.dma_mask = (void *)mask;

	for (i = 0; i < NUM_POOLS; i++) { 
		size = pool_max[i];
		snprintf(name, sizeof name, "buffer-%d", size);
		usbdevPool[i] = dma_pool_create(name, &deviceForDMA,
				size, size, 0);
		if (!usbdevPool [i]) {
			usbdev_buffer_destroy();
			return -ENOMEM;
		}
	}
	return 0;
}


/**
 * usbdev_buffer_destroy - deallocate buffer pools
 *
 * This frees the buffer pools created by usbdev_buffer_create().
 */
void usbdev_buffer_destroy(void)
{
	int i;

	for (i = 0; i < NUM_POOLS; i++) { 
		struct dma_pool * pool = usbdevPool[i];
		if (pool) {
			dma_pool_destroy(pool);
			usbdevPool[i] = NULL;
		}
	}
}


/**
 * usb_buffer_alloc - allocate dma-consistent buffer for URB_NO_xxx_DMA_MAP
 * @dev: device the buffer will be used with
 * @size: requested buffer size
 * @mem_flags: affect whether allocation may block
 * @dma: used to return DMA address of buffer
 *
 * Return value is either null (indicating no buffer could be allocated), or
 * the cpu-space pointer to a buffer that may be used to perform DMA to the
 * specified device.  Such cpu-space buffers are returned along with the DMA
 * address (through the pointer provided).
 *
 * These buffers are used with URB_NO_xxx_DMA_MAP set in urb->transfer_flags
 * to avoid behaviors like using "DMA bounce buffers", or tying down I/O
 * mapping hardware for long idle periods.  The implementation varies between
 * platforms, depending on details of how DMA will work to this device.
 * Using these buffers also helps prevent cacheline sharing problems on
 * architectures where CPU caches are not DMA-coherent.
 *
 * When the buffer is no longer used, free it with usb_buffer_free().
 */
void *usb_buffer_alloc(
	struct usb_device *dev,	// not used
	size_t size,
	gfp_t mem_flags,
	dma_addr_t *dma
)
{
	int i;
	for (i = 0; i < NUM_POOLS; i++) {
		if (size <= pool_max [i])
			return dma_pool_alloc(usbdevPool[i], mem_flags, dma);
	}
	return dma_alloc_coherent(&deviceForDMA, size, dma, 0);
}

/**
 * usb_buffer_free - free memory allocated with usb_buffer_alloc()
 * @dev: device the buffer was used with
 * @size: requested buffer size
 * @addr: CPU address of buffer
 * @dma: DMA address of buffer
 *
 * This reclaims an I/O buffer, letting it be reused.  The memory must have
 * been allocated using usb_buffer_alloc(), and the parameters must match
 * those provided in that allocation request. 
 */
void usb_buffer_free(
	struct usb_device *dev,	// not used
	size_t size,
	void *addr,
	dma_addr_t dma
)
{
	if (!addr)
		return;
	int i;

	for (i = 0; i < NUM_POOLS; i++) {
		if (size <= pool_max [i]) {
			dma_pool_free(usbdevPool [i], addr, dma);
			return;
		}
	}
	dma_free_coherent(&deviceForDMA, size, addr, dma);
}
