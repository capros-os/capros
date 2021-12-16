/*
 * Copyright (C) 2009, Strawberry Development Group.
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

#include <domain/assert.h>

#include <linuxk/linux-emul.h>
#include <linux/scatterlist.h>
#include <domain/assert.h>

void
sg_init_one(struct scatterlist * sg,
	const void * buf, dma_addr_t buf_dma, unsigned int buflen)
{
#ifdef CONFIG_DEBUG_SG
	sg->sg_magic = SG_MAGIC;
#endif
	sg->page_link = 0x02;	// set termination bit; compare sg_mark_end
	sg->offset = 0;
	sg->dma_address = buf_dma;
	sg->length = buflen;
}

struct scatterlist * sg_next(struct scatterlist * sg)
{
	assert(sg_is_last(sg));	// we use only single-member sgls
	return NULL;
}
