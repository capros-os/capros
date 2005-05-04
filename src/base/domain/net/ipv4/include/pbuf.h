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
#ifndef _PBUF_H_
#define _PBUF_H_

#include <eros/endian.h>

#define PBUF_TRANSPORT_HLEN 20
#define PBUF_IP_HLEN        20
#define PBUF_LINK_HLEN      14

#define PBUF_POOL_SIZE      100
#define PBUF_POOL_BUFSIZE   9100 

typedef enum {
  PBUF_TRANSPORT,
  PBUF_IP,
  PBUF_LINK,
  PBUF_RAW
} pbuf_layer;

typedef enum {
  PBUF_RAM,
  PBUF_ROM,
  PBUF_REF,
  PBUF_POOL
} pbuf_flag;


/* Definitions for the pbuf flag field (these are not the flags that
 * are passed to pbuf_alloc ). */
#define PBUF_FLAG_RAM   0x00u    /* Flags that pbuf data is stored in RAM. */
#define PBUF_FLAG_ROM   0x01u    /* Flags that pbuf data is stored in ROM. */
#define PBUF_FLAG_POOL  0x02u    /* Flags that the pbuf comes from the
				    pbuf pool. */
#define PBUF_FLAG_REF   0x04U    /* Flags that pbuf payload refers to RAM */

struct pbuf {
  struct pbuf* next;
  void *payload;     /* Payload starts from here */
  uint16_t tot_len;  /* Total length of buffer+additionally chained buffers */
  uint16_t len;      /* Length of this buffer. */
  uint16_t flags;    /* Flags telling the type of pbuf */
  uint16_t ref;      /* The ref count always equals the number of pointer
                        into this buffer. This can be pointers from an
                        application, the stack itself or pbuf->next
                        pointers from a chain */
};


/* Initializes the pbuf module. The num parameter determines how many
 * pbufs that should be allocated to the pbuf pool, and the size
 * parameter specifies the size of the data allocated to those.  */
void pbuf_init(void);

/* Allocates a pbuf at protocol layer l. The actual memory allocated
 * for the pbuf is determined by the layer at which the pbuf is
 * allocated and the requested size (from the size parameter). The
 * flag parameter decides how and where the pbuf should be allocated
 * as follows:
 
 * PBUF_RAM: buffer memory for pbuf is allocated as one large
             chunk. This includesprotocol headers as well.
 * RBUF_ROM: no buffer memory is allocated for the pbuf, even for
             protocol headers.  Additional headers must be
             prepended by allocating another pbuf and chain in to
	     the front of the ROM pbuf.
 * PBUF_ROOL: the pbuf is allocated as a pbuf chain, with pbufs from
             the pbuf pool that is allocated during pbuf_init().  */
struct pbuf *pbuf_alloc(pbuf_layer l, uint16_t size, pbuf_flag flag);

/* Shrinks the pbuf to the size given by the size parameter */
void pbuf_realloc(struct pbuf *p, uint16_t size); 

/* Tries to move the p->payload pointer header_size number of bytes
 * upward within the pbuf. The return value is non-zero if it
 * fails. If so, an additional pbuf should be allocated for the header
 * and it should be chained to the front. */
uint8_t pbuf_header(struct pbuf *p,short header_size);

/* Increments the reference count of the pbuf p */
void pbuf_ref(struct pbuf *p);

/* Decrements the reference count and deallocates the pbuf if the
 * reference count is zero. If the pbuf is a chain all pbufs in the
 * chain are deallocated.  */
uint8_t pbuf_free(struct pbuf *p);

/* Returns the length of the pbuf chain. */
uint8_t pbuf_clen(struct pbuf *p);  

/* Chains pbuf t on the end of pbuf h. Pbuf h will have it's tot_len
 * field adjusted accordingly. Pbuf t should no be used any more after
 * a call to this function, since pbuf t is now a part of pbuf h.  */
void pbuf_chain(struct pbuf *h, struct pbuf *t);

/* Picks off the first pbuf from the pbuf chain p. Returns the tail of
 * the pbuf chain or NULL if the pbuf p was not chained. */
struct pbuf *pbuf_dechain(struct pbuf *p);

#endif /*_PBUF_H_*/
