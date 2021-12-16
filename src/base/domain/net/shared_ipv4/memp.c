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

#include <domain/domdbg.h>
#include <string.h>

#include "include/memp.h"
#include "include/mem.h"
#include "include/pstore.h"
#include "include/udp.h"
#include "include/tcp.h"

#include "netsyskeys.h"

struct memp {
  struct memp *next;
};

static struct memp* memp_tab[MEMP_MAX];
static const uint16_t memp_sizes[MEMP_MAX] = {
  sizeof(struct udp_pcb),
  sizeof(struct tcp_pcb),
  sizeof(struct tcp_pcb_listen),
  sizeof(struct tcp_seg)
};

static const uint16_t memp_num[MEMP_MAX] = {
  MEMP_NUM_UDP_PCB,
  MEMP_NUM_TCP_PCB,
  MEMP_NUM_TCP_PCB_LISTEN,
  MEMP_NUM_TCP_SEG
};

/*
static uint8_t memp_memory[ MEMP_NUM_UDP_PCB *
			    MEM_ALIGN_SIZE(sizeof(struct udp_pcb) +
					   sizeof(struct memp)) 
			    +
			    MEMP_NUM_TCP_PCB *
			    MEM_ALIGN_SIZE(sizeof(struct tcp_pcb) +
					   sizeof(struct memp)) +
			    MEMP_NUM_TCP_PCB_LISTEN *
			    MEM_ALIGN_SIZE(sizeof(struct tcp_pcb_listen) +
					   sizeof(struct memp)) +
			    MEMP_NUM_TCP_SEG *
			    MEM_ALIGN_SIZE(sizeof(struct tcp_seg) +
					   sizeof(struct memp))
];  
*/

static uint8_t *memp_memory;
extern struct pstore_queue *TxPstoreQ;   /* The Tx pstore Queue */	

void
memp_init(void)
{
  struct memp *m, *memp;
  uint16_t i,j;
  uint16_t size;
  
  /* We start in the transmit segment below the pstores and the 
   * pstore_queues*/
  
  uint32_t memstart = (uint32_t)&TxPstoreQ[Qsize] + 
    sizeof(struct pstore_queue);
  
  memp_memory  = (void *)(memstart);
  
  memp = (struct memp *)&memp_memory[0];
  for(i=0;i<MEMP_MAX; i++) {
    size = MEM_ALIGN_SIZE(memp_sizes[i] + sizeof(struct memp));
    if(memp_num[i] > 0) {
      memp_tab[i] = memp;
      m = memp;
      for(j = 0; j < memp_num[i]; ++j) {
        m->next = (struct memp *)MEM_ALIGN((uint8_t *)m + size);
        memp = m;
        m = m->next;
      }
      memp->next = NULL;
      memp = m;
    } else {
      memp_tab[i] = NULL;
    }
  }
}


/* Look for space in memp_tab & return(NULL if no space)*/
void *
memp_alloc(memp_t type) 
{
  struct memp *memp;
  void *mem;

  memp = memp_tab[type];

  if(memp!=NULL) {
    memp_tab[type] = memp->next;
    memp->next = NULL;
    mem = MEM_ALIGN((uint8_t *)memp + sizeof(struct memp));
    
    /* initialize memp memory with zeroes */
    memset(mem,0,memp_sizes[type]);   
    return mem;
  }else {
    /* memp_alloc: out of memory in pool */
    return NULL;
  }
}


/* free memory */
void
memp_free(memp_t type, void *mem)
{
  struct memp *memp;

  if(mem == NULL) return;
  memp = (struct memp *)((uint8_t *)mem - sizeof(struct memp));

  memp->next = memp_tab[type]; 
  memp_tab[type] = memp;
  
  return;
}
