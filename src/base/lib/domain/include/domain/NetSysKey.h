/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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

#ifndef __NETSYS_KEY_H__
#define __NETSYS_KEY_H__

/* This file definies the API for using Network System domain. */

/* To start a session(connection) to the network system, we need to have
 * a unique session key. For this we go to the session creator to give us 
 * a session key. For the session creator key we go the network system.
 * Finally we have all the keys needed to use the network sub system */


/* We need to have a session creator key for starting session */
#define OC_NetSys_GetSessionCreatorKey    1
/* Get our network configuration */
#define OC_NetSys_GetNetConfig            2


/* We start a new session for every new "connection"  */
#define OC_NetSys_GetNewSessionKey        3

/* Close Session */
#define OC_NetSys_CloseSession            4

/* After obtaining a session key, we can finally use the network subsystem
 * The client may want to ... */
#define OC_NetSys_UDPConnect      11
#define OC_NetSys_UDPBind         12
#define OC_NetSys_UDPSend         13
#define OC_NetSys_UDPReceive      14
#define OC_NetSys_UDPClose        15

#define OC_NetSys_ICMPOpen        16
#define OC_NetSys_ICMPPing        17
#define OC_NetSys_ICMPReceive     18
#define OC_NetSys_ICMPClose       19

#define OC_NetSys_TCPBind         20
#define OC_NetSys_TCPConnect      21
#define OC_NetSys_TCPSend         22
#define OC_NetSys_TCPReceive      23
#define OC_NetSys_TCPListen       24
#define OC_NetSys_TCPClose	  25

/* Fault codes */
#define RC_NetSys_AlarmInitFailed     100
#define RC_NetSys_DHCPInitFailed       99 
#define RC_NetSys_NoSessionAvailable   98

#define RC_NetSys_NetworkNotConfigured 80
#define RC_NetSys_UDPConnectFailed     79
#define RC_NetSys_UDPBindFailed        78
#define RC_NetSys_NoExistingSession    77
#define RC_NetSys_PbufsExhausted       76
#define RC_NetSys_PingReplySuccess     75
#define RC_NetSys_PortInUse            74
#define RC_NetSys_MEMPExhausted        73
#define RC_NetSys_UDPNoBind            72
#define RC_NetSys_UDPReceiveTimedOut   71
#define RC_NetSys_TCPAlreadyConnected  70
#define RC_NetSys_TCPConnectTimedOut   69
#define RC_NetSys_TCPConnectFailed     68

#define RC_NetSys_BankRupt             50
#endif  /* __NETSYS_KEY_H__ */
