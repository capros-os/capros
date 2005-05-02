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
 
#ifndef __IP_CMDS__
#define __IP_CMDS__

/* Client Commands and Argument */
#define OC_Ip_Xmit    11    /* Transmit data */
// Rcv_w1   = Total Length[16 bit] + Identification [16 bit]
// Rcv_w2   = Flags [3 bit] + Fragment Offset [13 bit] + TTL [8 bit] + Protocol [8 bit]
// Rcv_w3   = Destination IP Addr
// Rcv data = Data

#define OC_Ip_Recv    12    /* Receive data */

 
/* Return Codes */
#define RC_Ip_Xmit_Error            100
#define RC_Ip_Recv_Error            101

 
#endif /*__IP_CMDS__*/
