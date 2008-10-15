#ifndef __LWIP_LWIPOPTS_H__
#define __LWIP_LWIPOPTS_H__
/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#define NO_SYS 1
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#define MEM_LIBC_MALLOC 1 // use libc's malloc
#include <stdlib.h>		// and declare it
#define MEM_ALIGNMENT 4	// correct for ARM
#define TCP_MSS 1024
#define TCP_SND_BUF (TCP_MSS * 2)

#define LWIP_DEBUG
//#define ETHARP_DEBUG LWIP_DBG_ON
//#define IP_DEBUG    LWIP_DBG_ON
//#define TCP_DEBUG   LWIP_DBG_ON

#endif /* __LWIP_LWIPOPTS_H__ */
