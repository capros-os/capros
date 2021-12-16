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

/** This is a pstore_store on the client's dime. So when the client contacts
 * us for creating a new session, we create one sub space using the client's
 * bank use it as a pstore_bank for that process.
 */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <domain/NetSysKey.h>
#include <domain/domdbg.h>

#include "netsyskeys.h"
#include "mapping_table.h"
#include "include/pstore.h"
#include "Session.h"

extern struct session ActiveSessions[MAX_SESSIONS];

#define DEBUG_PSTORE if(0)

/* Data Structure::: A Ring buffer size = PSTORE_POOL_SIZE
 * 1.  We have a current pointer if we need to allocate the next buffer 
 *     in the ring.
 * 2.  The pointer is always maintained between 0 - PSTORE_POOL_BUFSIZE
 */

/** Initialize the pstore. Each sector is divided into pstore rings. 
 * The pstore rings have a memory buffer akin to the pstore_pool implementation
 * in lwip.
 */
void 
pstore_init(sector s, uint32_t start_addr) 
{
  int i;
  struct pstore *p;
  
  DEBUG_PSTORE kprintf(KR_OSTREAM,"pstore_init: for %08x",start_addr);

  for(i=0;i<PSTORE_POOL_SIZE;i++) {
    p = (void *)start_addr + i*(PSTORE_POOL_BUFSIZE+sizeof(struct pstore));
    p->len = p->tot_len = PSTORE_POOL_BUFSIZE;
    p->sector = s;
    p->offset = (uint32_t)&p[0] + sizeof(struct pstore) - start_addr;
    p->nextsector = -1;
    p->nextoffset = -1;
    p->status = PSTORE_UNUSED;
    p->ref = 0;
  }
}

struct pstore *
pstore_alloc(pstore_layer l, uint16_t length,sector s,
	     int ssid)
{
  uint32_t offset;
  int32_t  rem_len;
  struct pstore *p = NULL,*r = NULL,*q;
  uint32_t start_address = ActiveSessions[ssid].mt[s].start_address;
  int *cur_p = &ActiveSessions[ssid].mt[s].cur_p;
  int32_t alloc_curp;
  
  /* determine header offset */
  offset = 0;
  switch (l) {
  case PSTORE_TRANSPORT:
    /* add room for transport (often TCP) layer header */
    offset += PSTORE_TRANSPORT_HLEN;
    /* FALLTHROUGH */
  case PSTORE_IP:
    /* add room for IP layer header */
    offset += PSTORE_IP_HLEN;
    /* FALLTHROUGH */
  case PSTORE_LINK:
    /* add room for link layer header */
    offset += PSTORE_LINK_HLEN;
    break;
  case PSTORE_RAW:
    break;
  default:
    return NULL;
  }

  /* The next empty buffer in the ring */
  p = (void *)(start_address + 
	       (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));

  /* If we are successful, remember to reset cur_p */
  alloc_curp = *cur_p;

  /* Now allocate the head of the buffer aggregate */
  if(p->status != PSTORE_UNUSED) {
    kdprintf(KR_OSTREAM,"alloc:ssid=%d,sect=%d,cur_p=%d stat=%d add=%08x used",
	     ssid,s,*cur_p,p->status,&p[0]);
    return NULL;
  }
  
  /* Set status flag to used */
  p->status = PSTORE_USED;
  p->tot_len = length;
  p->len = length > PSTORE_POOL_BUFSIZE - offset ? 
    PSTORE_POOL_BUFSIZE - offset : length;
  p->nextsector = -1;
  p->nextoffset = -1;
  p->offset = (uint32_t)&p[0] - start_address + sizeof(struct pstore) 
    + offset;
  p->sector = s;
  p->ref = 1;
  
  /* Increment the ring buffer pointer */
  *cur_p = *cur_p + 1 >=  PSTORE_POOL_SIZE ? 0: *cur_p + 1;
  
  /* remember for linkage */
  r = p;
  rem_len = length - p->len;

  /* Now loop until we have allocated the tail of the ring buffer */
  while(rem_len > 0) {
    /* The next empty buffer in the ring */
    q = (void *)(start_address + 
		 (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));
    
    if(q->status != PSTORE_UNUSED) {
      kdprintf(KR_OSTREAM,"alloc:ssid=%d,sect=%d,cur_p=%d,remlen=%d used?",
	       ssid,s,*cur_p,rem_len);
      /* Reset the cur_p pointer to initialize location */
      *cur_p = alloc_curp;
      pstore_free(p,ssid);/* Free already allocated rings */
      return NULL;
    }
    
    /* Change the flag of the particular ring buffer */
    q->status = PSTORE_USED;
    
    /* Form the chain */
    r->nextsector = s;
    r->nextoffset = (uint32_t)&q[0] - start_address;
    
    /* Fill in info for q */
    q->sector = s;
    q->tot_len = rem_len;
    q->len = rem_len > PSTORE_POOL_BUFSIZE?PSTORE_POOL_BUFSIZE:rem_len;
    q->offset = (uint32_t)&q[0] - start_address + sizeof(struct pstore);
    
    /* Next pointer is for now earthed */
    q->nextsector = -1;
    q->nextoffset = -1;
    
    /* Increase the reference for this pointer */
    q->ref = 1;
    /* update remaining length to be allocated */
    rem_len -= PSTORE_POOL_BUFSIZE;
    
    /* Remember pstore for linkage */
    r = q;

    /* Ring buffer current pointer increment */
    *cur_p = *cur_p + 1 >= PSTORE_POOL_SIZE ? 0 : *cur_p + 1;
  }

  DEBUG_PSTORE kprintf(KR_OSTREAM,"allocated pstore at %08x",&p[0]);
  
  return p;
}


/* Chain two pstores (or pstore chains) together. 
 * They must belong to the same packet.
 *
 * @param h head pstore (chain)
 * @param t tail pstore (chain)
 * @note May not be called on a packet queue.
 * 
 * The ->tot_len fields of all pstores of the head chain are adjusted.
 * The ->next field of the last pstore of the head chain is adjusted.
 * The ->ref field of the first pstore of the tail chain is adjusted.
 */
void 
pstore_chain(struct pstore *h, struct pstore *t,int ssid)
{
  struct pstore *q;
  uint32_t next_pstore = 0;
  struct mapping_table *mt=ActiveSessions[ssid].mt;

  if(&h[0] == 0) return;
  
  DEBUG_PSTORE 
    kprintf(KR_OSTREAM,"pstore_chain %d %d",h->tot_len,t->tot_len);
  q = h;
  /* proceed to last store of chain */
  while(q->nextoffset!=-1) {
    next_pstore = q->nextoffset + mt[q->nextsector].start_address;
    q->tot_len = t->tot_len;
    q = (void *)next_pstore;
  }
  q->tot_len += t->tot_len;
  q->nextsector = t->sector;
  q->nextoffset = (uint32_t)&t[0] - mt[t->sector].start_address;
  pstore_ref(t,ssid);
}

/* Run through the pstore chain freeing up all the pstores. For absolute
 * addresses consult the mapping table */
uint32_t 
pstore_free(struct pstore *p,int ssid)
{
  struct pstore *q = p;
  int32_t nextoff;
  struct mapping_table *mt=ActiveSessions[ssid].mt;

  if(&p[0] == NULL) return RC_OK;

  DEBUG_PSTORE {
    static int i;
    i++;
    kprintf(KR_OSTREAM,"(%d) pstore_freeing %08x %d ref=%d",
	    i,&p[0],ssid,p->ref);
  }
  
  do {
    DEBUG_PSTORE kprintf(KR_OSTREAM,"Freeing %08x %d",&q[0],q->ref);
    if(q->ref<=0) return RC_OK;
    nextoff = q->nextoffset;
    q->ref --;
    if(q->ref == 0) {
      q->status = PSTORE_UNUSED;
    }else {
      //DEBUG_PSTORE
	kprintf(KR_OSTREAM,"Someone reffed u eh? %08x %d",&q[0],q->ref);
      return RC_OK;
    }
    q = (void *)(nextoff + mt[q->nextsector].start_address);
  }while(nextoff!=-1);
  
  return RC_OK;
}


/**
 * Adjusts the payload pointer to hide or reveal headers in the payload.
 * 
 * Adjusts the ->payload pointer so that space for a header
 * (dis)appears in the pstore payload.
 *
 * The ->payload, ->tot_len and ->len fields are adjusted.
 *
 * @param hdr_size Number of bytes to increment header size which
 * increases the size of the pstore. New space is on the front.
 * (Using a negative value decreases the header size.)
 *
 * PSTORE_ROM and PSTORE_REF type buffers cannot have their sizes increased, so
 * the call will fail. A check is made that the increase in header size does
 * not move the payload pointer in front of the start of the buffer. 
 * @return 1 on failure, 0 on success.
 *
 * @note May not be called on a packet queue.
 */
uint8_t
pstore_header(struct pstore *p, int16_t header_size,int ssid)
{
  uint32_t offset = p->offset;
  struct mapping_table *mt=ActiveSessions[ssid].mt;
  sector s = p->sector;

  if(&p[0] == 0) return RC_OK;
  
  /* set new offset pointer */
  p->offset -= header_size;
  
  /* boundary check fails? */
  if (p->offset < (uint32_t)&p[0]-mt[s].start_address+sizeof(struct pstore)) {
    /* restore old payload pointer */
    p->offset = offset;
    /* bail out unsuccesfully */
    return 1;
  }

  /* modify pstore length fields */
  p->len += header_size;
  p->tot_len += header_size;

  return RC_OK;
}


/* Increment the reference count of the pstore.
 * @param p pstore to increase reference counter of
 */
void
pstore_ref(struct pstore *p,int ssid)
{
  DEBUG_PSTORE kprintf(KR_OSTREAM,"pstore_ref");

  if(&p[0] == 0) return;
  ++(p->ref);
}

/**
 * Shrink a pstore chain to a desired length.
 *
 * @param p pstore to shrink.
 * @param new_len desired new length of pstore chain
 *
 * Depending on the desired length, the first few pstores in a chain might
 * be skipped and left unchanged. The new last pstore in the chain will be
 * resized, and any remaining pstores will be freed.
 *
 * @note If the pstore is ROM/REF, only the 
 * ->tot_len and ->len fields are adjusted.
 * @note May not be called on a packet queue.
 *
 * @bug Cannot grow the size of a pstore (chain) (yet).
 */
void
pstore_realloc(struct pstore *p, uint16_t new_len,int ssid)
{
  struct pstore *q;
  uint32_t rem_len; /* remaining length */
  int32_t grow;

  if(&p[0] == 0) return;
  DEBUG_PSTORE
    kprintf(KR_OSTREAM,"pstore_realloc old_len = %dnew_len=%d len = %dssid=%d",
	    p->tot_len,new_len,p->len,ssid);
  
  if(new_len == p->tot_len) return;

  /* desired length larger than current length? */
  if (new_len > p->tot_len) {
    /* enlarging not yet supported */
    DEBUG_PSTORE 
      kprintf(KR_OSTREAM,"pbuf:alloc enlarging not yet supported %d->%d",
	      p->tot_len,new_len);
    return;
  }
  
  /* the pstore chain grows by (new_len - p->tot_len) bytes
   * (which may be negative in case of shrinking) */
  grow = new_len - p->tot_len;
  /* first, step over any pstores that should remain in the chain */
  rem_len = new_len;
  q = p;
  /* this pstore should be kept? */
  while (rem_len > q->len) {
    /* decrease remaining length by pstore length */
    rem_len -= q->len;
    /* decrease total length indicator */
    q->tot_len += grow;
    /* proceed to next pstore in chain */
    q = PSTORE_NEXT(q,ssid);
  }
  /* we have now reached the new last pstore (in q) */
  /* rem_len == desired length for pstore q */
  /* shrink allocated memory for PSTORE_RAM */
  /* (other types merely adjust their length fields */

  /* adjust length fields for new last pstore */
  q->len = rem_len;
  q->tot_len = q->len;
  
  DEBUG_PSTORE kprintf(KR_OSTREAM,"q->nextoffset = %d",q->nextoffset);
  
  /* any remaining pstores in chain? */
  if (q->nextoffset != -1) {
    /* free remaining pstores in chain */
    pstore_free(PSTORE_NEXT(q,ssid),ssid);
    q->nextoffset = -1;
    q->nextsector = -1;
  }
}

/* Count number of pstores in a chain
 * @param p first pbuf of chain
 * @return the number of pstores in a chain
 */
uint8_t
pstore_clen(struct pstore *p,int ssid)
{
  uint8_t len = 0;
  int32_t nextoff;

  if(&p[0] == 0) return 0;
  /* Run along the chain increasing the pref */
  do {
    nextoff = p->nextoffset;
    p = PSTORE_NEXT(p,ssid);
    len ++;
  }while(nextoff!=-1);
  
  return len;
}


/* Pure pstore chaining Dont ref the tail */
void 
pstore_chain_no_ref(struct pstore *h, struct pstore *t,int ssid)
{
  struct pstore *q;
  struct mapping_table *mt=ActiveSessions[ssid].mt;

  if(&h[0] == 0) return;
  DEBUG_PSTORE kprintf(KR_OSTREAM,"pstore_chain_!ref %d %d",
		       h->tot_len,t->tot_len);
  q = h;
  /* proceed to last store of chain */
  while(q->nextoffset!=-1) {
    q->tot_len = t->tot_len;
    q = (void *)(q->nextoffset + mt[q->nextsector].start_address);
  }
  q->tot_len += t->tot_len;
  q->nextsector = t->sector;
  q->nextoffset = (uint32_t)&t[0] - mt[t->sector].start_address;
}

/* Returns an allocated chained pstore with the data in it */
struct pstore * 
copy_data_in(void *h, int hdr_len, sector s1,
	     void *t, int pay_len, sector s2,
	     int ssid) 
{
  int32_t  rem_len;
  struct pstore *p = NULL,*r = NULL,*q;
  uint32_t start_address = ActiveSessions[ssid].mt[s1].start_address;
  int *cur_p = &ActiveSessions[ssid].mt[s1].cur_p;
  int alloc_curp1,alloc_curp2 = ActiveSessions[ssid].mt[s2].cur_p;
  struct pstore *stack_p,*pay_p;
  int totlen = 0;
  char *s;
  int32_t nextoff;

  /* Remember the current ring pointer */
  alloc_curp1 = *cur_p;
  
  /* The next empty buffer in the ring */
  p = (void *)(start_address + 
	       (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));

  /* Now allocate the head of the buffer aggregate */
  if(p->status != PSTORE_UNUSED) {
    kdprintf(KR_OSTREAM,"alloc:ssid=%d,sect=%d,cur_p=%d stat=%d add=%08x used",
	     ssid,s1,*cur_p,p->status,&p[0]);
    return NULL;
  }
  
  p->status = PSTORE_USED;
  p->tot_len = hdr_len;
  p->len = hdr_len > PSTORE_POOL_BUFSIZE ? PSTORE_POOL_BUFSIZE : hdr_len;
  p->nextsector = -1;
  p->nextoffset = -1;
  p->offset = (uint32_t)&p[0] - start_address + sizeof(struct pstore);
  p->sector = s1;
  p->ref = 1;
  
  /* Increment the ring buffer pointer */
  *cur_p = *cur_p + 1 >= PSTORE_POOL_SIZE ? 0 : *cur_p + 1;
  
  /* remember for linkage */
  r = p;
  stack_p = p;

  rem_len = hdr_len - p->len;

  DEBUG_PSTORE
    kprintf(KR_OSTREAM,"demux hdr pstore allocated at %08x %d",
	    &stack_p[0],stack_p->tot_len);
  
  /* Do a copy of the hdr from the hw ring buffer into this pstore */
  q = stack_p; 
  do{
    nextoff = q->nextoffset;
    /* Read enough bytes to fill this pbuf in the chain. The
       available data in the pbuf is given by the q->len
       variable. */
    s = (void *)(start_address + q->offset);
    memcpy(&s[0],&((char *)h)[totlen],q->len);
    totlen += q->len;
    q = (void*)(start_address + nextoff);
  }while (totlen < hdr_len );

  if(t!=NULL && pay_len > 0) {
    cur_p = &ActiveSessions[ssid].mt[s2].cur_p;
    start_address = ActiveSessions[ssid].mt[s2].start_address;
    
    /* The next empty buffer in the ring */
    p = (void *)(start_address + 
		 (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));
    
    /* Now allocate the head of the buffer aggregate */
    if(p->status != PSTORE_UNUSED) {
      kdprintf(KR_OSTREAM,
	       "alloc:ssid=%d,sect=%d,cur_p=%d stat=%d add=%08x used",
	       ssid,s2,*cur_p,p->status,&p[0]);
      /* Free already allocated rings */
      pstore_free(stack_p,ssid);
      ActiveSessions[ssid].mt[s1].cur_p = alloc_curp1;
      return NULL;
    }
    
    p->status = PSTORE_USED;
    p->tot_len = pay_len;
    p->len = pay_len > PSTORE_POOL_BUFSIZE ? PSTORE_POOL_BUFSIZE : pay_len;
    p->nextsector = -1;
    p->nextoffset = -1;
    p->offset = (uint32_t)&p[0] - start_address + sizeof(struct pstore);
    p->sector = s2;
    p->ref = 1;
    
    /* Increment the ring buffer pointer */
    *cur_p = *cur_p + 1 >= PSTORE_POOL_SIZE ?0:*cur_p + 1;
    
    /* remember for linkage */
    r = p;
    pay_p = p;
    rem_len = pay_len - p->len;

    q = p;
    /* Now loop until we have allocated the tail of the ring buffer */
    while(rem_len > 0) {
      /* Our next buffer to look at */
      if(0 == *cur_p) q = (void *)start_address;
      else q = (void*)((uint32_t)&q[0] + PSTORE_POOL_BUFSIZE + 
		       sizeof(struct pstore));
            
      if(q->status != PSTORE_UNUSED) {
	kdprintf(KR_OSTREAM,"alloc:ssid=%d,sect=%d,cur_p=%d,remlen=%d used?",
		 ssid,s,*cur_p,rem_len);
	*cur_p = alloc_curp2;
	ActiveSessions[ssid].mt[s1].cur_p = alloc_curp1;
	
	/* Free already allocated rings */
	pstore_free(stack_p,ssid);
	pstore_free(p,ssid);

	return NULL;
      }
      
      /* Form the chain */
      r->nextsector = s2;
      r->nextoffset = (uint32_t)&q[0] - start_address;
      
      /* Fill in info for q */
      q->status = PSTORE_USED;
      q->sector = s2;
      q->tot_len = rem_len;
      q->len = rem_len > PSTORE_POOL_BUFSIZE?PSTORE_POOL_BUFSIZE:rem_len;
      q->offset = (uint32_t)&q[0] - start_address + sizeof(struct pstore);
      
      /* Next pointer is for now earthed */
      q->nextsector = -1;
      q->nextoffset = -1;
      
      /* Increase the reference for this pointer */
      q->ref = 1;
      /* update remaining length to be allocated */
      rem_len -= PSTORE_POOL_BUFSIZE;
      
      /* Remember pstore for linkage */
      r = q;
      
      /* Ring buffer current pointer increment */
      *cur_p = *cur_p + 1 >= PSTORE_POOL_SIZE ? 0: *cur_p + 1;
    }
    
    DEBUG_PSTORE
      kprintf(KR_OSTREAM,"demux payload pstore allocated at %08x %d",
	      &pay_p[0],pay_p->tot_len);
    
    /* Do a copy of the hdr from the hw ring buffer into this pstore */
    q = pay_p; 
    totlen = 0;
    do{
      nextoff = q->nextoffset;
      /* Read enough bytes to fill this pbuf in the chain. The
	 available data in the pbuf is given by the q->len
	 variable. */
      s = (void*)(start_address+q->offset);
      memcpy(&s[0],&((char *)t)[totlen],q->len);
      totlen += q->len;
      q = (void*)(start_address + nextoff);
    }while (totlen < pay_len);
    
    /* Chain up the pstores */
    stack_p->tot_len += pay_p->tot_len;
    stack_p->nextsector = pay_p->sector;
    stack_p->nextoffset = (uint32_t)&pay_p[0] - 
      ActiveSessions[ssid].mt[pay_p->sector].start_address;
  }
  
  return stack_p;
}
