/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/** The netsys client must understand and the manner in which data is
 * written and read into/from the shared spaces. This is done using the 
 * ring data structure. The following functions are here to provide that 
 * abstraction.
 */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>

#include "mapping_table.h"
#include "webs_keys.h"
#include "pstore.h"

#define DEBUG_PSTORE if(0)

/* Globals and externs */
extern struct mapping_table mt[4]; /* The mapping table */

/* Run through the pstore chain freeing up all the pstores. For absolute
 * addresses consult the mapping table */
inline uint32_t 
pstore_free(struct pstore *p)
{
  struct pstore *q = p;
  int32_t nextoff;

  if(&p[0] == NULL) return RC_OK;

  DEBUG_PSTORE {
    static int i;
    i++;
    kprintf(KR_OSTREAM,"(%d) pstore_freeing %08x ref=%d",
	    i,&p[0],p->ref);
  }
  
  do {
    DEBUG_PSTORE kprintf(KR_OSTREAM,"Freeing %08x %d",&q[0],q->ref);
    if(q->ref<=0) return RC_OK;
    nextoff = q->nextoffset;
    q->ref --;
    if(q->ref == 0) {
      q->status = PSTORE_UNUSED;
    }else {
      DEBUG_PSTORE
	kprintf(KR_OSTREAM,"Someone reffed u eh? %08x %d",&q[0],q->ref);
      return RC_OK;
    }
    q = (void *)(nextoff + mt[q->nextsector].start_address);
  }while(nextoff!=-1);
  
  return RC_OK;
}

/** Copy data into this pstore ring 
 * s      - source from where to copy the data into
 * size   - size of data to be copied
 * sect   - into which sector (XMIT or RECV)
 *          This is always XMIT
 */
struct pstore*
copy_data_in(void *s, uint32_t size, sector sect) 
{
  int *cur_p = &mt[sect].cur_p;
  uint32_t start_address = mt[sect].start_address;
  int totlen = 0;
  int alloc_curp2 = mt[sect].cur_p;
  int32_t nextoff, rem_len;
  char * saddr;
  struct pstore *p, *q, *r;

  
  /* The next empty buffer in the ring */
  p = (void *)(start_address + 
	       (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));
  
  /* Now allocate the head of the buffer aggregate */
  if(p->status != PSTORE_UNUSED) {
    kdprintf(KR_OSTREAM,"alloc:sect=%d,cur_p=%d stat=%d add=%08x used",
	     sect,*cur_p,p->status,&p[0]);
    return NULL;
  }
  
  p->status = PSTORE_USED;
  p->tot_len = size;
  p->len = size > PSTORE_POOL_BUFSIZE ? PSTORE_POOL_BUFSIZE : size;
  p->nextsector = -1;
  p->nextoffset = -1;
  p->offset = (uint32_t)&p[0] - start_address + sizeof(struct pstore);
  p->sector = sect;
  p->ref = 1;
  
  /* Increment the ring buffer pointer */
  *cur_p = *cur_p + 1 >= PSTORE_POOL_SIZE ?0:*cur_p + 1; 
  /* remember for linkage */
  r = p;
  rem_len = size - p->len;

  q = p;
  /* Now loop until we have allocated the tail of the ring buffer */
  while(rem_len > 0) {
    /* Our next buffer to look at */
    if(0 == *cur_p) q = (void *)start_address;
    else q = (void*)((uint32_t)&q[0] + PSTORE_POOL_BUFSIZE + 
		     sizeof(struct pstore));
            
    if(q->status != PSTORE_UNUSED) {
      kdprintf(KR_OSTREAM,"alloc:sect=%d,cur_p=%d,remlen=%d used?",
	       sect,*cur_p,rem_len);
      *cur_p = alloc_curp2;
      return NULL;
    }
      
    /* Form the chain */
    r->nextsector = sect;
    r->nextoffset = (uint32_t)&q[0] - start_address;
      
    /* Fill in info for q */
    q->status = PSTORE_USED;
    q->sector = sect;
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
	    &p[0],p->tot_len);
  
  /* Do a copy of the data from the source into this pstore chain */
  q = p; 
  totlen = 0;
  do{
    nextoff = q->nextoffset;
    /* Read enough bytes to fill this pbuf in the chain. The
       available data in the pbuf is given by the q->len
       variable. */
    saddr = (void*)(start_address+q->offset);
    memcpy(&saddr[0],&((char *)s)[totlen],q->len);
    totlen += q->len;
    q = (void*)(start_address + nextoff);
  }while (totlen < size);
    
  return p;
}


/** Read data from the pstore ring AND frees the ring buffers
 * d      - destination into which to copy data into
 * sect   - from which sector to read (XMIT or RECV)
 *          This is always RECV
 * Return - Bytes copied from the ring
 */
uint32_t
read_data_from(void *d, sector sect)
{
  int *cur_p = &mt[sect].cur_p;
  uint32_t start_address = mt[sect].start_address;
  int32_t nextoff;
  int32_t totlen = 0;
  struct pstore *p, *q;
  void *s;
 
  /* The next empty buffer in the ring */
  p = (void *)(start_address + 
	       (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));
  q = p;
  do {
    nextoff = q->nextoffset;
    s = (void *)(mt[q->sector].start_address + q->offset);
    memcpy(&(((char *)d)[totlen]),s,q->len);
    totlen += q->len;
    q = (void *)(mt[q->nextsector].start_address + nextoff);
  }while(nextoff!=-1);
  
  /* free the rings */
  pstore_free(p);
  
  return totlen;
}

/* Given sector frees all used buffers */
uint32_t 
pstore_fast_free(sector sect)
{
  int *cur_p = &mt[sect].cur_p;
  struct pstore *p;
  uint32_t start_address = mt[sect].start_address;
   
  /* The next empty buffer in the ring */
  p = (void *)(start_address + 
	       (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));
  
  return pstore_free(p);
}
