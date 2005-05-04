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
#ifndef _PSTORE_H_
#define _PSTORE_H_

#include "../mapping_table.h"
#include <eros/endian.h>

#define PSTORE_TRANSPORT_HLEN 20
#define PSTORE_IP_HLEN        20
#define PSTORE_LINK_HLEN      14

#define PSTORE_POOL_SIZE      200
#define PSTORE_POOL_BUFSIZE   9000

typedef enum {
  PSTORE_TRANSPORT,
  PSTORE_IP,
  PSTORE_LINK,
  PSTORE_RAW
} pstore_layer;

typedef enum {
  PSTORE_RAM,
  PSTORE_ROM,
  PSTORE_REF,
  PSTORE_POOL
} pstore_flag;


/* Definitions for the pstore flag field (these are not the flags that
 * are passed to pstore_alloc ). */
#define PSTORE_FLAG_RAM   0x00u /* Flags that pstore data is stored in RAM. */
#define PSTORE_FLAG_ROM   0x01u /* Flags that pstore data is stored in ROM. */
#define PSTORE_FLAG_POOL  0x02u /* Flags that the pstore comes from the
				   pstore pool. */
#define PSTORE_FLAG_REF   0x04U /* Flags that pstore payload refers to RAM */

#define PSTORE_UNUSED     0x0u /* buffer used up*/
#define PSTORE_USED       0x1u /* buffer unused */
#define PSTORE_READY      0x4u /* buffer ready for action */

struct pstore {
  int8_t  nextsector; /* The next pstores sector */
  int32_t nextoffset; /* The next pstores offset */
  int8_t  sector;     /* Payload starts from here */
  int32_t offset;     /* Pointer = Sector:Offset */
  uint16_t tot_len;   /* Total length of buffer+additionally chained buffers */
  uint16_t len;       /* Length of this buffer. */
  uint16_t flags;     /* Flags telling the type of pstore */
  uint8_t  status;    /* Flags telling if buffer is used up */
  int      ref;       /* The ref count always equals the number of pointer
		       * into this buffer. This can be pointers from an
		       * application, the stack itself or pstore->next
		       * pointers from a chain */
};


/* Initializes the pstore module. The num parameter determines how many
 * pstores that should be allocated to the pstore pool, and the size
 * parameter specifies the size of the data allocated to those.  */
void pstore_init(sector s,uint32_t start_address);

/* Allocates a pstore at protocol layer l. The actual memory allocated
 * for the pstore is determined by the layer at which the pstore is
 * allocated and the requested size (from the size parameter). The
 * flag parameter decides how and where the pstore should be allocated
 * as follows:
 
 * PSTORE_RAM: buffer memory for pstore is allocated as one large
             chunk. This includesprotocol headers as well.
 * RBUF_ROM: no buffer memory is allocated for the pstore, even for
             protocol headers.  Additional headers must be
             prepended by allocating another pstore and chain in to
	     the front of the ROM pstore.
 * PSTORE_ROOL: the pstore is allocated as a pstore chain, with pstores from
             the pstore pool that is allocated during pstore_init().  */
struct pstore * pstore_alloc(pstore_layer l, uint16_t length, 
			     sector s, int ssid);
/* Shrinks the pstore to the size given by the size parameter */
void pstore_realloc(struct pstore *p, uint16_t size,int ssid); 

/* Tries to move the p->payload pointer header_size number of bytes
 * upward within the pstore. The return value is non-zero if it
 * fails. If so, an additional pstore should be allocated for the header
 * and it should be chained to the front. */
uint8_t pstore_header(struct pstore *p,short header_size,int ssid);

/* Increments the reference count of the pstore p */
void pstore_ref(struct pstore *p,int ssid);

/* Decrements the reference count and deallocates the pstore if the
 * reference count is zero. If the pstore is a chain all pstores in the
 * chain are deallocated.  */
uint32_t pstore_free(struct pstore *p,int ssid);

/* Returns the length of the pstore chain. */
uint8_t pstore_clen(struct pstore *p,int ssid);  

/* Chains pstore t on the end of pstore h. Pstore h will have it's tot_len
 * field adjusted accordingly. Pstore t should no be used any more after
 * a call to this function, since pstore t is now a part of pstore h.  */
void pstore_chain(struct pstore *h, struct pstore *t,int ssid);

void pstore_chain_no_ref(struct pstore *h, struct pstore *t,int ssid);
/* Picks off the first pstore from the pstore chain p. Returns the tail of
 * the pstore chain or NULL if the pstore p was not chained. */
struct pstore *pstore_dechain(struct pstore *p);

struct pstore * 
copy_data_in(void *h, int hdr_len, sector s1,
	     void *t, int pay_len, sector s2,
	     int ssid);

#define PSTORE_ALIGNMENT   2 
#define PSTORE_ALIGN_SIZE(size) (size + \
                             ((((size) % PSTORE_ALIGNMENT) == 0)? 0 : \
                             (PSTORE_ALIGNMENT - ((size) % PSTORE_ALIGNMENT))))
#define PSTORE_MEM_ALIGN(addr) PSTORE_ALIGN_SIZE((uint32_t)addr)
#define PSTORE_PAYLOAD(p,ssid) \
(void *)(p->offset+ActiveSessions[ssid].mt[p->sector].start_address)

#define PSTORE_NEXT(p,ssid) \
(void *)(p->nextoffset+ActiveSessions[ssid].mt[p->nextsector].start_address)

#endif /*_PSTORE_H_*/
