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

/* Functions common to all TCP/IP modules, such as the Internet 
 * checksum and the * byte order functions. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <string.h>

#include "timer.h"

#include "include/ip_frag.h"
#include "include/ip.h"
#include "include/inet.h"
#include "include/ethernet.h"
#include "include/mem.h"
#include "include/TxRxqueue.h"
#include "netif/netif.h"

#include "Session.h"
extern struct session ActiveSessions[MAX_SESSIONS];

/* Copy len bytes from offset in pstore to buffer 
 * helper used by both ip_reass and ip_frag */
static struct pstore *
copy_from_pstore(struct pstore *p, int ssid,uint16_t * offset,
				   uint8_t * buffer, uint16_t len)
{
  uint16_t l;

  p->offset += *offset;
  p->len -= *offset;
  while (len) {
    l = len < p->len ? len : p->len;
    memcpy(buffer,PSTORE_PAYLOAD(p,ssid), l);
    buffer += l;
    len -= l;
    if (len)
      p = PSTORE_NEXT(p,ssid);
    else
      *offset = l;
  }
  return p;
}

#define IP_REASS_BUFSIZE 5760
#define IP_REASS_MAXAGE 30
#define IP_REASS_TMO 1000

static uint8_t ip_reassbuf[IP_HLEN + IP_REASS_BUFSIZE];
static uint8_t ip_reassbitmap[IP_REASS_BUFSIZE / (8 * 8)];
static const uint8_t bitmap_bits[8] = { 0xff, 0x7f, 0x3f, 0x1f,
  0x0f, 0x07, 0x03, 0x01
};
static TimeOfDay ip_frag_tod;

static uint16_t ip_reasslen;
static uint8_t ip_reassflags;
#define IP_REASS_FLAG_LASTFRAG 0x01

static uint8_t ip_reasstmr;

struct pstore *
ip_reass(struct pstore *p,int ssid)
{
  struct pstore *q;
  struct ip_hdr *fraghdr, *iphdr;
  uint16_t offset, len;
  uint16_t i;
  TimeOfDay tod;
  
  iphdr = (struct ip_hdr *) ip_reassbuf;
  fraghdr = (struct ip_hdr *) PSTORE_PAYLOAD(p,ssid);
  
  /* Everytime we are into this update the ip_frag_tod. If more than 
   * IP_REASS_MAXAGE has passed */
  if(ip_reasstmr > 0) {
    uint64_t interval;
    
    getTimeOfDay(&tod); /* get time now */
    interval = difftime(&tod,&ip_frag_tod);
    ip_reasstmr = ip_reasstmr - interval > 0? ip_reasstmr - interval:0;
  }
  
  /* If ip_reasstmr is zero, no packet is present in the buffer, so we
   * write the IP header of the fragment into the reassembly
   * buffer. The timer is updated with the maximum age. */
  if (ip_reasstmr == 0) {
    memcpy(iphdr, fraghdr, IP_HLEN);
    ip_reasstmr = IP_REASS_MAXAGE;
    
    /* Make a note of the time now */
    getTimeOfDay(&ip_frag_tod);
    
    ip_reassflags = 0;
    /* Clear the bitmap. */
    memset(ip_reassbitmap, 0, sizeof(ip_reassbitmap));
  }
  
  /* Check if the incoming fragment matches the one currently present
   * in the reasembly buffer. If so, we proceed with copying the
   * fragment into the buffer. */
  if (ip_addr_cmp(&iphdr->src, &fraghdr->src) &&
      ip_addr_cmp(&iphdr->dest, &fraghdr->dest) &&
      IPH_ID(iphdr) == IPH_ID(fraghdr)) {
    
    /* Find out the offset in the reassembly buffer where we should
     * copy the fragment. */
    len = ntohs(IPH_LEN(fraghdr)) - IPH_HL(fraghdr) * 4;
    offset = (ntohs(IPH_OFFSET(fraghdr)) & IP_OFFMASK) * 8;
    
    /* If the offset or the offset + fragment length overflows the
       reassembly buffer, we discard the entire packet. */
    if (offset > IP_REASS_BUFSIZE || offset + len > IP_REASS_BUFSIZE) {
      ip_reasstmr = 0;
      goto nullreturn;
    }
    
    /* Copy the fragment into the reassembly buffer, at the right
       offset. */
    i = IPH_HL(fraghdr) * 4;
    copy_from_pstore(p,ssid,&i, &ip_reassbuf[IP_HLEN + offset], len);
    
    /* Update the bitmap. */
    if (offset / (8 * 8) == (offset + len) / (8 * 8)) {
      /* If the two endpoints are in the same byte, we only update
         that byte. */
      ip_reassbitmap[offset / (8 * 8)] |=
	bitmap_bits[(offset / 8) & 7] &
	~bitmap_bits[((offset + len) / 8) & 7];
    } else {
      /* If the two endpoints are in different bytes, we update the
         bytes in the endpoints and fill the stuff inbetween with
         0xff. */
      ip_reassbitmap[offset / (8 * 8)] |= bitmap_bits[(offset / 8) & 7];
      for (i = 1 + offset / (8 * 8); i < (offset + len) / (8 * 8); ++i) {
	ip_reassbitmap[i] = 0xff;
      }
      ip_reassbitmap[(offset + len) / (8 * 8)] |=
	~bitmap_bits[((offset + len) / 8) & 7];
    }
    
    /* If this fragment has the More Fragments flag set to zero, we
     * know that this is the last fragment, so we can calculate the
     * size of the entire packet. We also set the
     * IP_REASS_FLAG_LASTFRAG flag to indicate that we have received
     * the final fragment. */
    
    if ((ntohs(IPH_OFFSET(fraghdr)) & IP_MF) == 0) {
      ip_reassflags |= IP_REASS_FLAG_LASTFRAG;
      ip_reasslen = offset + len;
    }
    
    /* Finally, we check if we have a full packet in the buffer. We do
       this by checking if we have the last fragment and if all bits
       in the bitmap are set. */
    if (ip_reassflags & IP_REASS_FLAG_LASTFRAG) {
      /* Check all bytes up to and including all but the last byte in
         the bitmap. */
      for (i = 0; i < ip_reasslen / (8 * 8) - 1; ++i) {
	if (ip_reassbitmap[i] != 0xff) {
	  goto nullreturn;
	}
      }
      
      /* Check the last byte in the bitmap. It should contain just the
         right amount of bits. */
      if (ip_reassbitmap[ip_reasslen / (8 * 8)] !=
	  (uint8_t) ~ bitmap_bits[ip_reasslen / 8 & 7]) {
	goto nullreturn;
      }
      
      /* Pretend to be a "normal" (i.e., not fragmented) IP packet
       * from now on. */
      ip_reasslen += IP_HLEN;
      
      IPH_LEN_SET(iphdr, htons(ip_reasslen));
      IPH_OFFSET_SET(iphdr, 0);
      IPH_CHKSUM_SET(iphdr, 0);
      IPH_CHKSUM_SET(iphdr, inet_chksum(iphdr, IP_HLEN));
      
      /* If we have come this far, we have a full packet in the
       * buffer, so we allocate a pstore and copy the packet into it. We
       * also reset the timer. */
      ip_reasstmr = 0;
      pstore_free(p,ssid);
      p = pstore_alloc(PSTORE_LINK, ip_reasslen,RECV_CLIENT_SPACE,ssid);
      if (p != NULL) {
	i = 0;
	for (q = p; q != NULL; q = PSTORE_NEXT(q,ssid)) {
	  /* Copy enough bytes to fill this pstore in the chain. The
	   * available data in the pstore is given by the q->len
	   * variable. */
	  memcpy(PSTORE_PAYLOAD(q,ssid), &ip_reassbuf[i],
		 q->len > ip_reasslen - i ? ip_reasslen - i : q->len);
	  i += q->len;
	}
      } 
      return p;
    }
  }

nullreturn:
  pstore_free(p,ssid);
  return NULL;
}


#define MAX_MTU 1500
//static uint8_t buf[MEM_ALIGN_SIZE(MAX_MTU)];

/* Fragment an IP packet if too large
 * Chop the packet in mtu sized chunks and send them in order
 * by using a fixed size static memory buffer (PSTORE_ROM) */
uint32_t
ip_frag(struct pstore *p,int ssid)
{
  struct pstore *rambuf;
  struct pstore *header;
  struct ip_hdr *iphdr;
  uint16_t nfb = 0;
  uint16_t left, cop;
  uint16_t mtu = NETIF.mtu;
  uint16_t ofo, omf;
  uint16_t last;
  uint16_t poff = IP_HLEN;
  uint16_t tmp;

  /* Get a RAM based MTU sized pstore from sector 2 */
  rambuf = pstore_alloc(PSTORE_LINK,mtu, XMIT_STACK_SPACE,ssid);
  rambuf->tot_len = rambuf->len = mtu;
  //rambuf->payload = MEM_ALIGN((void *)buf);

  /* Copy the IP header in it */
  iphdr = PSTORE_PAYLOAD(rambuf,ssid);
  memcpy(iphdr, PSTORE_PAYLOAD(p,ssid), IP_HLEN);

  /* Save original offset */
  tmp = ntohs(IPH_OFFSET(iphdr));
  ofo = tmp & IP_OFFMASK;
  omf = tmp & IP_MF;

  left = p->tot_len - IP_HLEN;

  while (left) {
    last = (left <= mtu - IP_HLEN);
    
    /* Set new offset and MF flag */
    ofo += nfb;
    tmp = omf | (IP_OFFMASK & (ofo));
    if (!last)
      tmp = tmp | IP_MF;
    IPH_OFFSET_SET(iphdr, htons(tmp));
    
    /* Fill this fragment */
    nfb = (mtu - IP_HLEN) / 8;
    cop = last ? left : nfb * 8;
    
    p = copy_from_pstore(p,ssid,&poff, (uint8_t *) iphdr + IP_HLEN, cop);
    
    /* Correct header */
    IPH_LEN_SET(iphdr, htons(cop + IP_HLEN));
    IPH_CHKSUM_SET(iphdr, 0);
    IPH_CHKSUM_SET(iphdr, inet_chksum(iphdr, IP_HLEN));

    if (last)
      pstore_realloc(rambuf, left + IP_HLEN,ssid);
    /* This part is ugly: we alloc a RAM based pstore for 
     * the link level header for each chunk and then 
     * free it.A PSTORE_ROM style pstore for which pstore_header
     * worked would make things simpler.
     */
    header = pstore_alloc(PSTORE_LINK, 0, XMIT_STACK_SPACE,ssid);
    pstore_chain(header, rambuf,ssid);
    netif_start_xmit(header,ETHERTYPE_IP,&(iphdr->dest),ssid);
    pstore_free(header,ssid);

    left -= cop;
  }
  pstore_free(rambuf,ssid);
  return RC_OK;
}
