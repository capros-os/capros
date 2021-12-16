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

#include "keyring.h"

#include <domain/NetSysKey.h>
#include <domain/domdbg.h>

#include "include/pbuf.h"
#include "include/memp.h"
#include "include/mem.h"

#define DEBUGPBUF if(0)

static uint8_t pbuf_pool_memory[(PBUF_POOL_SIZE * 
				 MEM_ALIGN_SIZE(PBUF_POOL_BUFSIZE + 
						sizeof(struct pbuf)))];

static struct pbuf *pbuf_pool = NULL;

/**
 * Initializes the pbuf module.
 *
 * A large part of memory is allocated for holding the pool of pbufs.
 * The size of the individual pbufs in the pool is given by the size
 * parameter, and the number of pbufs in the pool by the num parameter.
 *
 * After the memory has been allocated, the pbufs are set up. The
 * ->next pointer in each pbuf is set up to point to the next pbuf in
 * the pool.
 */
void
pbuf_init(void)
{
  struct pbuf *p, *q = NULL;
  uint16_t i;

  pbuf_pool = (struct pbuf *)&pbuf_pool_memory[0];
  
  /* Set up ->next pointers to link the pbufs of the pool together */
  p = pbuf_pool;
  
  for(i = 0; i < PBUF_POOL_SIZE; ++i) {
    p->next = (struct pbuf *)((uint8_t *)p + 
			      PBUF_POOL_BUFSIZE + sizeof(struct pbuf));
    p->len = p->tot_len = PBUF_POOL_BUFSIZE;
    p->payload = (void *)MEM_ALIGN((void *)((uint8_t *)p + 
					    sizeof(struct pbuf)));
    q = p;
    p = p->next;
  }
  
  /* The ->next pointer of last pbuf is NULL to indicate that there
     are no more pbufs in the pool */
  q->next = NULL;
}


/**
 * @internal only called from pbuf_alloc()
 */
static struct pbuf *
pbuf_pool_alloc(void)
{
  struct pbuf *p = NULL;
  
  DEBUGPBUF kprintf(KR_OSTREAM,"----------POOL ALLOC---------------");
  p = pbuf_pool;
  if (p) {
    pbuf_pool = p->next; 
  }
  
  return p;   
}


#define PBUF_POOL_FAST_FREE(p)  do {                                    \
                                  p->next = pbuf_pool;                  \
                                  pbuf_pool = p;                        \
                                  /*DEC_PBUF_STATS;*/                   \
                                } while (0)

#define PBUF_POOL_FREE(p)  do {                                         \
                                PBUF_POOL_FAST_FREE(p);                 \
                               } while (0)

/**
 * Allocates a pbuf.
 *
 * The actual memory allocated for the pbuf is determined by the
 * layer at which the pbuf is allocated and the requested size
 * (from the size parameter).
 *
 * @param flag this parameter decides how and where the pbuf
 * should be allocated as follows:
 * 
 * - PBUF_RAM: buffer memory for pbuf is allocated as one large
 *             chunk. This includes protocol headers as well. 
 * - PBUF_ROM: no buffer memory is allocated for the pbuf, even for
 *             protocol headers. Additional headers must be prepended
 *             by allocating another pbuf and chain in to the front of
 *             the ROM pbuf. It is assumed that the memory used is really
 *             similar to ROM in that it is immutable and will not be
 *             changed. Memory which is dynamic should generally not
 *             be attached to PBUF_ROM pbufs. Use PBUF_REF instead.
 * - PBUF_REF: no buffer memory is allocated for the pbuf, even for
 *             protocol headers. It is assumed that the pbuf is only
 *             being used in a single thread. If the pbuf gets queued,
 *             then pbuf_take should be called to copy the buffer.
 * - PBUF_POOL: the pbuf is allocated as a pbuf chain, with pbufs from
 *              the pbuf pool that is allocated during pbuf_init().
 *
 * @return the allocated pbuf. If multiple pbufs where allocated, this
 * is the first pbuf of a pbuf chain. 
 */
struct pbuf *
pbuf_alloc(pbuf_layer l, uint16_t length, pbuf_flag flag)
{
  struct pbuf *p, *q, *r;
  uint16_t offset;
  int32_t rem_len; /* remaining length */

  /* determine header offset */
  offset = 0;
  switch (l) {
  case PBUF_TRANSPORT:
    /* add room for transport (often TCP) layer header */
    offset += PBUF_TRANSPORT_HLEN;
    /* FALLTHROUGH */
  case PBUF_IP:
    /* add room for IP layer header */
    offset += PBUF_IP_HLEN;
    /* FALLTHROUGH */
  case PBUF_LINK:
    /* add room for link layer header */
    offset += PBUF_LINK_HLEN;
    break;
  case PBUF_RAW:
    break;
  default:
    return NULL;
  }

  switch (flag) {
  case PBUF_POOL:
    /* allocate head of pbuf chain into p */
    p = pbuf_pool_alloc();
    if (p == NULL) return NULL;
    p->next = NULL;
    
    /* make the payload pointer point 'offset' bytes into pbuf data memory */
    p->payload = MEM_ALIGN((void *)((uint8_t *)p + 
				    (sizeof(struct pbuf) + offset)));
    /* the total length of the pbuf chain is the requested size */
    p->tot_len = length;
    /* set the length of the first pbuf in the chain */
    p->len = length > PBUF_POOL_BUFSIZE - offset? PBUF_POOL_BUFSIZE - offset: length;
    /* set pbuf type */
    p->flags = PBUF_FLAG_POOL;
    
    /* set reference count (needed here in case we fail) */
    p->ref = 1;

    /* now allocate the tail of the pbuf chain */
    
    /* remember first pbuf for linkage in next iteration */
    r = p;
    /* remaining length to be allocated */
    rem_len = length - p->len;
    /* any remaining pbufs to be allocated? */
    while(rem_len > 0) {      
      q = pbuf_pool_alloc();
      if (q == NULL) {
	/* bail out unsuccesfully */
        pbuf_free(p);
        return NULL;
      }
      q->next = NULL;
      /* make previous pbuf point to this pbuf */
      r->next = q;
      /* set length of this pbuf */
      q->len = rem_len > PBUF_POOL_BUFSIZE? PBUF_POOL_BUFSIZE: rem_len;
      q->flags = PBUF_FLAG_POOL;
      q->payload = (void *)((uint8_t *)q + sizeof(struct pbuf));
      q->ref = 1;
      /* calculate remaining length to be allocated */
      rem_len -= q->len;
      /* remember this pbuf for linkage in next iteration */
      r = q;
    }
    /* end of chain */
    //r->next = NULL;
    break;
  
  case PBUF_RAM:
    /* If pbuf is to be allocated in RAM, allocate memory for it. */
    p = mem_malloc(MEM_ALIGN_SIZE(sizeof(struct pbuf) + length + offset));
    if (p == NULL) {
      return NULL;
    }
    DEBUGPBUF
      kprintf(KR_OSTREAM,"***********PBUF_RAM Allocated************%x**",p);

    /* Set up internal structure of the pbuf. */
    p->payload = MEM_ALIGN((void *)((uint8_t *)p + 
				    sizeof(struct pbuf) + offset));
    p->len = p->tot_len = length;
    p->next = NULL;
    p->flags = PBUF_FLAG_RAM;
    break;
  
    /* pbuf references existing (static constant) ROM payload? */
  case PBUF_ROM:
    
    /* pbuf references existing (externally allocated) RAM payload? */
  case PBUF_REF:
    /* only allocate memory for the pbuf structure */
    p = memp_alloc(MEMP_PBUF);
    if (p == NULL)  return NULL;
    /* caller must set this field properly, afterwards */
    p->payload = NULL;
    p->len = p->tot_len = length;
    p->next = NULL;
    p->flags = (flag == PBUF_ROM? PBUF_FLAG_ROM: PBUF_FLAG_REF);
    break;

  default:
    return NULL;
  }
  p->ref = 1;
  return p;
}


/**
 * Shrink a pbuf chain to a desired length.
 *
 * @param p pbuf to shrink.
 * @param new_len desired new length of pbuf chain
 *
 * Depending on the desired length, the first few pbufs in a chain might
 * be skipped and left unchanged. The new last pbuf in the chain will be
 * resized, and any remaining pbufs will be freed.
 *
 * @note If the pbuf is ROM/REF, only the 
 * ->tot_len and ->len fields are adjusted.
 * @note May not be called on a packet queue.
 *
 * @bug Cannot grow the size of a pbuf (chain) (yet).
 */
void
pbuf_realloc(struct pbuf *p, uint16_t new_len)
{
  struct pbuf *q;
  uint16_t rem_len; /* remaining length */
  int16_t grow;
  
  /*FIX: Assert to be put later on.
    ERIP_ASSERT("pbuf_realloc: sane p->flags", p->flags == PBUF_FLAG_POOL ||
    p->flags == PBUF_FLAG_ROM ||
    p->flags == PBUF_FLAG_RAM ||
    p->flags == PBUF_FLAG_REF);
  */
  /* desired length larger than current length? */
  if (new_len >= p->tot_len) {
    /* enlarging not yet supported */
    return;
  }
  
  /* the pbuf chain grows by (new_len - p->tot_len) bytes
   * (which may be negative in case of shrinking) */
  grow = new_len - p->tot_len;

  /* first, step over any pbufs that should remain in the chain */
  rem_len = new_len;
  q = p;
  /* this pbuf should be kept? */
  while (rem_len > q->len) {
    /* decrease remaining length by pbuf length */
        rem_len -= q->len;
    /* decrease total length indicator */
    q->tot_len += grow;
    /* proceed to next pbuf in chain */
    q = q->next;
  }
  /* we have now reached the new last pbuf (in q) */
  /* rem_len == desired length for pbuf q */
  
  /* shrink allocated memory for PBUF_RAM */
  /* (other types merely adjust their length fields */
  if ((q->flags == PBUF_FLAG_RAM) && (rem_len != q->len)) {
    /* reallocate and adjust the length of the pbuf that will be split */
    mem_realloc(q, (uint8_t *)q->payload - (uint8_t *)q + rem_len);
  }
  /* adjust length fields for new last pbuf */
  q->len = rem_len;
  q->tot_len = q->len;
  
  /* any remaining pbufs in chain? */
  if (q->next != NULL) {
    /* free remaining pbufs in chain */
    pbuf_free(q->next);
  }
  /* q is last packet in chain */
  q->next = NULL;
}


/**
 * Adjusts the payload pointer to hide or reveal headers in the payload.
 * 
 * Adjusts the ->payload pointer so that space for a header
 * (dis)appears in the pbuf payload.
 *
 * The ->payload, ->tot_len and ->len fields are adjusted.
 *
 * @param hdr_size Number of bytes to increment header size which
 * increases the size of the pbuf. New space is on the front.
 * (Using a negative value decreases the header size.)
 *
 * PBUF_ROM and PBUF_REF type buffers cannot have their sizes increased, so
 * the call will fail. A check is made that the increase in header size does
 * not move the payload pointer in front of the start of the buffer. 
 * @return 1 on failure, 0 on success.
 *
 * @note May not be called on a packet queue.
 */
uint8_t
pbuf_header(struct pbuf *p, int16_t header_size)
{
  void *payload;

  /* remember current payload pointer */
  payload = p->payload;

  /* pbuf types containing payloads? */
  switch (p->flags) {
  case (PBUF_FLAG_RAM) :
  case (PBUF_FLAG_POOL): {
    /* set new payload pointer */
    p->payload = (uint8_t *)p->payload - header_size;
    /* boundary check fails? */
    if ((uint8_t *)p->payload < (uint8_t *)p + sizeof(struct pbuf)) {
      /* restore old payload pointer */
      p->payload = payload;
      /* bail out unsuccesfully */
      return 1;
    }
    break;
  }
    /* pbuf types refering to payloads? */
  case(PBUF_FLAG_REF) :
  case (PBUF_FLAG_ROM): {
    /* hide a header in the payload? */
    if ((header_size < 0) && (header_size - p->len <= 0)) {
      /* increase payload pointer */
      p->payload = (uint8_t *)p->payload - header_size;
    } else {
      /* cannot expand payload to front (yet!)
       * bail out unsuccesfully */
      return 1;
    }
    break;
  }
  }
  /* modify pbuf length fields */
  p->len += header_size;
  p->tot_len += header_size;

  return 0;
}


/**
 * Free a pbuf (chain) from usage, de-allocate non-used head of chain.
 *
 * Decrements the pbuf reference count. If it reaches
 * zero, the pbuf is deallocated.
 *
 * For a pbuf chain, this is repeated for each pbuf in the chain, until
 * a non-zero reference count is encountered, or the end of the chain is
 * reached. 
 *
 * @param pbuf pbuf (chain) to be freed from one user.
 *
 * @return the number of unreferenced pbufs that were de-allocated 
 * from the head of the chain.
 *
 * @note May not be called on a packet queue.
 * @note the reference counter of a pbuf equals the number of pointers
 * that refer to the pbuf (or into the pbuf).
 *
 * @internal examples:
 *
 * 1->2->3 becomes ...1->3
 * 3->3->3 becomes 2->3->3
 * 1->1->2 becomes ....->1
 * 2->1->1 becomes 1->1->1
 * 1->1->1 becomes .......
 * 
 */ 
uint8_t
pbuf_free(struct pbuf *p)
{
  struct pbuf *q;
  uint8_t count;

  if (p == NULL)  return 0;
  
  count = 0;
  /* Since decrementing ref cannot be guaranteed to be a single 
   * machine operation we must protect it. Also, the later test of 
   * ref must be protected.
   */
  
  /* de-allocate all consecutive pbufs from the head of the chain that
   * obtain a zero reference count after decrementing*/
  while (p != NULL) {
    /* decrease reference count (number of pointers to pbuf) */
    p->ref--;
    /* this pbuf is no longer referenced to? */
    if (p->ref == 0) {
      /* remember next pbuf in chain for next iteration */
      q = p->next;
      /* is this a pbuf from the pool? */
      switch(p->flags) {
      case (PBUF_FLAG_POOL): {
	  p->len = p->tot_len = PBUF_POOL_BUFSIZE;
	  p->payload = (void *)((uint8_t *)p + sizeof(struct pbuf));
	  DEBUGPBUF kprintf(KR_OSTREAM,"----------POOL FREE---------------");
	  PBUF_POOL_FREE(p);
	  /* a ROM or RAM referencing pbuf */
	  break;
	}
      case (PBUF_FLAG_ROM) :
      case (PBUF_FLAG_REF) : {
	memp_free(MEMP_PBUF, p);
	/* p->flags == PBUF_FLAG_RAM */
	break;
      } 
      default: {
	DEBUGPBUF
	  kprintf(KR_OSTREAM,"***********PBUF_RAM Free**********%x****",p);
        mem_free(p);
	break;
      }
      }
      count++;
      /* proceed to next pbuf */
      p = q;
      /* p->ref > 0, this pbuf is still referenced to */
      /* (and so the remaining pbufs in chain as well) */
    } else {
      /* stop walking through chain */
      p = NULL;
    }
  }
 
  /* return number of de-allocated pbufs */
  return count;
}

/* Chain two pbufs (or pbuf chains) together. 
 * They must belong to the same packet.
 *
 * @param h head pbuf (chain)
 * @param t tail pbuf (chain)
 * @note May not be called on a packet queue.
 * 
 * The ->tot_len fields of all pbufs of the head chain are adjusted.
 * The ->next field of the last pbuf of the head chain is adjusted.
 * The ->ref field of the first pbuf of the tail chain is adjusted.
 */
void
pbuf_chain(struct pbuf *h, struct pbuf *t)
{
  struct pbuf *p;

  if (t == NULL)     return;
  
  /* proceed to last pbuf of chain */
  for (p = h; p->next != NULL; p = p->next) {
    /* add total length of second chain to all totals of first chain */
    p->tot_len += t->tot_len;
  }
  /* p is last pbuf of first h chain */
  
  /* add total length of second chain to last pbuf total of first chain */
  p->tot_len += t->tot_len;
  
  /* chain last pbuf of h chain (p) with first of tail (t) */
  p->next = t;
  
  /* t is now referenced to one more time */
  pbuf_ref(t);
}

/* Increment the reference count of the pbuf.
 * @param p pbuf to increase reference counter of
 */
void
pbuf_ref(struct pbuf *p)
{
  /* pbuf given? */  
  if (p != NULL) {
    ++(p->ref);
  }
}


/* Count number of pbufs in a chain
 * @param p first pbuf of chain
 * @return the number of pbufs in a chain
 */
uint8_t
pbuf_clen(struct pbuf *p)
{
  uint8_t len;

  len = 0;  
  while (p != NULL) {
    ++len;
    p = p->next;
  }
  return len;
}
