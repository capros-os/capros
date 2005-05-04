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

/* Transmission Control Protocol: based on RFC 793 */

#ifndef __TCP_H__
#define __TCP_H__

#include <eros/target.h>

#include  "mem.h"
#include  "pbuf.h"
#include  "opt.h"
#include  "ip.h"
#include  "icmp.h"
#include  "../netif/netif.h"

/* TCP active pcb states: */
#define ACTIVE_PRECONNECT   0   /* At the start of the world,when 'malloced'*/
#define ACTIVE_CONNECTING   1   /* Connecting to remote host */
#define ACTIVE_TIMEDOUT     2   /* Action requested has timedout */
#define ACTIVE_ESTABLISHED  3   /* Connection est. - report back to user */
#define ACTIVE_NOCONNECT    4   /* Connection failed */

struct tcp_pcb;

/* Functions for interfacing with TCP: */

/* Lower layer interface to TCP: */
void             tcp_init    (void);  /* Must be called first to
					 initialize TCP. */
void             tcp_tmr     (void);  /* Must be called every
					 TCP_TMR_INTERVAL
					 ms. (Typically 100 ms). */

/* Application program's interface: */
struct tcp_pcb * tcp_new     (void);
struct tcp_pcb * tcp_alloc   (uint8_t prio);

void             tcp_arg     (struct tcp_pcb *pcb, void *arg);
void             tcp_accept  (struct tcp_pcb *pcb,
			      uint32_t (* accept)(void *arg,
						  struct tcp_pcb *newpcb,
						  uint32_t err));
void             tcp_recv    (struct tcp_pcb *pcb,
			      uint32_t (* recv)(void *arg,
						struct tcp_pcb *tpcb,
						struct pbuf *p,uint32_t err));
void             tcp_sent    (struct tcp_pcb *pcb,
			      uint32_t (* sent)(void *arg,
						struct tcp_pcb *tpcb,
						uint16_t len));
void             tcp_poll    (struct tcp_pcb *pcb,
			      uint32_t(* poll)(void *arg,struct tcp_pcb *tpcb),
			      uint8_t interval);
void             tcp_err     (struct tcp_pcb *pcb,
			      void (* err)(void *arg, uint32_t err));

#define          tcp_mss(pcb)      ((pcb)->mss)
#define          tcp_sndbuf(pcb)   ((pcb)->snd_buf)

void             tcp_recved  (struct tcp_pcb *pcb, uint16_t len);
uint32_t         tcp_bind    (struct tcp_pcb *pcb, struct ip_addr *ipaddr,
			      uint16_t port);
uint32_t         tcp_connect (struct tcp_pcb *pcb, struct ip_addr *ipaddr,
			      uint16_t port, 
			      uint32_t (* connected)(void *arg,
						     struct tcp_pcb *tpcb,
						     uint32_t err));
struct tcp_pcb * tcp_listen  (struct tcp_pcb *pcb);
void             tcp_abort   (struct tcp_pcb *pcb);
uint32_t         tcp_close   (struct tcp_pcb *pcb);
uint32_t         tcp_write   (struct tcp_pcb *pcb, 
			      const void *dataptr, uint16_t len,
			      uint8_t copy);

void             tcp_setprio (struct tcp_pcb *pcb, uint8_t prio);

#define TCP_PRIO_MIN    1
#define TCP_PRIO_NORMAL 64
#define TCP_PRIO_MAX    127

/* Only used by IP to pass a TCP segment to TCP: */
void             tcp_input   (struct pbuf *p , struct netif *inp);
/* Used within the TCP code only: */
uint32_t         tcp_output  (struct tcp_pcb *pcb);
void             tcp_rexmit  (struct tcp_pcb *pcb);


#define TCP_SEQ_LT(a,b)     ((signed long)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)    ((signed long)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b)     ((signed long)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)    ((signed long)((a)-(b)) >= 0)

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
#define TCP_URG 0x20

#define TCP_FLAGS 0x3f

/* Length of the TCP header, excluding options. */
#define TCP_HLEN 20

#ifndef TCP_TMR_INTERVAL
#define TCP_TMR_INTERVAL       100  /* The TCP timer interval in
                                       milliseconds. */
#endif /* TCP_TMR_INTERVAL */

#ifndef TCP_FAST_INTERVAL
#define TCP_FAST_INTERVAL      200  /* the fine grained timeout in
                                       milliseconds */
#endif /* TCP_FAST_INTERVAL */

#ifndef TCP_SLOW_INTERVAL
#define TCP_SLOW_INTERVAL      500  /* the coarse grained timeout in
                                       milliseconds */
#endif /* TCP_SLOW_INTERVAL */

#define TCP_FIN_WAIT_TIMEOUT 20000 /* milliseconds */
#define TCP_SYN_RCVD_TIMEOUT 20000 /* milliseconds */

#define TCP_OOSEQ_TIMEOUT        6 /* x RTO */

#define TCP_MSL 60000  /* The maximum segment lifetime in microseconds */

struct tcp_hdr {
  uint16_t src;
  uint16_t dest;
  uint32_t seqno;
  uint32_t ackno;
  uint16_t _offset_flags;
  uint16_t wnd;
  uint16_t chksum;
  uint16_t urgp;
}__attribute__ ((packed));

#define TCPH_OFFSET(hdr) (ntohs((hdr)->_offset_flags) >> 8)
#define TCPH_FLAGS(hdr) (ntohs((hdr)->_offset_flags) & 0xff)

#define TCPH_OFFSET_SET(hdr, offset) (hdr)->_offset_flags = \
                                      htons(((offset) << 8) | TCPH_FLAGS(hdr))
#define TCPH_FLAGS_SET(hdr, flags) (hdr)->_offset_flags = \
                                      htons((TCPH_OFFSET(hdr) << 8) | (flags))

#define TCP_TCPLEN(seg) ((seg)->len + ((TCPH_FLAGS((seg)->tcphdr) & TCP_FIN ||\
				  TCPH_FLAGS((seg)->tcphdr) & TCP_SYN)? 1: 0))

enum tcp_state {
  CLOSED      = 0,
  LISTEN      = 1,
  SYN_SENT    = 2,
  SYN_RCVD    = 3,
  ESTABLISHED = 4,
  FIN_WAIT_1  = 5,
  FIN_WAIT_2  = 6,
  CLOSE_WAIT  = 7,
  CLOSING     = 8,
  LAST_ACK    = 9,
  TIME_WAIT   = 10
};


/* the TCP protocol control block */
struct tcp_pcb {
  struct tcp_pcb *next;   /* for the linked list */
  uint8_t prio;
  void *callback_arg;

  struct ip_addr local_ip;
  uint16_t local_port;
  enum tcp_state state;   /* TCP state */
  
  struct ip_addr remote_ip;
  uint16_t remote_port;
  
  /* The session id of the process to whom this udp session belongs */
  uint32_t ssid;

  /* State of the pcb pre-connect, post connect or timedout */
  uint8_t session_state;

  /* time_out at session layer */
  int32_t timeout;
  
  /* receiver varables */
  uint32_t rcv_nxt;   /* next seqno expected */
  uint16_t rcv_wnd;   /* receiver window */
  
  /* Timers */
  uint32_t tmr;
  uint8_t polltmr, pollinterval;
  
  /* Retransmission timer. */
  uint16_t rtime;
  
  uint16_t mss;   /* maximum segment size */

  uint8_t flags;
#define TF_ACK_DELAY 0x01U   /* Delayed ACK. */
#define TF_ACK_NOW   0x02U   /* Immediate ACK. */
#define TF_INFR      0x04U   /* In fast recovery. */
#define TF_RESET     0x08U   /* Connection was reset. */
#define TF_CLOSED    0x10U   /* Connection was sucessfully closed. */
#define TF_GOT_FIN   0x20U   /* Connection was closed by the remote end. */
  
  /* RTT estimation variables. */
  uint16_t rttest; /* RTT estimate in 500ms ticks */
  uint32_t rtseq;  /* sequence number being timed */
  int16_t sa, sv;

  uint16_t rto;    /* retransmission time-out */
  uint8_t nrtx;    /* number of retransmissions */

  /* fast retransmit/recovery */
  uint32_t lastack; /* Highest acknowledged seqno. */
  uint8_t dupacks;
  
  /* congestion avoidance/control variables */
  uint16_t cwnd;  
  uint16_t ssthresh;

  /* sender variables */
  uint32_t snd_nxt,       /* next seqno to be sent */
    snd_max,       /* Highest seqno sent. */
    snd_wnd,       /* sender window */
    snd_wl1, snd_wl2, /* Sequence and acknowledgement numbers of last
			 window update. */
    snd_lbb;       /* Sequence number of next byte to be buffered. */

  uint16_t acked;
  
  uint16_t snd_buf;   /* Available buffer space for sending (in bytes). */
  uint8_t snd_queuelen; /* Available buffer space for sending (in tcp_segs). */
  
  
  /* These are ordered by sequence number: */
  struct tcp_seg *unsent;   /* Unsent (queued) segments. */
  struct tcp_seg *unacked;  /* Sent but unacknowledged segments. */
  struct tcp_seg *ooseq;    /* Received out of sequence segments. */

#if ERIP_CALLBACK_API
  /* Function to be called when more send buffer space is available. */
  uint32_t (* sent)(void *arg, struct tcp_pcb *pcb, uint16_t space);
  
  /* Function to be called when (in-sequence) data has arrived. */
  uint32_t (* recv)(void *arg, struct tcp_pcb *pcb, 
		    struct pbuf *p, uint32_t err);

  /* Function to be called when a connection has been set up. */
  uint32_t (* connected)(void *arg, struct tcp_pcb *pcb, uint32_t err);

  /* Function to call when a listener has been connected. */
  uint32_t (* accept)(void *arg, struct tcp_pcb *newpcb, uint32_t err);

  /* Function which is called periodically. */
  uint32_t (* poll)(void *arg, struct tcp_pcb *pcb);

  /* Function to be called whenever a fatal error occurs. */
  void (* errf)(void *arg, uint32_t err);
#endif /* ERIP_CALLBACK_API */
};

struct tcp_pcb_listen {  
  struct tcp_pcb_listen *next;   /* for the linked list */
  uint8_t prio;
  void *callback_arg;
  
  struct ip_addr local_ip;
  uint16_t local_port; 
  /* Even if state is obviously LISTEN this is here for
   * field compatibility with tpc_pcb to which it is cast sometimes
   * Until a cleaner solution emerges this is here.FIXME
   */ 
  enum tcp_state state;   /* TCP state */

  /* Function to call when a listener has been connected. */
  uint32_t (* accept)(void *arg, struct tcp_pcb *newpcb, uint32_t err);
};

#if ERIP_EVENT_API

enum erip_event {
  ERIP_EVENT_ACCEPT,
  ERIP_EVENT_SENT,
  ERIP_EVENT_RECV,
  ERIP_EVENT_CONNECTED,
  ERIP_EVENT_POLL,
  ERIP_EVENT_ERR
};

uint32_t erip_tcp_event(void *arg, struct tcp_pcb *pcb,
		     enum erip_event,
		     struct pbuf *p,
		     uint16_t size,
		     uint32_t err);

#define TCP_EVENT_ACCEPT(pcb,err,ret) \
                  ret = erip_tcp_event((pcb)->callback_arg, (pcb),\
					ERIP_EVENT_ACCEPT, NULL, 0, err)
#define TCP_EVENT_SENT(pcb,space,ret) \
                  ret = erip_tcp_event((pcb)->callback_arg,\ (pcb),\
				        ERIP_EVENT_SENT, NULL, space, ERR_OK)
#define TCP_EVENT_RECV(pcb,p,err,ret) \
                  ret = erip_tcp_event((pcb)->callback_arg, (pcb),\
			                ERIP_EVENT_RECV, (p), 0, (err))
#define TCP_EVENT_CONNECTED(pcb,err,ret) \
                   ret = erip_tcp_event((pcb)->callback_arg, (pcb),\
			                ERIP_EVENT_CONNECTED, NULL, 0, (err))
#define TCP_EVENT_POLL(pcb,ret) \
                    ret = erip_tcp_event((pcb)->callback_arg, (pcb),\
			                ERIP_EVENT_POLL, NULL, 0, ERR_OK)
#define TCP_EVENT_ERR(errf,arg,err) \
                    erip_tcp_event((arg), NULL, \
			                ERIP_EVENT_ERR, NULL, 0, (err))

#else /* ERIP_EVENT_API */
#define TCP_EVENT_ACCEPT(pcb,err,ret)     \
                        if((pcb)->accept != NULL) \
                        (ret = (pcb)->accept((pcb)->callback_arg,(pcb),(err)))
#define TCP_EVENT_SENT(pcb,space,ret) \
                        if((pcb)->sent != NULL) \
                        (ret = (pcb)->sent((pcb)->callback_arg,(pcb),(space)))
#define TCP_EVENT_RECV(pcb,p,err,ret) \
                        if((pcb)->recv != NULL) \
                        { ret = (pcb)->recv((pcb)->callback_arg,(pcb),(p),(err)); } else { \
						pbuf_free(p); }
#define TCP_EVENT_CONNECTED(pcb,err,ret) \
                        if((pcb)->connected != NULL) \
                        (ret = (pcb)->connected((pcb)->callback_arg,(pcb),(err)))
#define TCP_EVENT_POLL(pcb,ret) \
                        if((pcb)->poll != NULL) \
                        (ret = (pcb)->poll((pcb)->callback_arg,(pcb)))
#define TCP_EVENT_ERR(errf,arg,err) \
                        if((errf) != NULL) \
                        (errf)((arg),(err))
#endif /* ERIP_EVENT_API */

/* This structure is used to repressent TCP segments when queued. */
struct tcp_seg {
  struct tcp_seg *next;    /* used when putting segements on a queue */
  struct pbuf *p;          /* buffer containing data + TCP header */
  void *dataptr;           /* pointer to the TCP data in the pbuf */
  uint16_t len;               /* the TCP length of this segment */
  struct tcp_hdr *tcphdr;  /* the TCP header */
};

/* Internal functions and global variables: */
struct tcp_pcb *tcp_pcb_copy(struct tcp_pcb *pcb);
void tcp_pcb_purge(struct tcp_pcb *pcb);
void tcp_pcb_remove(struct tcp_pcb **pcblist, struct tcp_pcb *pcb);

uint8_t tcp_segs_free(struct tcp_seg *seg);
uint8_t tcp_seg_free(struct tcp_seg *seg);
struct tcp_seg *tcp_seg_copy(struct tcp_seg *seg);

#define tcp_ack(pcb)     if((pcb)->flags & TF_ACK_DELAY) { \
                            (pcb)->flags &= ~TF_ACK_DELAY; \
                            (pcb)->flags |= TF_ACK_NOW; \
                            tcp_output(pcb); \
                         } else { \
                            (pcb)->flags |= TF_ACK_DELAY; \
                         }

#define tcp_ack_now(pcb) (pcb)->flags |= TF_ACK_NOW; \
                         tcp_output(pcb)

uint32_t tcp_send_ctrl(struct tcp_pcb *pcb, uint8_t flags);
uint32_t tcp_enqueue(struct tcp_pcb *pcb, void *dataptr, uint16_t len,
		uint8_t flags, uint8_t copy,
                uint8_t *optdata, uint8_t optlen);

void tcp_rexmit_seg(struct tcp_pcb *pcb, struct tcp_seg *seg);

void tcp_rst(uint32_t seqno, uint32_t ackno,
	     struct ip_addr *local_ip, struct ip_addr *remote_ip,
	     uint16_t local_port, uint16_t remote_port);

uint32_t tcp_next_iss(void);

extern struct tcp_pcb *tcp_input_pcb;
extern uint32_t tcp_ticks;

#if TCP_DEBUG || TCP_INPUT_DEBUG || TCP_OUTPUT_DEBUG
void tcp_debug_print(struct tcp_hdr *tcphdr);
void tcp_debug_print_flags(uint8_t flags);
void tcp_debug_print_state(enum tcp_state s);
void tcp_debug_print_pcbs(void);
int tcp_pcbs_sane(void);
#else
#define tcp_pcbs_sane() 1
#endif /* TCP_DEBUG */

//FIX: TCP_timer
//#if NO_SYS
//#define tcp_timer_needed()
//#else
//void tcp_timer_needed(void);
//#endif

/* The TCP PCB lists. */

/* List of all TCP PCBs in LISTEN state. */
extern struct tcp_pcb_listen *tcp_listen_pcbs;  

/* List of all TCP PCBs that are in a
 *state in which they accept or send data. */
extern struct tcp_pcb *tcp_active_pcbs;  

/* List of all TCP PCBs in TIME-WAIT. */
extern struct tcp_pcb *tcp_tw_pcbs;      

/* Only used for temporary storage. */
extern struct tcp_pcb *tcp_tmp_pcb;     

/* Axioms about the above lists:   
   1) Every TCP PCB that is not CLOSED is in one of the lists.
   2) A PCB is only in one of the lists.
   3) All PCBs in the tcp_listen_pcbs list is in LISTEN state.
   4) All PCBs in the tcp_tw_pcbs list is in TIME-WAIT state.
*/

/* Define two macros, TCP_REG and TCP_RMV that registers a TCP PCB
   with a PCB list or removes a PCB from a list, respectively. */
#if 0
#define TCP_REG(pcbs, npcb) do {\
                            DEBUGF(TCP_DEBUG, ("TCP_REG %p local port %d\n", npcb, npcb->local_port));
                            for(tcp_tmp_pcb = *pcbs;
				tcp_tmp_pcb != NULL;
				tcp_tmp_pcb = tcp_tmp_pcb->next) { 
                                ERIP_ASSERT("TCP_REG: already registered\n", tcp_tmp_pcb != npcb); 
                            } 
                            ERIP_ASSERT("TCP_REG: pcb->state != CLOSED", npcb->state != CLOSED); \
                            npcb->next = *pcbs; \
                            ERIP_ASSERT("TCP_REG: npcb->next != npcb", npcb->next != npcb); \
                            *(pcbs) = npcb; \
                            ERIP_ASSERT("TCP_RMV: tcp_pcbs sane", tcp_pcbs_sane()); \
										      /*tcp_timer_needed();*/ \
                            } while(0)
#define TCP_RMV(pcbs, npcb) do { \
                            ERIP_ASSERT("TCP_RMV: pcbs != NULL", *pcbs != NULL); \
                            DEBUGF(TCP_DEBUG, ("TCP_RMV: removing %p from %p\n", npcb, *pcbs)); \
                            if(*pcbs == npcb) { \
                               *pcbs = (*pcbs)->next; \
                            } else for(tcp_tmp_pcb = *pcbs; tcp_tmp_pcb != NULL; tcp_tmp_pcb = tcp_tmp_pcb->next) { \
                               if(tcp_tmp_pcb->next != NULL && tcp_tmp_pcb->next == npcb) { \
                                  tcp_tmp_pcb->next = npcb->next; \
                                  break; \
                               } \
                            } \
                            npcb->next = NULL; \
                            ERIP_ASSERT("TCP_RMV: tcp_pcbs sane", tcp_pcbs_sane()); \
                            DEBUGF(TCP_DEBUG, ("TCP_RMV: removed %p from %p\n", npcb, *pcbs)); \
                            } while(0)

#else /* ERIP_DEBUG */
#define TCP_REG(pcbs, npcb) do { \
                            npcb->next = *pcbs; \
                            *(pcbs) = npcb; \
							/*tcp_timer_needed();*/ \
                            } while(0)
#define TCP_RMV(pcbs, npcb) do { \
                            if(*(pcbs) == npcb) { \
                               (*(pcbs)) = (*pcbs)->next; \
                            } else for(tcp_tmp_pcb = *pcbs; tcp_tmp_pcb != NULL; tcp_tmp_pcb = tcp_tmp_pcb->next) { \
                               if(tcp_tmp_pcb->next != NULL && tcp_tmp_pcb->next == npcb) { \
                                  tcp_tmp_pcb->next = npcb->next; \
                                  break; \
                               } \
                            } \
                            npcb->next = NULL; \
                            } while(0)
#endif /* ERIP_DEBUG */
#endif /* __TCP_H__ */



