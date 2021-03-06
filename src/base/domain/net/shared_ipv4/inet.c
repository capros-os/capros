/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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

/* Functions common to all TCP/IP modules, such as the Internet 
 * checksum and the * byte order functions. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>
#include "include/inet.h"
#include "Session.h"
#include "netsyskeys.h"

extern struct session ActiveSessions[MAX_SESSIONS];
/* Sums up all 16 bit words in a memory portion. Also includes any odd byte.
 * This function is used by the other checksum functions.
 * For now, this is not optimized. Must be optimized for the particular
 * processor arcitecture on which it is to run. Preferebly coded in assembler.
 */
static inline uint32_t 
chksum(void *dataptr, int len)
{
  uint32_t acc = 0;
  uint16_t * ptr16 = (uint16_t *)dataptr;
    
  for(acc = 0; len > 1; len -= 2)  
    acc += *ptr16++;
  
  /* add up any odd byte */
  if(len == 1)  acc += htons((uint16_t)((*(uint8_t *)ptr16) & 0xff) << 8);
  
  acc = (acc >> 16) + (acc & 0xffffu);
  
  if ((acc & 0xffff0000) != 0) {
    acc = (acc >> 16) + (acc & 0xffffu);
  }
  
  return (uint16_t) acc;
}

/* Calculates the Internet checksum over a portion of memory. 
 * Used primarely for IP and ICMP.
 */
uint16_t
inet_chksum(void *dataptr, uint16_t len)
{
  uint32_t acc;

  acc = chksum(dataptr, len);
  while(acc >> 16) {
    acc = (acc & 0xffff) + (acc >> 16);
  }    
  return ~(acc & 0xffff);
}

uint16_t
inet_chksum_pstore(struct pstore *p,int ssid)
{
  uint32_t acc;
  struct pstore *q;
  uint8_t swapped;
  int32_t nextoff;
  struct mapping_table *mt = ActiveSessions[ssid].mt;
  
  acc = 0;
  swapped = 0;
  q = p;
  do {
    nextoff = q->nextoffset;
    acc += chksum((void*)(mt[q->sector].start_address + q->offset), q->len);
    while(acc >> 16) acc = (acc & 0xffffu) + (acc >> 16);
    if(q->len % 2 != 0) {
      swapped = 1 - swapped;
      acc = (acc & 0xffu << 8) | (acc & 0xff00u >> 8);
    }
    q = (void *)(mt[q->nextsector].start_address + nextoff);
  }while(nextoff != -1);
 
  if(swapped)  acc = ((acc & 0xffu) << 8) | ((acc & 0xff00u) >> 8);

  return ~(acc & 0xffff);
}



/* Calculates the pseudo Inet checksum used by TCP and UDP for a 
 * pstore chain */
uint16_t
inet_chksum_pseudo(struct pstore *p,int ssid, 
		   struct ip_addr *src,struct ip_addr *dest,
                   uint8_t proto, uint16_t proto_len)
{
  uint32_t acc;
  struct pstore *q = NULL;
  uint8_t swapped;
  int32_t nextoff;
  struct mapping_table *mt = ActiveSessions[ssid].mt;

  acc = 0;
  swapped = 0;
  q = p; 
  do{
    nextoff = q->nextoffset;
    acc += chksum((void*)(mt[q->sector].start_address + q->offset), q->len);
    while(acc >> 16) acc = (acc & 0xffff) + (acc >> 16);
    if(q->len % 2 != 0) {
      swapped = 1 - swapped;
      acc = ((acc & 0xff) << 8) | ((acc & 0xff00) >> 8);
    }
    q = (void *)(mt[q->nextsector].start_address + nextoff);
  }while(nextoff!= -1);    
  
  if(swapped) acc = ((acc & 0xff) << 8) | ((acc & 0xff00) >> 8);
  acc += (src->addr & 0xffff);
  acc += ((src->addr >> 16) & 0xffff);
  acc += (dest->addr & 0xffff);
  acc += ((dest->addr >> 16) & 0xffff);
  acc += (uint32_t)htons((uint16_t)proto);
  acc += (uint32_t)htons(proto_len);  
  
  while(acc >> 16) acc = (acc & 0xffff) + (acc >> 16);
  return ~(acc & 0xffff);
}
