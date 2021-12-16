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

/* Internet Protocol : see rfc 791. Taken from freebsd */

#ifndef _IP_ADDR_H_
#define _IP_ADDR_H_

#define IP_ADDR_ANY (&ip_addr_any)
#define IP_ADDR_BROADCAST (&ip_addr_broadcast)

struct ip_addr {
  uint32_t addr;
};

/* used by IP_ADDR_ANY and IP_ADDR_BROADCAST */
static const struct ip_addr ip_addr_any = { 0x00000000UL };
static const struct ip_addr ip_addr_broadcast = { 0xffffffffUL };


#define IP4_ADDR(ipaddr,a,b,c,d) (ipaddr)->addr = htonl(((uint32_t)(a & 0xff) \
                                  << 24) | ((uint32_t)(b & 0xff) << 16) | \
                           ((uint32_t)(c & 0xff) << 8) | (uint32_t)(d & 0xff))

#define ip_addr_set(dest,src) (dest)->addr = ((struct ip_addr *)src)->addr
#define ip_addr_maskcmp(addr1,addr2,mask) (((addr1)->addr & \
                                            (mask)->addr) == \
                                            ((addr2)->addr & \
                                             (mask)->addr))
#define ip_addr_cmp(addr1,addr2) ((addr1)->addr == (addr2)->addr)
#define ip_addr_isany(addr1) ((addr1) == NULL || (addr1)->addr == 0)
#define ip_addr_isbroadcast(addr1,mask) (((((addr1)->addr) & ~((mask)->addr))\
					  == \
					 (0xffffffff & ~((mask)->addr))) || \
                                         ((addr1)->addr == 0xffffffff) || \
                                         ((addr1)->addr == 0x00000000))


#define ip_addr_ismulticast(addr1) (((addr1)->addr & ntohl(0xf0000000)) == ntohl(0xe0000000))
				   

#define ip_addr_debug_print(ipaddr) DEBUGF(ERIP_DEBUG, ("%d.%d.%d.%d", \
		    (uint8_t)(ntohl((ipaddr)->addr) >> 24) & 0xff, \
		    (uint8_t)(ntohl((ipaddr)->addr) >> 16) & 0xff, \
		    (uint8_t)(ntohl((ipaddr)->addr) >> 8) & 0xff, \
		    (uint8_t)ntohl((ipaddr)->addr) & 0xff))


#define ip4_addr1(ipaddr) ((uint8_t)(ntohl((ipaddr)->addr) >> 24) & 0xff)
#define ip4_addr2(ipaddr) ((uint8_t)(ntohl((ipaddr)->addr) >> 16) & 0xff)
#define ip4_addr3(ipaddr) ((uint8_t)(ntohl((ipaddr)->addr) >> 8) & 0xff)
#define ip4_addr4(ipaddr) ((uint8_t)(ntohl((ipaddr)->addr)) & 0xff)
#endif /* _IP_ADDR_H_*/
