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

/* tcp_input.c
 * The input processing functions of TCP.4
 * These functions are generally called in the order 
 *         (ip_input() ->) tcp_input() ->
 * tcp_process() -> tcp_receive() (-> application).
 */

#include <string.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include <idl/capros/Stream.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include "netif/netif.h"

#include "include/mem.h"
#include "include/memp.h"
#include "include/inet.h"
#include "include/tcp.h"
#include "include/def.h"
#include "include/opt.h"
#include "include/stats.h"
#include "include/err.h"

#include "keyring.h"

#define DEBUG_TCPIN if(0)
#if NETWORK_CARD == LANCE
#define STATS if(0)
#else
#define  STATS if(1)
#endif

/* These variables are global to all functions involved in the input
 * processing of TCP segments. They are set by the tcp_input()
 * function. */
static struct tcp_seg inseg;
static struct tcp_hdr *tcphdr;
static struct ip_hdr *iphdr;
static uint32_t seqno, ackno;
static uint8_t flags;
static uint16_t tcplen;

static uint8_t recv_flags;
static struct pbuf *recv_data;

struct tcp_pcb *tcp_input_pcb;

/* Forward declarations. */
static uint32_t tcp_process(struct tcp_pcb *pcb);
static void tcp_receive(struct tcp_pcb *pcb);
static void tcp_parseopt(struct tcp_pcb *pcb);

static uint32_t tcp_listen_input(struct tcp_pcb_listen *pcb);
static uint32_t tcp_timewait_input(struct tcp_pcb *pcb);

/* Measurement counters */
uint64_t global_counter;
uint32_t user_counterHi, user_counterLo;
extern int total_interrupts;


/* The initial input processing of TCP. It verifies the TCP header, 
 * demultiplexes the segment between the PCBs and passes it on to 
 * tcp_process(), which implements the TCP finite state machine. This
 * function is called by the IP layer (in ip_input()).
 */
void
tcp_input(struct pbuf *p, struct netif *inp)
{
  struct tcp_pcb *pcb, *prev;
  struct tcp_pcb_listen *lpcb;
  uint8_t offset;
  uint32_t err;

  iphdr = p->payload;
  tcphdr = (struct tcp_hdr *)((uint8_t *)p->payload + IPH_HL(iphdr) * 4);
  
  /* remove header from payload */ 
  if (pbuf_header(p, -((int16_t)(IPH_HL(iphdr) * 4))) || 
      (p->tot_len < sizeof(struct tcp_hdr))) {
    /* drop short packets */
    kprintf(KR_OSTREAM,"tcp_input: short packet(%d bytes) discarded",
	    p->tot_len);
    pbuf_free(p);
    return;
  }
  
  /* Don't even process incoming broadcasts/multicasts. */
  if (ip_addr_isbroadcast(&(iphdr->dest), &(inp->netmask)) ||
      ip_addr_ismulticast(&(iphdr->dest))) {
    pbuf_free(p);
    return;
  }

#if 0
  /* Verify TCP checksum. */
  if (inet_chksum_pseudo(p, (struct ip_addr *)&(iphdr->src),
			 (struct ip_addr *)&(iphdr->dest),
			 IP_PROTO_TCP, p->tot_len) != 0) {
    kprintf(KR_OSTREAM,"tcp_input:packet discarded due to failing chksum 0x%x",
	    inet_chksum_pseudo(p, (struct ip_addr *)&(iphdr->src),
			       (struct ip_addr *)&(iphdr->dest),
			       IP_PROTO_TCP, p->tot_len));
#if TCP_DEBUG
    tcp_debug_print(tcphdr);
#endif /* TCP_DEBUG */
    pbuf_free(p);
    return;
  }
#endif

  /* Move the payload pointer in the pbuf so that it points to the
   *  TCP data instead of the TCP header. */
  offset = TCPH_OFFSET(tcphdr) >> 4;
  pbuf_header(p, -(offset * 4));
  
  /* Convert fields in TCP header to host byte order. */
  tcphdr->src = ntohs(tcphdr->src);
  tcphdr->dest = ntohs(tcphdr->dest);
  seqno = tcphdr->seqno = ntohl(tcphdr->seqno);
  ackno = tcphdr->ackno = ntohl(tcphdr->ackno);
  tcphdr->wnd = ntohs(tcphdr->wnd);

  flags = TCPH_FLAGS(tcphdr) & TCP_FLAGS;
  tcplen = p->tot_len + ((flags & TCP_FIN || flags & TCP_SYN)? 1: 0);
  
  /* Demultiplex an incoming segment. First, we check if it is destined
   * for an active connection. */  
  prev = NULL;  
  for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
    ERIP_ASSERT("tcp_input: active pcb->state != CLOSED", 
		pcb->state != CLOSED);
    ERIP_ASSERT("tcp_input: active pcb->state != TIME-WAIT", 
		pcb->state != TIME_WAIT);
    ERIP_ASSERT("tcp_input: active pcb->state != LISTEN", 
		pcb->state != LISTEN);
    if (pcb->remote_port == tcphdr->src &&
	pcb->local_port == tcphdr->dest &&
	ip_addr_cmp(&(pcb->remote_ip), &(iphdr->src)) &&
	ip_addr_cmp(&(pcb->local_ip), &(iphdr->dest))) {
      
      /* Move this PCB to the front of the list so that subsequent
       * lookups will be faster (we exploit locality in TCP segment
       * arrivals). */
      ERIP_ASSERT("tcp_input: pcb->next != pcb (before cache)", 
		  pcb->next != pcb);
      if (prev != NULL) {
	prev->next = pcb->next;
	pcb->next = tcp_active_pcbs;
	tcp_active_pcbs = pcb; 
      }
      ERIP_ASSERT("tcp_input: pcb->next != pcb (after cache)", 
		  pcb->next != pcb);
      break;
    }
    prev = pcb;
  }

  if (pcb == NULL) {
    /* If it did not go to an active connection, we check the connections
     * in the TIME-WAIT state. */
    for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
      ERIP_ASSERT("tcp_input: TIME-WAIT pcb->state == TIME-WAIT", 
		  pcb->state == TIME_WAIT);
      if (pcb->remote_port == tcphdr->src &&
	  pcb->local_port == tcphdr->dest &&
	  ip_addr_cmp(&(pcb->remote_ip), &(iphdr->src)) &&
	  ip_addr_cmp(&(pcb->local_ip), &(iphdr->dest))) {
	/* We don't really care enough to move this PCB to the front
	 * of the list since we are not very likely to receive that
	 * many segments for connections in TIME-WAIT. */
	DEBUG_TCPIN
	  kprintf(KR_OSTREAM,"tcp_input: packed for TIME_WAITing connection");
	tcp_timewait_input(pcb);
	pbuf_free(p);
	return;	  
      }
    }  
  
    /* Finally, if we still did not get a match, we check all PCBs that
     * are LISTENing for incoming connections. */
    prev = NULL;  
    for(lpcb = tcp_listen_pcbs; lpcb != NULL; lpcb = lpcb->next) {
      if ((ip_addr_isany(&(lpcb->local_ip)) ||
	   ip_addr_cmp(&(lpcb->local_ip), &(iphdr->dest))) &&
	  lpcb->local_port == tcphdr->dest) {	  
	/* Move this PCB to the front of the list so that subsequent
	 * lookups will be faster (we exploit locality in TCP segment
	 * arrivals). */
	if (prev != NULL) {
	  ((struct tcp_pcb_listen *)prev)->next = lpcb->next;
          /* our successor is the remainder of the listening list */
	  lpcb->next = tcp_listen_pcbs;
          /* put this listening pcb at the head of the listening list */
	  tcp_listen_pcbs = lpcb; 
	}

	DEBUG_TCPIN 
	  kprintf(KR_OSTREAM,"tcp_input: packed for LISTENing connection");
	tcp_listen_input(lpcb);
	pbuf_free(p);
	return;
      }
      prev = (struct tcp_pcb *)lpcb;
    }
  }
  
#if TCP_INPUT_DEBUG
  kprintf(KR_OSTREAM, ("+-+-+-+-+-+-+-+-+-+-+-+-+-+- tcp_input: flags ");
  tcp_debug_print_flags(TCPH_FLAGS(tcphdr));
  kprintf(KR_OSTREAM, ("-+-+-+-+-+-+-+-+-+-+-+-+-+-+\n");
#endif /* TCP_INPUT_DEBUG */

  
  if (pcb != NULL) {
    /* The incoming segment belongs to a connection. */
#if TCP_INPUT_DEBUG
#if TCP_DEBUG
    tcp_debug_print_state(pcb->state);
#endif /* TCP_DEBUG */
#endif /* TCP_INPUT_DEBUG */
    
    /* Set up a tcp_seg structure. */
    inseg.next = NULL;
    inseg.len = p->tot_len;
    inseg.dataptr = p->payload;
    inseg.p = p;
    inseg.tcphdr = tcphdr;
    
    recv_data = NULL;
    recv_flags = 0;
    
    tcp_input_pcb = pcb;
    err = tcp_process(pcb);
    tcp_input_pcb = NULL;
    /* A return value of ERR_ABRT means that tcp_abort() was called
     * and that the pcb has been freed. If so, we don't do anything. */
    if (err != ERR_ABRT) {
      if (recv_flags & TF_RESET) {
	/* TF_RESET means that the connection was reset by the other
	 * end. We then call the error callback to inform the
	 * application that the connection is dead before we
	 * deallocate the PCB. */
	TCP_EVENT_CONNECTED(pcb,ERR_RST, err);
	TCP_EVENT_ERR(pcb->errf, pcb->callback_arg, ERR_RST);
	if(pcb->session_state == ACTIVE_CONNECTING) {
	  tcp_pcb_remove(&tcp_active_pcbs, pcb);            
	  memp_free(MEMP_TCP_PCB, pcb);
	}
      } else if (recv_flags & TF_CLOSED) {
	/* The connection has been closed and we will deallocate the
	 * PCB. */
	tcp_pcb_remove(&tcp_active_pcbs, pcb);
	memp_free(MEMP_TCP_PCB, pcb);
      } else {
	err = RC_OK;
	/* If the application has registered a "sent" function to be
	 * called when new send buffer space is available, we call it
	 * now. */
	if (pcb->acked > 0) {
	  TCP_EVENT_SENT(pcb, pcb->acked, err);
	}
	
	if (recv_data != NULL) {
	  /* Notify application that data has been received. */
	  TCP_EVENT_RECV(pcb, recv_data, RC_OK, err);
	}
	
	/* If a FIN segment was received, we call the callback
	   function with a NULL buffer to indicate EOF. */
	if (recv_flags & TF_GOT_FIN) {
	  TCP_EVENT_RECV(pcb, NULL, RC_OK, err);
	}
	/* If there were no errors, we try to send something out. */
	if (err == RC_OK) {
	  tcp_output(pcb);
	}
      }
    }
    
    /* We deallocate the incoming pbuf. If it was buffered by the
     * application, the application should have called pbuf_ref() to
     * increase the reference counter in the pbuf. If so, the buffer
     * isn't actually deallocated by the call to pbuf_free(), only the
     * reference count is decreased. */
    pbuf_free(inseg.p);
#if TCP_INPUT_DEBUG
#if TCP_DEBUG
    tcp_debug_print_state(pcb->state);
#endif /* TCP_DEBUG */
#endif /* TCP_INPUT_DEBUG */
  } else {
    /* If no matching PCB was found, send a TCP RST (reset) to the
     * sender. */
    DEBUGF(TCP_RST_DEBUG, ("tcp_input: no PCB match found, resetting.\n"));
    if (!(TCPH_FLAGS(tcphdr) & TCP_RST)) {
      tcp_rst(ackno, seqno + tcplen,
	      &(iphdr->dest), &(iphdr->src),
	      tcphdr->dest, tcphdr->src);
    }
    pbuf_free(p);
  }

  ERIP_ASSERT("tcp_input: tcp_pcbs_sane()", tcp_pcbs_sane());  
}

/* tcp_listen_input():
 * Called by tcp_input() when a segment arrives for a listening
 * connection.
 */
static uint32_t
tcp_listen_input(struct tcp_pcb_listen *pcb)
{
  struct tcp_pcb *npcb;
  uint32_t optdata;
    
  /* In the LISTEN state, we check for incoming SYN segments,
   * creates a new PCB, and responds with a SYN|ACK. */
  if (flags & TCP_ACK) {
    /* For incoming segments with the ACK flag set, respond with a RST. */
    kprintf(KR_OSTREAM,"tcp_listen_input:ACK in LISTEN, sending reset");
    tcp_rst(ackno + 1, seqno + tcplen,&(iphdr->dest), &(iphdr->src),
	    tcphdr->dest, tcphdr->src);
  } else if (flags & TCP_SYN) {
    DEBUG_TCPIN 
      kprintf(KR_OSTREAM,"TCP connection request %d -> %d", 
	      tcphdr->src, tcphdr->dest);
    npcb = tcp_alloc(pcb->prio);
    /* If a new PCB could not be created (probably due to lack of memory),
     * we don't do anything, but rely on the sender will retransmit the
     * SYN at a time when we have more memory available. */
    if (npcb == NULL) {
      kprintf(KR_OSTREAM,"tcp_listen_input: could not allocate PCB");
      return ERR_MEM;
    }
    /* Set up the new PCB. */
    ip_addr_set(&(npcb->local_ip), &(iphdr->dest));
    npcb->local_port = pcb->local_port;
    ip_addr_set(&(npcb->remote_ip), &(iphdr->src));
    npcb->remote_port = tcphdr->src;
    npcb->state = SYN_RCVD;
    npcb->rcv_nxt = seqno + 1;
    npcb->snd_wnd = tcphdr->wnd;
    npcb->ssthresh = npcb->snd_wnd;
    npcb->snd_wl1 = seqno;
    npcb->callback_arg = pcb->callback_arg;
    npcb->accept = pcb->accept;
    
    DEBUG_TCPIN
      kprintf(KR_OSTREAM,"npcb lp = %d, rp = %d, lip =%x, rip = %x",
	      npcb->local_port,npcb->remote_port,
	      npcb->local_ip,npcb->remote_ip);
    /* Register the new PCB so that we can begin receiving 
     * segments for it. */
    TCP_REG(&tcp_active_pcbs, npcb);
    
    /* Parse any options in the SYN. */
    tcp_parseopt(npcb);
    
    /* Build an MSS option. */
    optdata = htonl(((uint32_t)2 << 24) | 
		    ((uint32_t)4 << 16) | 
		    (((uint32_t)npcb->mss / 256) << 8) |
		    (npcb->mss & 255));
    /* Send a SYN|ACK together with the MSS option. */
    tcp_enqueue(npcb, NULL, 0, TCP_SYN | TCP_ACK, 0, (uint8_t *)&optdata, 4);
    return tcp_output(npcb);
  }
  return RC_OK;
}


/* Called by tcp_input() when a segment arrives for a connection 
 * in TIME_WAIT.
 */
static uint32_t
tcp_timewait_input(struct tcp_pcb *pcb)
{
  if (TCP_SEQ_GT(seqno + tcplen, pcb->rcv_nxt)) {
    pcb->rcv_nxt = seqno + tcplen;
  }
  if (tcplen > 0) {
    tcp_ack_now(pcb);
  }
  return tcp_output(pcb);
}


/* 
 * Implements the TCP state machine. Called by tcp_input. In some
 * states tcp_receive() is called to receive data. The tcp_seg
 * argument will be freed by the caller (tcp_input()) unless the
 * recv_data pointer in the pcb is set.
 */
static uint32_t
tcp_process(struct tcp_pcb *pcb)
{
  struct tcp_seg *rseg;
  uint8_t acceptable = 0;
  uint32_t err;
  
  err = RC_OK;
  
  /* Process incoming RST segments. */
  if (flags & TCP_RST) {
    /* First, determine if the reset is acceptable. */
    if (pcb->state == SYN_SENT) {
      if (ackno == pcb->snd_nxt) {
	acceptable = 1;
      }
    } else {
      if (TCP_SEQ_GEQ(seqno, pcb->rcv_nxt) &&
	  TCP_SEQ_LEQ(seqno, pcb->rcv_nxt + pcb->rcv_wnd)) {
	acceptable = 1;
      }
    }
    
    if (acceptable) {
      kprintf(KR_OSTREAM,"tcp_process: Connection RESET on %d",pcb->ssid);
      ERIP_ASSERT("tcp_input: pcb->state != CLOSED", pcb->state != CLOSED);
      recv_flags = TF_RESET;
      pcb->flags &= ~TF_ACK_DELAY;
      return ERR_RST;
    } else {
      kprintf(KR_OSTREAM,"tcp_process: unacceptable reset seqno %d rcv_nxt %d",
	     seqno, pcb->rcv_nxt);
      return RC_OK;
    }
  }

  /* Update the PCB (in)activity timer. */
  pcb->tmr = tcp_ticks;
  
  /* Do different things depending on the TCP state. */
  switch (pcb->state) {
  case SYN_SENT:
    DEBUG_TCPIN
      kprintf(KR_OSTREAM,"SYN-SENT: ackno %d pcb->snd_nxt %d unacked %d",ackno,
	      pcb->snd_nxt, ntohl(pcb->unacked->tcphdr->seqno));
    if (flags & (TCP_ACK | TCP_SYN) &&
      ackno == ntohl(pcb->unacked->tcphdr->seqno) + 1) {
      pcb->rcv_nxt = seqno + 1;
      pcb->lastack = ackno;
      pcb->snd_wnd = pcb->snd_wl1 = tcphdr->wnd;
      pcb->state = ESTABLISHED;
      pcb->cwnd = pcb->mss;
      --pcb->snd_queuelen;
      DEBUG_TCPIN kprintf(KR_OSTREAM,"tcp_process: SYN-SENT --queuelen %d", 
			  pcb->snd_queuelen);
      rseg = pcb->unacked;
      pcb->unacked = rseg->next;
      tcp_seg_free(rseg);

      /* Parse any options in the SYNACK. */
      tcp_parseopt(pcb);

      /* Call the user specified function to call when sucessfully
	 connected. */
      TCP_EVENT_CONNECTED(pcb, RC_OK, err);
      tcp_ack(pcb);
    }    
    break;
  case SYN_RCVD:
    if (flags & TCP_ACK &&
	!(flags & TCP_RST)) {
      if (TCP_SEQ_LT(pcb->lastack, ackno) &&
	  TCP_SEQ_LEQ(ackno, pcb->snd_nxt)) {
        pcb->state = ESTABLISHED;
	
	STATS{
	  global_counter = rdtsc();
	  rdpmc(0x0,user_counterHi,user_counterLo);
	  total_interrupts = 0;
	}
	
        DEBUG_TCPIN
	  kprintf(KR_OSTREAM,"TCP connection established %d -> %d", 
		  inseg.tcphdr->src, inseg.tcphdr->dest);
	ERIP_ASSERT("pcb->accept != NULL", pcb->accept != NULL);
	/* Call the accept function. */
	TCP_EVENT_ACCEPT(pcb, RC_OK, err);
	if (err != RC_OK) {
	  /* If the accept function returns with an error, we abort
	   * the connection. */
	  kprintf(KR_OSTREAM,"Connection aborting %d",err);
	  tcp_abort(pcb);
	  return ERR_ABRT;
	}	
	/* If there was any data contained within this ACK,
	 * we'd better pass it on to the application as well. */
	tcp_receive(pcb);
	pcb->cwnd = pcb->mss;
      }	
    }  
    break;
  case CLOSE_WAIT:
    /* FALLTHROUGH */
  case ESTABLISHED:
    tcp_receive(pcb);	  
    if (flags & TCP_FIN) {
      STATS{ 
	uint32_t Hi, Lo;
	static bool constructed = 0;
	
	if(!constructed) 
	  /* Construct the console domain */
	  constructor_request(KR_CONSTREAM,KR_BANK,KR_SCHED,KR_VOID,
			      KR_CONSTREAM);
	constructed = 1;
	
	global_counter = rdtsc() - global_counter;
	rdpmc(0x0,Hi,Lo);
	
	kprintf(KR_OSTREAM,"Total time = %16x",global_counter);
	kprintf(KR_OSTREAM," Hi=%08x  Lo = %08x",Hi,Lo);
	kprintf(KR_OSTREAM," OldHi=%08x  OldLo = %08x",user_counterHi,
		user_counterLo);
	user_counterHi = Hi - user_counterHi;
	user_counterLo = Lo - user_counterLo;
	
	{
	  char buf1[20], buf2[20] , buf3[20];
	  int i;
	  
	  memset(&buf1[0],0,20);
	  memset(&buf2[0],0,20);
	  memset(&buf3[0],0,20);
	  
	  sprintf(buf1,"%16x",global_counter);
	  sprintf(buf2,"%16x",user_counterHi);
	  sprintf(buf3," %d",total_interrupts);
	  for (i = 0; i < 20 && buf1[i] != '\0'; i++) {
	    (void) capros_Stream_write(KR_CONSTREAM, buf1[i]);
	    if(buf1[i] == '\r') capros_Stream_write(KR_CONSTREAM,'\n');
	  }
	  capros_Stream_write(KR_CONSTREAM,'\n');
	  
	  for (i = 0; i < 20 && buf2[i]!='\0'; i++) {
	    (void) capros_Stream_write(KR_CONSTREAM, buf2[i]);
	    if(buf2[i] == '\r') capros_Stream_write(KR_CONSTREAM,'\n');
	  }
	  
	  capros_Stream_write(KR_CONSTREAM,'\n');
	  for (i = 0; i < 20 && buf3[i]!='\0'; i++) {
	    (void) capros_Stream_write(KR_CONSTREAM, buf3[i]);
	    if(buf3[i] == '\r') capros_Stream_write(KR_CONSTREAM,'\n');
	  }
	  capros_Stream_write(KR_CONSTREAM,'\n');
	}
      }
      tcp_ack_now(pcb);
      pcb->state = CLOSE_WAIT;
    }
    break;
  case FIN_WAIT_1:
    tcp_receive(pcb);
    if (flags & TCP_FIN) {
      if (flags & TCP_ACK && ackno == pcb->snd_nxt) {
        DEBUG_TCPIN 
	  kprintf(KR_OSTREAM,"TCP connection closed %d -> %d.", 
		  inseg.tcphdr->src, inseg.tcphdr->dest);
	tcp_ack_now(pcb);
	tcp_pcb_purge(pcb);
	TCP_RMV(&tcp_active_pcbs, pcb);
	pcb->state = TIME_WAIT;
	TCP_REG(&tcp_tw_pcbs, pcb);
      } else {
	tcp_ack_now(pcb);
	pcb->state = CLOSING;
      }
    } else if (flags & TCP_ACK && ackno == pcb->snd_nxt) {
      pcb->state = FIN_WAIT_2;
    }
    break;
  case FIN_WAIT_2:
    tcp_receive(pcb);
    if (flags & TCP_FIN) {
      DEBUG_TCPIN kprintf(KR_OSTREAM,"TCP connection closed %d -> %d", 
			  inseg.tcphdr->src, inseg.tcphdr->dest);
      tcp_ack_now(pcb);
      tcp_pcb_purge(pcb);
      TCP_RMV(&tcp_active_pcbs, pcb);
      pcb->state = TIME_WAIT;
      TCP_REG(&tcp_tw_pcbs, pcb);
    }
    break;
  case CLOSING:
    tcp_receive(pcb);
    if (flags & TCP_ACK && ackno == pcb->snd_nxt) {
      DEBUG_TCPIN  kprintf(KR_OSTREAM,"TCP connection closed %d -> %d",
			   inseg.tcphdr->src, inseg.tcphdr->dest);
      tcp_ack_now(pcb);
      tcp_pcb_purge(pcb);
      TCP_RMV(&tcp_active_pcbs, pcb);
      pcb->state = TIME_WAIT;
      TCP_REG(&tcp_tw_pcbs, pcb);
    }
    break;
  case LAST_ACK:
    tcp_receive(pcb);
    if (flags & TCP_ACK && ackno == pcb->snd_nxt) {
      DEBUG_TCPIN kprintf(KR_OSTREAM,"TCP connection closed %d -> %d", 
			  inseg.tcphdr->src, inseg.tcphdr->dest);
      pcb->state = CLOSED;
      recv_flags = TF_CLOSED;
    }
    break;
  default:
    break;
  }
  
  return RC_OK;
}

/* 
 * Called by tcp_process. Checks if the given segment is an ACK for 
 * outstanding data, and if so frees the memory of the buffered data. 
 * Next, is places the segment on any of the receive queues (pcb->recved
 * or pcb->ooseq). If the segment is buffered, the pbuf is referenced by 
 * pbuf_ref so that it will not be freed until it has been removed from
 * the buffer.If the incoming segment constitutes an ACK for a segment 
 * that was used for RTT estimation, the RTT is estimated here as well.
 */
static void
tcp_receive(struct tcp_pcb *pcb)
{
  struct tcp_seg *next;
  struct tcp_seg *prev, *cseg;
  struct pbuf *p;
  int32_t off;
  int m;
  uint32_t right_wnd_edge;

  if (flags & TCP_ACK) {
    right_wnd_edge = pcb->snd_wnd + pcb->snd_wl1;
    
    /* Update window. */
    if (TCP_SEQ_LT(pcb->snd_wl1, seqno) ||
	(pcb->snd_wl1 == seqno && TCP_SEQ_LT(pcb->snd_wl2, ackno)) ||
	(pcb->snd_wl2 == ackno && tcphdr->wnd > pcb->snd_wnd)) {
      pcb->snd_wnd = tcphdr->wnd;
      pcb->snd_wl1 = seqno;
      pcb->snd_wl2 = ackno;
      DEBUG_TCPIN 
	kprintf(KR_OSTREAM,"tcp_receive:window update %d", pcb->snd_wnd);
    } else {
      DEBUG_TCPIN if (pcb->snd_wnd != tcphdr->wnd) {
	kprintf(KR_OSTREAM,"tcp_receive: no window update lastack %d\
                           snd_max %d ackno %d wl1 %lu seqno %d wl2 %d\n",
		pcb->lastack, pcb->snd_max, ackno, 
		pcb->snd_wl1, seqno, pcb->snd_wl2);
      }
    }
    
    if (pcb->lastack == ackno) {
      pcb->acked = 0;
      
      if (pcb->snd_wl1 + pcb->snd_wnd == right_wnd_edge){
	++pcb->dupacks;
	if (pcb->dupacks >= 3 && pcb->unacked != NULL) {
	  if (!(pcb->flags & TF_INFR)) {
	    /* This is fast retransmit.Rexmit the first unacked segment.*/
	    DEBUG_TCPIN
	      kprintf(KR_OSTREAM,"tcp_receive:dupacks %d (%d),fast rexmit %d",
		      pcb->dupacks, pcb->lastack,
		      ntohl(pcb->unacked->tcphdr->seqno));
	    tcp_rexmit(pcb);
	    /* Set ssthresh to max (FlightSize / 2, 2*SMSS) */
	    pcb->ssthresh = ERIP_MAX((pcb->snd_max - pcb->lastack) / 2,
				     2 * pcb->mss);
	    
	    pcb->cwnd = pcb->ssthresh + 3 * pcb->mss;
	    pcb->flags |= TF_INFR;          
	  } else {
	    /* Inflate the congestion window, but not if it means that
	     * the value overflows. */
	    if ((uint16_t)(pcb->cwnd + pcb->mss) > pcb->cwnd) {
	      pcb->cwnd += pcb->mss;
	    }
	  }
	}
      } else {
	DEBUG_TCPIN
	  kprintf(KR_OSTREAM,"tcp_receive: dupack averted %d %d\n",
		  pcb->snd_wl1 + pcb->snd_wnd, right_wnd_edge);
      }
    } else if (TCP_SEQ_LT(pcb->lastack, ackno) &&
	       TCP_SEQ_LEQ(ackno, pcb->snd_max)) {
      /* We come here when the ACK acknowledges new data. */
      
      /* Reset the "IN Fast Retransmit" flag, since we are no longer
       * in fast retransmit. Also reset the congestion window to the
       * slow start threshold. */
      if (pcb->flags & TF_INFR) {
	pcb->flags &= ~TF_INFR;
	pcb->cwnd = pcb->ssthresh;
      }

      /* Reset the number of retransmissions. */
      pcb->nrtx = 0;
      
      /* Reset the retransmission time-out. */
      pcb->rto = (pcb->sa >> 3) + pcb->sv;
      
      /* Update the send buffer space. */
      pcb->acked = ackno - pcb->lastack;
      pcb->snd_buf += pcb->acked;
      
      /* Reset the fast retransmit variables. */
      pcb->dupacks = 0;
      pcb->lastack = ackno;
      
      /* Update the congestion control variables (cwnd and
       * ssthresh). */
      if (pcb->state >= ESTABLISHED) {
        if (pcb->cwnd < pcb->ssthresh) {
	  if ((uint16_t)(pcb->cwnd + pcb->mss) > pcb->cwnd) {
	    pcb->cwnd += pcb->mss;
	  }
          DEBUG_TCPIN
	    kprintf(KR_OSTREAM,"tcp_receive: slow start cwnd %d",pcb->cwnd);
        } else {
	  uint16_t new_cwnd = (pcb->cwnd + pcb->mss * pcb->mss / pcb->cwnd);
	  if (new_cwnd > pcb->cwnd) {
	    pcb->cwnd = new_cwnd;
	  }
          DEBUG_TCPIN
	    kprintf(KR_OSTREAM,"tcp_receive: congestion avoidance cwnd %d", 
		    pcb->cwnd); 
        }
      }
      DEBUG_TCPIN
	kprintf(KR_OSTREAM,"tcp_receive: ACK for %d, unacked->seqno %d:%d",
		ackno,
		pcb->unacked != NULL?
		ntohl(pcb->unacked->tcphdr->seqno): 0,
		pcb->unacked != NULL?
		ntohl(pcb->unacked->tcphdr->seqno)+TCP_TCPLEN(pcb->unacked):0);
      
      /* Remove segment from the unacknowledged list if the incoming
       * ACK acknowlegdes them. */
      while (pcb->unacked != NULL && 
	     TCP_SEQ_LEQ(ntohl(pcb->unacked->tcphdr->seqno) +
			 TCP_TCPLEN(pcb->unacked), ackno)) {
	DEBUG_TCPIN
	  kprintf(KR_OSTREAM,"tcp_receive: removing %u:%u from pcb->unacked",
		  ntohl(pcb->unacked->tcphdr->seqno),
		  ntohl(pcb->unacked->tcphdr->seqno) +
		  TCP_TCPLEN(pcb->unacked));
	
	next = pcb->unacked;
	pcb->unacked = pcb->unacked->next;
	
	DEBUG_TCPIN
	  kprintf(KR_OSTREAM,"tcp_receive: queuelen %d ..",pcb->snd_queuelen);
	pcb->snd_queuelen -= pbuf_clen(next->p);
	tcp_seg_free(next);
	
	DEBUG_TCPIN
	  kprintf(KR_OSTREAM,"%d (after freeing unacked)",pcb->snd_queuelen);
	if (pcb->snd_queuelen != 0) {
	  ERIP_ASSERT("tcp_receive: valid queue length", 
		      pcb->unacked != NULL ||
		      pcb->unsent != NULL);      
	}
      }
      pcb->polltmr = 0;
    }

    /* We go through the ->unsent list to see if any of the segments
     * on the list are acknowledged by the ACK. This may seem
     * strange since an "unsent" segment shouldn't be acked. The
     * rationale is that lwip puts all outstanding segments on the
     * ->unsent list after a retransmission, so these segments may
     * in fact have been sent once. */
    while (pcb->unsent != NULL && 
	   TCP_SEQ_LEQ(ntohl(pcb->unsent->tcphdr->seqno) + 
		       TCP_TCPLEN(pcb->unsent), ackno) &&
	   TCP_SEQ_LEQ(ackno, pcb->snd_max)) {
      DEBUG_TCPIN
	kprintf(KR_OSTREAM,"tcp_receive: removing %u:%u from pcb->unsent",
		ntohl(pcb->unsent->tcphdr->seqno),
		ntohl(pcb->unsent->tcphdr->seqno) +
		TCP_TCPLEN(pcb->unsent));
      
      next = pcb->unsent;
      pcb->unsent = pcb->unsent->next;
      DEBUGF(TCP_QLEN_DEBUG, ("tcp_receive: queuelen %d ... ", 
			      pcb->snd_queuelen));
      pcb->snd_queuelen -= pbuf_clen(next->p);
      tcp_seg_free(next);
      DEBUGF(TCP_QLEN_DEBUG, ("%d (after freeing unsent)\n", 
			      pcb->snd_queuelen));
      if (pcb->snd_queuelen != 0) {
	ERIP_ASSERT("tcp_receive: valid queue length", 
		    pcb->unacked != NULL || pcb->unsent != NULL);      
      }
      
      if (pcb->unsent != NULL) {
	pcb->snd_nxt = htonl(pcb->unsent->tcphdr->seqno);
      }
    }
      
    /* End of ACK for new data processing. */
    DEBUGF(TCP_RTO_DEBUG, ("tcp_receive: pcb->rttest %d rtseq %lu ackno %lu\n",
			   pcb->rttest, pcb->rtseq, ackno));
    
    /* RTT estimation calculations. This is done by checking if the
     * incoming segment acknowledges the segment we use to take a
     * round-trip time measurement. */
    if (pcb->rttest && TCP_SEQ_LT(pcb->rtseq, ackno)) {
      m = tcp_ticks - pcb->rttest;
      
      DEBUGF(TCP_RTO_DEBUG,("tcp_receive: experienced rtt %d ticks(%d msec)",
			    m, m * TCP_SLOW_INTERVAL));

      /* This is taken directly from VJs original code in his paper */      
      m = m - (pcb->sa >> 3);
      pcb->sa += m;
      if (m < 0) {
	m = -m;
      }
      m = m - (pcb->sv >> 2);
      pcb->sv += m;
      pcb->rto = (pcb->sa >> 3) + pcb->sv;
      
      DEBUG_TCPIN kprintf(KR_OSTREAM,"tcp_receive: RTO %d (%d miliseconds)",
			     pcb->rto, pcb->rto * TCP_SLOW_INTERVAL);
      
      pcb->rttest = 0;
    } 
  }
  
  /* If the incoming segment contains data, we must process it further. */
  if (tcplen > 0) {
    /* This code basically does three things:
       +) If the incoming segment contains data that is the next
       in-sequence data, this data is passed to the application. This
       might involve trimming the first edge of the data. The rcv_nxt
       variable and the advertised window are adjusted.       
	
       +) If the incoming segment has data that is above the next
       sequence number expected (->rcv_nxt), the segment is placed on
       the ->ooseq queue. This is done by finding the appropriate
       place in the ->ooseq queue (which is ordered by sequence
       number) and trim the segment in both ends if needed. An
       immediate ACK is sent to indicate that we received an
       out-of-sequence segment.
	
       +) Finally, we check if the first segment on the ->ooseq queue
       now is in sequence (i.e., if rcv_nxt >= ooseq->seqno). If
       rcv_nxt > ooseq->seqno, we must trim the first edge of the
       segment on ->ooseq before we adjust rcv_nxt. The data in the
       segments that are now on sequence are chained onto the
       incoming segment so that we only need to call the application
       once.
    */
    
    /* First, we check if we must trim the first edge. We have to do
     * this if the sequence number of the incoming segment is less
     * than rcv_nxt, and the sequence number plus the length of the
     * segment is larger than rcv_nxt. */
    if (TCP_SEQ_LT(seqno, pcb->rcv_nxt)){
      if (TCP_SEQ_LT(pcb->rcv_nxt, seqno + tcplen)) {
	/* Trimming the first edge is done by pushing the payload
	   pointer in the pbuf downwards. This is somewhat tricky since
	   we do not want to discard the full contents of the pbuf up to
	   the new starting point of the data since we have to keep the
	   TCP header which is present in the first pbuf in the chain.
	   
	   What is done is really quite a nasty hack: the first pbuf in
	   the pbuf chain is pointed to by inseg.p. Since we need to be
	   able to deallocate the whole pbuf, we cannot change this
	   inseg.p pointer to point to any of the later pbufs in the
	   chain. Instead, we point the ->payload pointer in the first
	   pbuf to data in one of the later pbufs. We also set the
	   inseg.data pointer to point to the right place. This way, the
	   ->p pointer will still point to the first pbuf, but the
	   ->p->payload pointer will point to data in another pbuf.
	   
	   After we are done with adjusting the pbuf pointers we must
	   adjust the ->data pointer in the seg and the segment
	   length.*/
	off = pcb->rcv_nxt - seqno;
	if (inseg.p->len < off) {
	  p = inseg.p;
	  while (p->len < off) {
	    off -= p->len;
	    inseg.p->tot_len -= p->len;
	    p->len = 0;
	    p = p->next;
	  }
	  pbuf_header(p, -off);
	} else {
	  pbuf_header(inseg.p, -off);
	}
	inseg.dataptr = inseg.p->payload;
	inseg.len -= pcb->rcv_nxt - seqno;      
	inseg.tcphdr->seqno = seqno = pcb->rcv_nxt;
      }
      else{
	/* the whole segment is < rcv_nxt */
	/* must be a duplicate of a packet that has already been 
	 * correctly handled */
	DEBUG_TCPIN
	  kprintf(KR_OSTREAM,"tcp_receive: duplicate seqno %d\n", seqno);
	tcp_ack_now(pcb);
      }
    }

    /* The sequence number must be within the window (above rcv_nxt
       and below rcv_nxt + rcv_wnd) in order to be further
       processed. */
    if (TCP_SEQ_GEQ(seqno, pcb->rcv_nxt) &&
	TCP_SEQ_LT(seqno, pcb->rcv_nxt + pcb->rcv_wnd)) {
      if (pcb->rcv_nxt == seqno) {			
	/* The incoming segment is the next in sequence. We check if
           we have to trim the end of the segment and update rcv_nxt
           and pass the data to the application. */
	if (pcb->ooseq != NULL &&
	    TCP_SEQ_LEQ(pcb->ooseq->tcphdr->seqno, seqno + inseg.len)) {
	  /* We have to trim the second edge of the incoming
             segment. */
	  inseg.len = pcb->ooseq->tcphdr->seqno - seqno;
	  pbuf_realloc(inseg.p, inseg.len);
	}

	tcplen = TCP_TCPLEN(&inseg);
	
	pcb->rcv_nxt += tcplen;
	
	/* Update the receiver's (our) window. */
	if (pcb->rcv_wnd < tcplen) {
	  pcb->rcv_wnd = 0;
	} else {
	  pcb->rcv_wnd -= tcplen;
	}
	
	/* If there is data in the segment, we make preparations to
	   pass this up to the application. The ->recv_data variable
	   is used for holding the pbuf that goes to the
	   application. The code for reassembling out-of-sequence data
	   chains its data on this pbuf as well.
	   
	   If the segment was a FIN, we set the TF_GOT_FIN flag that will
	   be used to indicate to the application that the remote side has
	   closed its end of the connection. */      
	if (inseg.p->tot_len > 0) {
	  recv_data = inseg.p;
	  /* Since this pbuf now is the responsibility of the
	     application, we delete our reference to it so that we won't
	     (mistakingly) deallocate it. */
	  inseg.p = NULL;
	}
	if (TCPH_FLAGS(inseg.tcphdr) & TCP_FIN) {
	  DEBUGF(TCP_INPUT_DEBUG, ("tcp_receive: received FIN."));
	  recv_flags = TF_GOT_FIN;
	}
	
	/* We now check if we have segments on the ->ooseq queue that
           is now in sequence. */
	while (pcb->ooseq != NULL &&
	       pcb->ooseq->tcphdr->seqno == pcb->rcv_nxt) {

	  cseg = pcb->ooseq;
	  seqno = pcb->ooseq->tcphdr->seqno;
	  
	  pcb->rcv_nxt += TCP_TCPLEN(cseg);
	  if (pcb->rcv_wnd < TCP_TCPLEN(cseg)) {
	    pcb->rcv_wnd = 0;
	  } else {
	    pcb->rcv_wnd -= TCP_TCPLEN(cseg);
	  }
	  if (cseg->p->tot_len > 0) {
	    /* Chain this pbuf onto the pbuf that we will pass to
	       the application. */
	    if (recv_data) {
              pbuf_chain(recv_data, cseg->p);
              pbuf_free(cseg->p);
            } else {
	      recv_data = cseg->p;
	    }
	    cseg->p = NULL;
	  }
	  if (flags & TCP_FIN) {
	    DEBUGF(TCP_INPUT_DEBUG, ("tcp_receive: dequeued FIN."));
	    recv_flags = TF_GOT_FIN;
	  }	    
	  

	  pcb->ooseq = cseg->next;
	  tcp_seg_free(cseg);
	}

	/* Acknowledge the segment(s). */
	tcp_ack(pcb);

      } else {
	/* We get here if the incoming segment is out-of-sequence. */
	tcp_ack_now(pcb);
	/* We queue the segment on the ->ooseq queue. */
	if (pcb->ooseq == NULL) {
	  pcb->ooseq = tcp_seg_copy(&inseg);
	} else {
	  /* If the queue is not empty, we walk through the queue and
	     try to find a place where the sequence number of the
	     incoming segment is between the sequence numbers of the
	     previous and the next segment on the ->ooseq queue. That is
	     the place where we put the incoming segment. If needed, we
	     trim the second edges of the previous and the incoming
	     segment so that it will fit into the sequence.

	     If the incoming segment has the same sequence number as a
	     segment on the ->ooseq queue, we discard the segment that
	     contains less data. */

	  prev = NULL;
	  for(next = pcb->ooseq; next != NULL; next = next->next) {
	    if (seqno == next->tcphdr->seqno) {
	      /* The sequence number of the incoming segment is the
                 same as the sequence number of the segment on
                 ->ooseq. We check the lengths to see which one to
                 discard. */
	      if (inseg.len > next->len) {
		/* The incoming segment is larger than the old
                   segment. We replace the old segment with the new
                   one. */
		cseg = tcp_seg_copy(&inseg);
		if (cseg != NULL) {
		  cseg->next = next->next;
		  if (prev != NULL) {
		    prev->next = cseg;
		  } else {
		    pcb->ooseq = cseg;
		  }
		}
		break;
	      } else {
		/* Either the lenghts are the same or the incoming
                   segment was smaller than the old one; in either
                   case, we ditch the incoming segment. */
		break;
	      } 
	    } else {
	      if (prev == NULL) {
		if (TCP_SEQ_LT(seqno, next->tcphdr->seqno)) {
		  /* The sequence number of the incoming segment is lower
		     than the sequence number of the first segment on the
		     queue. We put the incoming segment first on the
		     queue. */
		  
		  if (TCP_SEQ_GT(seqno + inseg.len, next->tcphdr->seqno)) {
		    /* We need to trim the incoming segment. */
		    inseg.len = next->tcphdr->seqno - seqno;
		    pbuf_realloc(inseg.p, inseg.len);
		  }
		  cseg = tcp_seg_copy(&inseg);
		  if (cseg != NULL) {
		    cseg->next = next;
		    pcb->ooseq = cseg;
		  }
		  break;
		}
	      } else if (TCP_SEQ_LT(prev->tcphdr->seqno, seqno) &&
			 TCP_SEQ_LT(seqno, next->tcphdr->seqno)) {
		/* The sequence number of the incoming segment is in
                   between the sequence numbers of the previous and
                   the next segment on ->ooseq. We trim and insert the
                   incoming segment and trim the previous segment, if
                   needed. */
		if (TCP_SEQ_GT(seqno + inseg.len, next->tcphdr->seqno)) {
		  /* We need to trim the incoming segment. */
		  inseg.len = next->tcphdr->seqno - seqno;
		  pbuf_realloc(inseg.p, inseg.len);
		}

		cseg = tcp_seg_copy(&inseg);
		if (cseg != NULL) {		 		  
		  cseg->next = next;
		  prev->next = cseg;
		  if (TCP_SEQ_GT(prev->tcphdr->seqno + prev->len, seqno)) {
		    /* We need to trim the prev segment. */
		    prev->len = seqno - prev->tcphdr->seqno;
		    pbuf_realloc(prev->p, prev->len);
		  }
		}
		break;
	      }
	      /* If the "next" segment is the last segment on the
                 ooseq queue, we add the incoming segment to the end
                 of the list. */
	      if (next->next == NULL &&
		  TCP_SEQ_GT(seqno, next->tcphdr->seqno)) {
		next->next = tcp_seg_copy(&inseg);
		if (next->next != NULL) {		
		  if (TCP_SEQ_GT(next->tcphdr->seqno + next->len, seqno)) {
		    /* We need to trim the last segment. */
		    next->len = seqno - next->tcphdr->seqno;
		    pbuf_realloc(next->p, next->len);
		  }
		}
		break;
	      }
	    }
	    prev = next;
	  }    
	} 
      }    
    }
  } else {
    /* Segments with length 0 is taken care of here. Segments that
       fall out of the window are ACKed. */
    if (TCP_SEQ_GT(pcb->rcv_nxt, seqno) ||
	TCP_SEQ_GEQ(seqno, pcb->rcv_nxt + pcb->rcv_wnd)) {
      tcp_ack_now(pcb);
    }      
  }
#if 0
  {
    uint32_t Hi,Lo,cs;
    process_get_trace(KR_SELF,&Hi,&Lo,&cs);
    kprintf(KR_OSTREAM,"<<<<<<<<<Returning In = %d ##### %d:%d:%d",
	    rdtsc(),Hi,Lo,cs);
  }
#endif
}


/*
 * Parses the options contained in the incoming segment. (Code taken
 * from uIP with only small changes.)
 */
static void
tcp_parseopt(struct tcp_pcb *pcb)
{
  uint8_t c;
  uint8_t *opts, opt;
  uint16_t mss;

  opts = (uint8_t *)tcphdr + TCP_HLEN;
  
  /* Parse the TCP MSS option, if present. */
  if ((TCPH_OFFSET(tcphdr) & 0xf0) > 0x50) {
    for(c = 0; c < ((TCPH_OFFSET(tcphdr) >> 4) - 5) << 2 ;) {
      opt = opts[c];
      if (opt == 0x00) {
        /* End of options. */   
	break;
      } else if (opt == 0x01) {
        ++c;
        /* NOP option. */
      } else if (opt == 0x02 &&
		 opts[c + 1] == 0x04) {
        /* An MSS option with the right option length. */       
        mss = (opts[c + 2] << 8) | opts[c + 3];
        pcb->mss = mss > TCP_MSS? TCP_MSS: mss;
        
        /* And we are done processing options. */
        break;
      } else {
	if (opts[c + 1] == 0) {
          /* If the length field is zero, the options are malformed
             and we don't process them further. */
          break;
        }
        /* All other options have a length field, so that we easily
           can skip past them. */
        c += opts[c + 1];
      }      
    }
  }
}
