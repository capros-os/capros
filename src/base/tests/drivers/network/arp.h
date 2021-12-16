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

/* Address Resolution Protocol : see RFC 826 */

#ifndef _ARP_H_
#define	_ARP_H_

typedef struct {
  unsigned short ar_hrd;    /* Hardware address space */
  unsigned short ar_pro;    /* Protocol address space */
  uint8_t  ar_hln;          /* byte length of each hardware address */
  uint8_t  ar_pln;          /* byte length of each protocol address */
  uint16_t ar_op;           /* opcode (ARP_REQUEST | ARP_REPLY )*/ 
  //uint8_t  
}arphdr;

/* Hardware address space */
#define ARPHADR_ETHER    1   /* Ethernet hardware format*/
#define ARPHRD_IEEE802	 6   /* token-ring hardware format */
#define ARPHRD_ARCNET	 7   /* arcnet hardware format */
#define ARPHRD_FRELAY 	 15  /* frame relay hardware format */

/* Protocol address space */


#endif /* F_ARP_H_ */
