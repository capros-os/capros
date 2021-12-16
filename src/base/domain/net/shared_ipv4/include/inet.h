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

#ifndef _INET_H_
#define _INET_H_

#include "ip_addr.h"
#include "pstore.h"

uint16_t inet_chksum(void *dataptr, uint16_t len);
uint16_t inet_chksum_pstore(struct pstore *p,int ssid);
uint16_t inet_chksum_pseudo(struct pstore *p,int ssid,
			    struct ip_addr* src,struct ip_addr* dest,
			    uint8_t proto, uint16_t proto_len);
uint32_t inet_Addr(const char *cp);

#endif /*_INET_H_*/
