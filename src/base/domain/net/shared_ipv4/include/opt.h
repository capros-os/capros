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
                                                                                                

#ifndef __OPT_H__
#define __OPT_H__

#include "debug.h"

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#ifndef MEMP_NUM_PSTORE
#define MEMP_NUM_PSTORE                 100
#endif

/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#ifndef MEMP_NUM_UDP_PCB
#define MEMP_NUM_UDP_PCB                4
#endif
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB
#define MEMP_NUM_TCP_PCB                7
#endif
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#ifndef MEMP_NUM_TCP_PCB_LISTEN
#define MEMP_NUM_TCP_PCB_LISTEN         10
#endif
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#ifndef MEMP_NUM_TCP_SEG
#define MEMP_NUM_TCP_SEG                50
#endif
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#ifndef MEMP_NUM_SYS_TIMEOUT
#define MEMP_NUM_SYS_TIMEOUT            3
#endif

/* The following four are used only with the sequential API and can be
   set to 0 if the application only will use the raw API. */
/* MEMP_NUM_NETBUF: the number of struct netbufs. */
#ifndef MEMP_NUM_NETBUF
#define MEMP_NUM_NETBUF                 2
#endif
/* MEMP_NUM_NETCONN: the number of struct netconns. */
#ifndef MEMP_NUM_NETCONN
#define MEMP_NUM_NETCONN                4
#endif
/* MEMP_NUM_APIMSG: the number of struct api_msg, used for
   communication between the TCP/IP stack and the sequential
   programs. */
#ifndef MEMP_NUM_API_MSG
#define MEMP_NUM_API_MSG                8
#endif
/* MEMP_NUM_TCPIPMSG: the number of struct tcpip_msg, which is used
   for sequential API communication and incoming packets. Used in
   src/api/tcpip.c. */
#ifndef MEMP_NUM_TCPIP_MSG
#define MEMP_NUM_TCPIP_MSG              8
#endif

/* ---------- ARP options ---------- */

/** Number of active hardware address, IP address pairs cached */
#ifndef ARP_TABLE_SIZE
#define ARP_TABLE_SIZE                  10
#endif

/**
 * If enabled, outgoing packets are queued during hardware address
 * resolution. The etharp.c implementation queues 1 packet only.
 */
#ifndef ARP_QUEUEING
#define ARP_QUEUEING                    0
#endif
/** If enabled, the first packet queued will not be overwritten by
 * later packets. If disabled, later packets overwrite early packets
 * in the queue. Default is disabled, which is recommended. 
 */
#ifndef ARP_QUEUE_FIRST
#define ARP_QUEUE_FIRST                 0
#endif
/**
 * If defined to 1, cache entries are updated or added for every kind of ARP traffic
 * or broadcast IP traffic. Recommended for routers.
 * If defined to 0, only existing cache entries are updated. Entries are added when
 * erip is sending to them. Recommended for embedded devices.
 */
#ifndef ETHARP_ALWAYS_INSERT
#define ETHARP_ALWAYS_INSERT            1
#endif

/* ---------- IP options ---------- */
/* Define IP_FORWARD to 1 if you wish to have the ability to forward
   IP packets across network interfaces. If you are going to run erip
   on a device with only one network interface, define this to 0. */
#ifndef IP_FORWARD
#define IP_FORWARD                      0
#endif

/** IP reassembly and segmentation. Even if they both deal with IP
 *  fragments, note that these are orthogonal, one dealing with incoming
 *  packets, the other with outgoing packets
 */

/** Reassemble incoming fragmented IP packets */
#ifndef IP_REASSEMBLY
#define IP_REASSEMBLY                   1
#endif

/** Fragment outgoing IP packets if their size exceeds MTU */
#ifndef IP_FRAG
#define IP_FRAG                         1
#endif

/* ---------- ICMP options ---------- */

#ifndef ICMP_TTL
#define ICMP_TTL                        255
#endif

/* ---------- DHCP options ---------- */

/* 1 if you want to do an ARP check on the offered address
   (recommended). */
#ifndef DHCP_DOES_ARP_CHECK
#define DHCP_DOES_ARP_CHECK             0
#endif

/* ---------- UDP options ---------- */
#ifndef ERIP_UDP
#define ERIP_UDP                        1
#endif

#ifndef UDP_TTL
#define UDP_TTL                         255
#endif

/* ---------- TCP options ---------- */
#ifndef TCP_TTL
#define TCP_TTL                         255
#endif

#ifndef TCP_WND
#define TCP_WND                        62720 
#endif 

#ifndef TCP_MAXRTX
#define TCP_MAXRTX                      12
#endif

#ifndef TCP_SYNMAXRTX
#define TCP_SYNMAXRTX                   4 
#endif


/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#ifndef TCP_QUEUE_OOSEQ
#define TCP_QUEUE_OOSEQ                 1
#endif

/* TCP Maximum segment size. */
#ifndef TCP_MSS
#define TCP_MSS                        8960
#endif

/* TCP sender buffer space (bytes). */
#ifndef TCP_SND_BUF
#define TCP_SND_BUF                    64512
#endif

/* TCP sender buffer space (pbufs). This must be at least = 2 *
   TCP_SND_BUF/TCP_MSS for things to work. */
#ifndef TCP_SND_QUEUELEN
#define TCP_SND_QUEUELEN                2048*16
#endif


/* Maximum number of retransmissions of data segments. */

/* Maximum number of retransmissions of SYN segments. */

/* TCP writable space (bytes). This must be less than or equal
   to TCP_SND_BUF. It is the amount of space which must be
   available in the tcp snd_buf for select to return writable */
#ifndef TCP_SNDLOWAT
#define TCP_SNDLOWAT                    TCP_SND_BUF/2
#endif

#ifndef ERIP_EVENT_API
#define ERIP_EVENT_API                  0
#define ERIP_CALLBACK_API               1
#else 
#define ERIP_EVENT_API                  1
#define ERIP_CALLBACK_API               0
#endif 

#ifndef ERIP_COMPAT_SOCKETS
#define ERIP_COMPAT_SOCKETS             1
#endif


#ifndef TCPIP_THREAD_PRIO
#define TCPIP_THREAD_PRIO               1
#endif

#ifndef SLIPIF_THREAD_PRIO
#define SLIPIF_THREAD_PRIO              1
#endif

#ifndef PPP_THREAD_PRIO
#define PPP_THREAD_PRIO                 1
#endif

#ifndef DEFAULT_THREAD_PRIO
#define DEFAULT_THREAD_PRIO             1
#endif

/* ---------- Statistics options ---------- */
#ifndef ERIP_STATS
#define ERIP_STATS                      1
#endif

#if ERIP_STATS

#define LINK_STATS
#define IP_STATS
#define ICMP_STATS
#define UDP_STATS
#define TCP_STATS
#define MEM_STATS
#define MEMP_STATS
#define PBUF_STATS
#define SYS_STATS

#endif /* ERIP_STATS */

/* ---------- PPP options ---------- */

#ifndef PPP_SUPPORT
#define PPP_SUPPORT                     0      /* Set for PPP */
#endif

#if PPP_SUPPORT 

#define NUM_PPP                         1      /* Max PPP sessions. */



#ifndef PAP_SUPPORT
#define PAP_SUPPORT                     0      /* Set for PAP. */
#endif

#ifndef CHAP_SUPPORT
#define CHAP_SUPPORT                    0      /* Set for CHAP. */
#endif

#define MSCHAP_SUPPORT                  0      /* Set for MSCHAP (NOT FUNCTIONAL!) */
#define CBCP_SUPPORT                    0      /* Set for CBCP (NOT FUNCTIONAL!) */
#define CCP_SUPPORT                     0      /* Set for CCP (NOT FUNCTIONAL!) */

#ifndef VJ_SUPPORT
#define VJ_SUPPORT                      0      /* Set for VJ header compression. */
#endif

#ifndef MD5_SUPPORT
#define MD5_SUPPORT                     0      /* Set for MD5 (see also CHAP) */
#endif


/*
 * Timeouts.
 */
#define FSM_DEFTIMEOUT                  6       /* Timeout time in seconds */
#define FSM_DEFMAXTERMREQS              2       /* Maximum Terminate-Request transmissions */
#define FSM_DEFMAXCONFREQS              10      /* Maximum Configure-Request transmissions */
#define FSM_DEFMAXNAKLOOPS              5       /* Maximum number of nak loops */

#define UPAP_DEFTIMEOUT                 6       /* Timeout (seconds) for retransmitting req */
#define UPAP_DEFREQTIME                 30      /* Time to wait for auth-req from peer */

#define CHAP_DEFTIMEOUT                 6       /* Timeout time in seconds */
#define CHAP_DEFTRANSMITS               10      /* max # times to send challenge */


/* Interval in seconds between keepalive echo requests, 0 to disable. */
#if 1
#define LCP_ECHOINTERVAL                0
#else
#define LCP_ECHOINTERVAL                10
#endif

/* Number of unanswered echo requests before failure. */
#define LCP_MAXECHOFAILS                3

/* Max Xmit idle time (in jiffies) before resend flag char. */
#define PPP_MAXIDLEFLAG                 100

/*
 * Packet sizes
 *
 * Note - lcp shouldn't be allowed to negotiate stuff outside these
 *    limits.  See lcp.h in the pppd directory.
 * (XXX - these constants should simply be shared by lcp.c instead
 *    of living in lcp.h)
 */
#define PPP_MTU                         1500     /* Default MTU (size of Info field) */
#if 0
#define PPP_MAXMTU  65535 - (PPP_HDRLEN + PPP_FCSLEN)
#else
#define PPP_MAXMTU                      1500 /* Largest MTU we allow */
#endif
#define PPP_MINMTU                      64
#define PPP_MRU                         1500     /* default MRU = max length of info field */
#define PPP_MAXMRU                      1500     /* Largest MRU we allow */
#define PPP_DEFMRU                      296             /* Try for this */
#define PPP_MINMRU                      128             /* No MRUs below this */


#define MAXNAMELEN                      256     /* max length of hostname or name for auth */
#define MAXSECRETLEN                    256     /* max length of password or secret */

#endif /* PPP_SUPPORT */


/* Debugging options all default to off */

#ifndef DBG_TYPES_ON
#define DBG_TYPES_ON                    0
#endif

#ifndef DEMO_DEBUG
#define DEMO_DEBUG                      DBG_OFF
#endif

#ifndef ETHARP_DEBUG
#define ETHARP_DEBUG                    DBG_OFF
#endif

#ifndef NETIF_DEBUG
#define NETIF_DEBUG                     DBG_OFF
#endif

#ifndef PBUF_DEBUG
#define PBUF_DEBUG                      DBG_OFF
#endif

#ifndef API_LIB_DEBUG
#define API_LIB_DEBUG                   DBG_OFF
#endif

#ifndef API_MSG_DEBUG
#define API_MSG_DEBUG                   DBG_OFF
#endif

#ifndef SOCKETS_DEBUG
#define SOCKETS_DEBUG                   DBG_OFF
#endif

#ifndef ICMP_DEBUG
#define ICMP_DEBUG                      DBG_OFF
#endif

#ifndef INET_DEBUG
#define INET_DEBUG                      DBG_OFF
#endif

#ifndef IP_DEBUG
#define IP_DEBUG                        DBG_OFF
#endif

#ifndef IP_REASS_DEBUG
#define IP_REASS_DEBUG                  DBG_OFF
#endif

#ifndef MEM_DEBUG
#define MEM_DEBUG                       DBG_OFF
#endif

#ifndef MEMP_DEBUG
#define MEMP_DEBUG                      DBG_OFF
#endif

#ifndef SYS_DEBUG
#define SYS_DEBUG                       DBG_OFF
#endif

#ifndef TCP_DEBUG
#define TCP_DEBUG                       DBG_OFF
#endif

#ifndef TCP_INPUT_DEBUG
#define TCP_INPUT_DEBUG                 DBG_OFF
#endif

#ifndef TCP_FR_DEBUG
#define TCP_FR_DEBUG                    DBG_OFF
#endif

#ifndef TCP_RTO_DEBUG
#define TCP_RTO_DEBUG                   DBG_OFF
#endif

#ifndef TCP_REXMIT_DEBUG
#define TCP_REXMIT_DEBUG                DBG_OFF
#endif

#ifndef TCP_CWND_DEBUG
#define TCP_CWND_DEBUG                  DBG_OFF
#endif

#ifndef TCP_WND_DEBUG
#define TCP_WND_DEBUG                   DBG_OFF
#endif

#ifndef TCP_OUTPUT_DEBUG
#define TCP_OUTPUT_DEBUG                DBG_OFF
#endif

#ifndef TCP_RST_DEBUG
#define TCP_RST_DEBUG                   DBG_OFF
#endif

#ifndef TCP_QLEN_DEBUG
#define TCP_QLEN_DEBUG                  DBG_OFF
#endif

#ifndef UDP_DEBUG
#define UDP_DEBUG                       DBG_OFF
#endif

#ifndef TCPIP_DEBUG
#define TCPIP_DEBUG                     DBG_OFF
#endif

#ifndef PPP_DEBUG 
#define PPP_DEBUG                       DBG_OFF
#endif

#ifndef SLIP_DEBUG 
#define SLIP_DEBUG                      DBG_OFF
#endif

#ifndef DHCP_DEBUG 
#define DHCP_DEBUG                      DBG_OFF
#endif


#ifndef DBG_MIN_LEVEL
#define DBG_MIN_LEVEL                   DBG_LEVEL_OFF
#endif

#endif /* __OPT_H__ */



