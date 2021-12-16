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
/* tcp.c
 *
 * This file contains common functions for the TCP implementation, 
 * such as functions for manipulating the data structures and the TCP
 * timer functions. TCP functions related to input and output is found 
 * in tcp_input.c and tcp_output.c respectively.
 */

#include <stddef.h>
#include <string.h>
#include <eros/target.h>
#include <eros/endian.h>
#include <eros/Invoke.h>

#include "netsyskeys.h"

#include <domain/domdbg.h>

#include "include/def.h"
#include "include/mem.h"
#include "include/memp.h"
#include "include/tcp.h"
#include "include/opt.h" 
#include "include/err.h" 

#define DEBUG_TCP if(0) 

/* Incremented every coarse grained timer shot
 * (typically every 500 ms, determined by TCP_COARSE_TIMEOUT). */
uint32_t tcp_ticks;
const uint8_t tcp_backoff[13] =
  { 1, 2, 3, 4, 5, 6, 7, 7, 7, 7, 7, 7, 7};

/* The TCP PCB lists. */
struct tcp_pcb_listen *tcp_listen_pcbs;  /* List of all TCP PCBs in \
					  * LISTEN state. */
struct tcp_pcb *tcp_active_pcbs;         /* List of all TCP PCBs that are \
					  * in a state in which they accept 
					  * or send data. */
struct tcp_pcb *tcp_tw_pcbs;     /* List of all TCP PCBs in TIME-WAIT. */

struct tcp_pcb *tcp_tmp_pcb;

static uint8_t tcp_timer;

static uint16_t tcp_new_port(void);


/* Initializes the TCP layer.*/
void
tcp_init(void)
{
  /* Clear globals. */
  tcp_listen_pcbs = NULL;
  tcp_active_pcbs = NULL;
  tcp_tw_pcbs = NULL;
  tcp_tmp_pcb = NULL;
  
  /* initialize timer */
  tcp_ticks = 0;
  tcp_timer = 0;
}

/* Closes the connection held by the PCB. */
uint32_t
tcp_close(struct tcp_pcb *pcb)
{
  uint32_t err;

#if TCP_DEBUG
  DEBUGF(TCP_DEBUG, ("tcp_close: closing in state "));
  tcp_debug_print_state(pcb->state);
  DEBUGF(TCP_DEBUG, ("\n"));
#endif /* TCP_DEBUG */
  switch (pcb->state) {
  case LISTEN:
    err = RC_OK;
    tcp_pcb_remove((struct tcp_pcb **)&tcp_listen_pcbs, pcb);
    memp_free(MEMP_TCP_PCB_LISTEN, pcb);
    pcb = NULL;
    break;
  case SYN_SENT:
    err = RC_OK;
    tcp_pcb_remove(&tcp_active_pcbs, pcb);
    memp_free(MEMP_TCP_PCB, pcb);
    pcb = NULL;
    break;
  case SYN_RCVD:
    err = tcp_send_ctrl(pcb, TCP_FIN);
    if (err == RC_OK) {
      pcb->state = FIN_WAIT_1;
    }
    break;
  case ESTABLISHED:
    err = tcp_send_ctrl(pcb, TCP_FIN);
    if (err == RC_OK) {
      pcb->state = FIN_WAIT_1;
    }
    break;
  case CLOSE_WAIT:
    err = tcp_send_ctrl(pcb, TCP_FIN);
    if (err == RC_OK) {
      pcb->state = LAST_ACK;
    }
    break;
  default:
    /* Has already been closed, do nothing. */
    err = RC_OK;
    pcb = NULL;
    break;
  }

  if (pcb != NULL && err == RC_OK) {
    err = tcp_output(pcb);
  }
  return err;
}


/* Aborts a connection by sending a RST to the remote host and deletes
 * the local protocol control block. This is done when a connection is
 * killed because of shortage of memory.
 */
void
tcp_abort(struct tcp_pcb *pcb)
{
  uint32_t seqno, ackno;
  uint16_t remote_port, local_port;
  struct ip_addr remote_ip, local_ip;
  void (* errf)(void *arg, uint32_t err);
  void *errf_arg;

  
  /* Figure out on which TCP PCB list we are, and remove us. If we
     are in an active state, call the receive function associated with
     the PCB with a NULL argument, and send an RST to the remote end. */
  if (pcb->state == TIME_WAIT) {
    tcp_pcb_remove(&tcp_tw_pcbs, pcb);
    memp_free(MEMP_TCP_PCB, pcb);
  } else {
    seqno = pcb->snd_nxt;
    ackno = pcb->rcv_nxt;
    ip_addr_set(&local_ip, &(pcb->local_ip));
    ip_addr_set(&remote_ip, &(pcb->remote_ip));
    local_port = pcb->local_port;
    remote_port = pcb->remote_port;
    errf = pcb->errf;
    errf_arg = pcb->callback_arg;
    tcp_pcb_remove(&tcp_active_pcbs, pcb);
    if (pcb->unacked != NULL) {
      tcp_segs_free(pcb->unacked,pcb->ssid);
    }
    if (pcb->unsent != NULL) {
      tcp_segs_free(pcb->unsent,pcb->ssid);
    }
#if TCP_QUEUE_OOSEQ    
    if (pcb->ooseq != NULL) {
      tcp_segs_free(pcb->ooseq,pcb->ssid);
    }
#endif /* TCP_QUEUE_OOSEQ */
    memp_free(MEMP_TCP_PCB, pcb);
    TCP_EVENT_ERR(errf, errf_arg, ERR_ABRT);
    DEBUGF(TCP_RST_DEBUG, ("tcp_abort: sending RST\n"));
    tcp_rst(seqno, ackno, &local_ip, &remote_ip, local_port, remote_port);
  }
}


/*
 * Binds the connection to a local portnumber and IP address. If the
 * IP address is not given (i.e., ipaddr == NULL), the IP address of
 * the outgoing network interface is used instead.
 */
uint32_t
tcp_bind(struct tcp_pcb *pcb, struct ip_addr *ipaddr, uint16_t port)
{
  struct tcp_pcb *cpcb;

  if (port == 0) {
    port = tcp_new_port();
  }
  
  /* Check if the address already is in use. */
  for(cpcb = (struct tcp_pcb *)tcp_listen_pcbs;
      cpcb != NULL; cpcb = cpcb->next) {
    if (cpcb->local_port == port) {
      if (ip_addr_isany(&(cpcb->local_ip)) ||
	  ip_addr_isany(ipaddr) ||
	  ip_addr_cmp(&(cpcb->local_ip), ipaddr)) {
	return ERR_USE;
      }
    }
  }
  for(cpcb = tcp_active_pcbs;
      cpcb != NULL; cpcb = cpcb->next) {
    if (cpcb->local_port == port) {
      if (ip_addr_isany(&(cpcb->local_ip)) ||
	  ip_addr_isany(ipaddr) ||
	  ip_addr_cmp(&(cpcb->local_ip), ipaddr)) {
	return ERR_USE;
      }
    }
  }
  if (!ip_addr_isany(ipaddr)) {
    pcb->local_ip = *ipaddr;
  }
  pcb->local_port = port;
  DEBUG_TCP kprintf(KR_OSTREAM,"tcp_bind: bind to port %d", port);
  
  /* register this pcb with the active pcbs */
  TCP_REG(&tcp_active_pcbs,pcb);
  return RC_OK;
}

#if 0
static uint32_t
tcp_accept_null(void *arg, struct tcp_pcb *pcb, uint32_t err)
{
  if (arg || pcb || err);
  return ERR_ABRT;
}
#endif

/* Set the state of the connection to be LISTEN, which means that it
 * is able to accept incoming connections. The protocol control block
 * is reallocated in order to consume less memory. Setting the
 * connection to LISTEN is an irreversible process.
 */
struct tcp_pcb *
tcp_listen(struct tcp_pcb *pcb)
{
  struct tcp_pcb_listen *lpcb;

  /* already listening? */
  if (pcb->state == LISTEN) {
    return pcb;
  }
  lpcb = memp_alloc(MEMP_TCP_PCB_LISTEN);
  if (lpcb == NULL) {
    return NULL;
  }
  kprintf(KR_OSTREAM,"listen pcb alloced at %08x",&lpcb[0]);
  
  lpcb->callback_arg = pcb->callback_arg;
  lpcb->local_port = pcb->local_port;
  lpcb->state = LISTEN;
  lpcb->ssid = pcb->ssid; /* Do not lose our ssid brand */
  ip_addr_set(&lpcb->local_ip, &pcb->local_ip);
  tcp_pcb_remove((struct tcp_pcb **)&tcp_active_pcbs, pcb);
  memp_free(MEMP_TCP_PCB, pcb);
  TCP_REG(&tcp_listen_pcbs, lpcb);
  return (struct tcp_pcb *)lpcb;
}


/*
 * tcp_recved():
 *
 * This function should be called by the application when it has
 * processed the data. The purpose is to advertise a larger window
 * when the data has been processed.
 *
 */
void
tcp_recved(struct tcp_pcb *pcb, uint16_t len)
{
  pcb->rcv_wnd += len;
  if (pcb->rcv_wnd > TCP_WND) {
    pcb->rcv_wnd = TCP_WND;
  }
  if (!(pcb->flags & TF_ACK_DELAY) &&
      !(pcb->flags & TF_ACK_NOW)) {
    tcp_ack(pcb);
  }
  DEBUG_TCP kprintf(KR_OSTREAM,"tcp_recved: received %d bytes, wnd %d (%d)",
		    len, pcb->rcv_wnd, TCP_WND - pcb->rcv_wnd);
}

/*
 * tcp_new_port():
 *
 * A nastly hack featuring 'goto' statements that allocates a
 * new TCP local port.
 */
static uint16_t
tcp_new_port(void)
{
  struct tcp_pcb *pcb;
#ifndef TCP_LOCAL_PORT_RANGE_START
#define TCP_LOCAL_PORT_RANGE_START 4096
#define TCP_LOCAL_PORT_RANGE_END   0x7fff
#endif
  static uint16_t port = TCP_LOCAL_PORT_RANGE_START;
  
 again:
  if (++port > TCP_LOCAL_PORT_RANGE_END) {
    port = TCP_LOCAL_PORT_RANGE_START;
  }
  
  for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
    if (pcb->local_port == port) {
      goto again;
    }
  }
  for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
    if (pcb->local_port == port) {
      goto again;
    }
  }
  for(pcb = (struct tcp_pcb *)tcp_listen_pcbs; pcb != NULL; pcb = pcb->next) {
    if (pcb->local_port == port) {
      goto again;
    }
  }
  return port;
}

/*
 * tcp_connect():
 *
 * Connects to another host. The function given as the "connected"
 * argument will be called when the connection has been established.
 *
 */
uint32_t
tcp_connect(struct tcp_pcb *pcb, struct ip_addr *ipaddr, uint16_t port,
	    uint32_t (* connected)(void *arg, struct tcp_pcb *tpcb, 
				   uint32_t err))
{
  uint32_t optdata;
  uint32_t ret;
  uint32_t iss;

  if (ipaddr != NULL) {
    pcb->remote_ip = *ipaddr;
  } else {
    return ERR_VAL;
  }
  pcb->remote_port = port;
  if (pcb->local_port == 0) {
    pcb->local_port = tcp_new_port();
  }
  iss = tcp_next_iss();
  pcb->rcv_nxt = 0;
  pcb->snd_nxt = iss;
  pcb->lastack = iss - 1;
  pcb->snd_lbb = iss - 1;
  pcb->rcv_wnd = TCP_WND;
  pcb->snd_wnd = TCP_WND;
  pcb->mss = TCP_MSS;
  pcb->cwnd = 1;
  pcb->ssthresh = pcb->mss * 10;
  pcb->state = SYN_SENT;
  pcb->connected = connected;

  TCP_REG(&tcp_active_pcbs, pcb);
  
  /* Build an MSS option */
  optdata = htonl(((uint32_t)2 << 24) | 
		  ((uint32_t)4 << 16) | 
		  (((uint32_t)pcb->mss / 256) << 8) |
		  (pcb->mss & 255));

  ret = tcp_enqueue(pcb, NULL, 0, TCP_SYN, 0, (uint8_t *)&optdata, 4);
  if (ret == RC_OK) { 
    DEBUG_TCP kprintf(KR_OSTREAM,"Connect out put");
    tcp_output(pcb);
  }
  return ret;
} 

/*
 * tcp_slowtmr():
 *
 * Called every 500 ms and implements the retransmission timer and 
 * the timer that removes PCBs that have been in TIME-WAIT for enough
 * time. It also increments various timers such as the inactivity timer
 * in each PCB.
 */
void
tcp_slowtmr(void)
{
  struct tcp_pcb *pcb, *pcb2, *prev;
  uint32_t eff_wnd;
  uint8_t pcb_remove;      /* flag if a PCB should be removed */
  uint32_t err;

  err = RC_OK;

  ++tcp_ticks;

  /* Steps through all of the active PCBs. */
  prev = NULL;
  pcb = tcp_active_pcbs;
  
  //DEBUG_TCP if(pcb == NULL) kprintf(KR_OSTREAM,"tcp_slowtmr:no active pcbs");
  while (pcb != NULL) {
    //DEBUG_TCP
    //kprintf(KR_OSTREAM,"tcp_slowtmr: processing active pcb\n");
    ERIP_ASSERT("tcp_slowtmr: active pcb->state != CLOSED\n", 
		pcb->state != CLOSED);
    ERIP_ASSERT("tcp_slowtmr: active pcb->state != LISTEN\n", 
		pcb->state != LISTEN);
    ERIP_ASSERT("tcp_slowtmr: active pcb->state != TIME-WAIT\n", 
		pcb->state != TIME_WAIT);
    
    pcb_remove = 0;

    if (pcb->state == SYN_SENT && pcb->nrtx == TCP_SYNMAXRTX) {
      ++pcb_remove;
      DEBUG_TCP kprintf(KR_OSTREAM,"tcp_slowtmr: max SYN retries reached");
    }else if (pcb->nrtx == TCP_MAXRTX) {
      ++pcb_remove;
      DEBUG_TCP kprintf(KR_OSTREAM,"tcp_slowtmr: max DATA retries reached\n");
    } else {
      ++pcb->rtime;
      if (pcb->unacked != NULL && pcb->rtime >= pcb->rto) {
	/* Time for a retransmission. */
        DEBUG_TCP kprintf(KR_OSTREAM,"tcp_slowtmr: rtime %d pcb->rto %d",
			  pcb->rtime, pcb->rto);

        /* Double retransmission time-out unless we are trying to
         * connect to somebody (i.e., we are in SYN_SENT). */
        if (pcb->state != SYN_SENT) {
          pcb->rto = ((pcb->sa >> 3) + pcb->sv) << tcp_backoff[pcb->nrtx];
        }
        tcp_rexmit(pcb);
        /* Reduce congestion window and ssthresh. */
        eff_wnd = ERIP_MIN(pcb->cwnd, pcb->snd_wnd);
        pcb->ssthresh = eff_wnd >> 1;
        if (pcb->ssthresh < pcb->mss) {
          pcb->ssthresh = pcb->mss * 2;
        }
        pcb->cwnd = pcb->mss;
        DEBUG_TCP kprintf(KR_OSTREAM,"tcp_slowtmr: cwnd %u ssthresh %u\n",
			  pcb->cwnd, pcb->ssthresh);
      }
    }
    /* Check if this PCB has stayed too long in FIN-WAIT-2 */
    if (pcb->state == FIN_WAIT_2) {
      if ((uint32_t)(tcp_ticks - pcb->tmr) >
	  TCP_FIN_WAIT_TIMEOUT / TCP_SLOW_INTERVAL) {
        ++pcb_remove;
        DEBUG_TCP
	  kprintf(KR_OSTREAM,"tcp_slowtmr: removing pcb stuck in FIN-WAIT-2");
      }
    }

    /* If this PCB has queued out of sequence data, but has been
       inactive for too long, will drop the data (it will eventually
       be retransmitted). */
    if (pcb->ooseq != NULL &&
	(uint32_t)tcp_ticks - pcb->tmr >=
	pcb->rto * TCP_OOSEQ_TIMEOUT) {
      tcp_segs_free(pcb->ooseq,pcb->ssid);
      pcb->ooseq = NULL;
      DEBUG_TCP kprintf(KR_OSTREAM,"tcp_slowtmr: dropping OOSEQ queued data");
    }
    
    /* Check if this PCB has stayed too long in SYN-RCVD */
    if (pcb->state == SYN_RCVD) {
      if ((uint32_t)(tcp_ticks - pcb->tmr) >
	  TCP_SYN_RCVD_TIMEOUT / TCP_SLOW_INTERVAL) {
        ++pcb_remove;
        DEBUG_TCP 
	  kprintf(KR_OSTREAM,"tcp_slowtmr: removing pcb stuck in SYN-RCVD");
      }
    }

    /* If the PCB should be removed, do it. */
    if (pcb_remove) {
      tcp_pcb_purge(pcb);      
      /* Remove PCB from tcp_active_pcbs list. */
      if (prev != NULL) {
	ERIP_ASSERT("tcp_slowtmr: middle tcp != tcp_active_pcbs", 
		    pcb != tcp_active_pcbs);
        prev->next = pcb->next;
      } else {
        /* This PCB was the first. */
        ERIP_ASSERT("tcp_slowtmr: first pcb == tcp_active_pcbs", 
		    tcp_active_pcbs == pcb);
        tcp_active_pcbs = pcb->next;
      }

      TCP_EVENT_ERR(pcb->errf, pcb->callback_arg, ERR_ABRT);
      
      pcb2 = pcb->next;
      memp_free(MEMP_TCP_PCB, pcb);
      pcb = pcb2;
    } else {
#if 0
      /* We check if we should poll the connection. */
      tcp_output(pcb);
      ++pcb->polltmr;
      if (pcb->polltmr >= pcb->pollinterval) {
	pcb->polltmr = 0;
        DEBUG_TCP kprintf(KR_OSTREAM,"tcp_slowtmr: polling application %d",
			  pcb->pollinterval);
	//TCP_EVENT_POLL(pcb, err);
	//if (err == RC_OK) tcp_output(pcb);
	tcp_output(pcb);
      }
#endif
      prev = pcb;
      pcb = pcb->next;
    }
  }

  
  /* Steps through all of the TIME-WAIT PCBs. */
  prev = NULL;    
  pcb = tcp_tw_pcbs;
  while (pcb != NULL) {
    ERIP_ASSERT("tcp_slowtmr: TIME-WAIT pcb->state == TIME-WAIT", 
		pcb->state == TIME_WAIT);
    pcb_remove = 0;

    /* Check if this PCB has stayed long enough in TIME-WAIT */
    if ((uint32_t)(tcp_ticks - pcb->tmr) > 2 * TCP_MSL / TCP_SLOW_INTERVAL) {
      ++pcb_remove;
    }
    
    /* If the PCB should be removed, do it. */
    if (pcb_remove) {
      tcp_pcb_purge(pcb);      
      /* Remove PCB from tcp_tw_pcbs list. */
      if (prev != NULL) {
	ERIP_ASSERT("tcp_slowtmr: middle tcp != tcp_tw_pcbs", 
		    pcb != tcp_tw_pcbs);
        prev->next = pcb->next;
      } else {
        /* This PCB was the first. */
        ERIP_ASSERT("tcp_slowtmr: first pcb == tcp_tw_pcbs", 
		    tcp_tw_pcbs == pcb);
        tcp_tw_pcbs = pcb->next;
      }
      pcb2 = pcb->next;
      memp_free(MEMP_TCP_PCB, pcb);
      pcb = pcb2;
    } else {
      prev = pcb;
      pcb = pcb->next;
    }
  }
}


/* Is called every TCP_FINE_TIMEOUT (200 ms) and sends delayed ACKs. */
void
tcp_fasttmr(void)
{
  struct tcp_pcb *pcb;

  /* send delayed ACKs */  
  for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
    if (pcb->flags & TF_ACK_DELAY) {
      DEBUG_TCP kprintf(KR_OSTREAM,"tcp_fasttmr: delayed ACK");
      tcp_ack_now(pcb);
      pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
    }
  }
}

/* Called periodically to dispatch TCP timers. */
void
tcp_tmr(void)
{
  if (++tcp_timer == 9) {
    tcp_timer = 0;
  }
  
  if (tcp_timer & 1) {
    /* Call tcp_fasttmr() every 200 ms, i.e., every other timer
     *tcp_tmr() is called. */
    tcp_fasttmr();
  }
  if (tcp_timer == 0 || tcp_timer == 4) {
    /* Call tcp_slowtmr() every 500 ms, i.e., every fifth timer
       tcp_tmr() is called. */
    tcp_slowtmr();
  }
}

/* Deallocates a list of TCP segments (tcp_seg structures) */
uint8_t
tcp_segs_free(struct tcp_seg *seg,int ssid)
{
  uint8_t count = 0;
  struct tcp_seg *next;
 again:  
  if (seg != NULL) {
    next = seg->next;
    count += tcp_seg_free(seg,ssid);
    seg = next;
    goto again;
  }
  return count;
}


/* Frees a TCP segment */
uint8_t
tcp_seg_free(struct tcp_seg *seg,int ssid)
{
  uint8_t count = 0;
  
  if (seg != NULL) {
    if (seg->p == NULL) {
      memp_free(MEMP_TCP_SEG, seg);
    } else {
      count = pstore_free(seg->p,ssid);
#if TCP_DEBUG
      seg->p = NULL;
#endif /* TCP_DEBUG */
      memp_free(MEMP_TCP_SEG, seg);
    }
  }
  return count;
}

/* Sets the priority of a connection.*/
void
tcp_setprio(struct tcp_pcb *pcb, uint8_t prio)
{
  pcb->prio = prio;
}

/* Returns a copy of the given TCP segment. */ 
struct tcp_seg *
tcp_seg_copy(struct tcp_seg *seg,int ssid)
{
  struct tcp_seg *cseg;

  cseg = memp_alloc(MEMP_TCP_SEG);
  if (cseg == NULL) {
    return NULL;
  }
  memcpy((char *)cseg, (const char *)seg, sizeof(struct tcp_seg)); 
  pstore_ref(cseg->p,ssid);
  return cseg;
}

static uint32_t
tcp_recv_null(void *arg, struct tcp_pcb *pcb, struct pstore *p, uint32_t err)
{
  arg = arg;
  if (p != NULL) {
    pstore_free(p,pcb->ssid);
  } else if (err == RC_OK) {
    return tcp_close(pcb);
  }
  return RC_OK;
}

static void
tcp_kill_prio(uint8_t prio)
{
  struct tcp_pcb *pcb, *inactive;
  uint32_t inactivity;
  uint8_t mprio;

  mprio = TCP_PRIO_MAX;
  
  /* We kill the oldest active connection that has lower priority than
     prio. */
  inactivity = 0;
  inactive = NULL;
  for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
    if (pcb->prio <= prio &&
	pcb->prio <= mprio &&
	(uint32_t)(tcp_ticks - pcb->tmr) >= inactivity) {
      inactivity = tcp_ticks - pcb->tmr;
      inactive = pcb;
      mprio = pcb->prio;
    }
  }
  if (inactive != NULL) {
    DEBUG_TCP kprintf(KR_OSTREAM,"tcp_kill_prio: killing oldest PCB 0x%c (%ld)",
		      (void *)inactive, inactivity);
    tcp_abort(inactive);
  }      
}

static void
tcp_kill_timewait(void)
{
  struct tcp_pcb *pcb, *inactive;
  uint32_t inactivity;

  inactivity = 0;
  inactive = NULL;
  for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
    if ((uint32_t)(tcp_ticks - pcb->tmr) >= inactivity) {
      inactivity = tcp_ticks - pcb->tmr;
      inactive = pcb;
    }
  }
  if (inactive != NULL) {
    DEBUGF(TCP_DEBUG,
	   ("tcp_kill_timewait: killing oldest TIME-WAIT PCB 0x%p (%ld)\n",
	    (void *)inactive, inactivity));
    tcp_abort(inactive);
  }      
}

struct tcp_pcb *
tcp_alloc(uint8_t prio)
{
  struct tcp_pcb *pcb;
  uint32_t iss;
  
  pcb = memp_alloc(MEMP_TCP_PCB);
  if (pcb == NULL) {
    /* Try killing oldest connection in TIME-WAIT. */
    DEBUGF(TCP_DEBUG,("tcp_alloc: killing off oldest TIME-WAIT connection"));
    tcp_kill_timewait();
    pcb = memp_alloc(MEMP_TCP_PCB);
    if (pcb == NULL) {
      tcp_kill_prio(prio);    
      pcb = memp_alloc(MEMP_TCP_PCB);
    }
  }
  
  DEBUG_TCP kprintf(KR_OSTREAM,"Allocated tcp connection at %08x",&pcb[0]);
  if (pcb != NULL) {
    //memset(pcb, 0, sizeof(struct tcp_pcb));
    pcb->timeout = 0;
    pcb->prio = TCP_PRIO_NORMAL;
    pcb->snd_buf = TCP_SND_BUF;
    pcb->snd_queuelen = 0;
    pcb->rcv_wnd = TCP_WND;
    pcb->mss = TCP_MSS;
    pcb->rto = 3000 / TCP_SLOW_INTERVAL;
    pcb->sa = 0;
    pcb->sv = 3000 / TCP_SLOW_INTERVAL;
    pcb->rtime = 0;
    pcb->cwnd = 1;
    iss = tcp_next_iss();
    pcb->snd_wl2 = iss;
    pcb->snd_nxt = iss;
    pcb->snd_max = iss;
    pcb->lastack = iss;
    pcb->snd_lbb = iss;   
    pcb->tmr = tcp_ticks;
    
    pcb->polltmr = 0;

    pcb->recv = tcp_recv_null;
  }
  return pcb;
}

/* Creates a new TCP protocol control block but doesn't place it on
 * any of the TCP PCB lists. */
struct tcp_pcb *
tcp_new(void)
{
  return tcp_alloc(TCP_PRIO_NORMAL);
}

/* Used to specify the argument that should be passed callback functions.*/
void
tcp_arg(struct tcp_pcb *pcb, void *arg)
{  
  pcb->callback_arg = arg;
}

/*  Used to specify the function that should be called when a TCP
 * connection receives data. */ 
void
tcp_recv(struct tcp_pcb *pcb,
	 uint32_t (* recv)(void *arg, struct tcp_pcb *tpcb, 
			   struct pstore *p, uint32_t err))
{
  pcb->recv = recv;
}

/* Used to specify the function that should be called when TCP data
 * has been successfully delivered to the remote host.*/
void
tcp_sent(struct tcp_pcb *pcb,
	 uint32_t (* sent)(void *arg, struct tcp_pcb *tpcb, uint16_t len))
{
  pcb->sent = sent;
}


/* Used to specify the function that should be called when a fatal error
 * has occured on the connection.*/
void
tcp_err(struct tcp_pcb *pcb,
	void (* errf)(void *arg, uint32_t err))
{
  pcb->errf = errf;
}

/* Used to specify the function that should be called periodically
 * from TCP. The interval is specified in terms of the TCP coarse
 * timer interval, which is called twice a second. */ 
void
tcp_poll(struct tcp_pcb *pcb,
	 uint32_t (* poll)(void *arg, struct tcp_pcb *tpcb), uint8_t interval)
{
  pcb->poll = poll;
  pcb->pollinterval = interval;
}

/* Used for specifying the function that should be called when a
 * LISTENing connection has been connected to another host. */ 
void
tcp_accept(struct tcp_pcb *pcb,
	   uint32_t (* accept)(void *arg, struct tcp_pcb *newpcb, 
			       uint32_t err))
{
  ((struct tcp_pcb_listen *)pcb)->accept = accept;
}


/* Purges a TCP PCB. Removes any buffered data and frees the buffer memory.*/
void
tcp_pcb_purge(struct tcp_pcb *pcb)
{
  if (pcb->state != CLOSED &&
      pcb->state != TIME_WAIT &&
      pcb->state != LISTEN) {

    DEBUGF(TCP_DEBUG, ("tcp_pcb_purge\n"));
    
#if TCP_DEBUG
    if (pcb->unsent != NULL) {    
      DEBUGF(TCP_DEBUG, ("tcp_pcb_purge: not all data sent\n"));
    }
    if (pcb->unacked != NULL) {    
      DEBUGF(TCP_DEBUG, ("tcp_pcb_purge: data left on ->unacked\n"));
    }
#if TCP_QUEUE_OOSEQ /* LW */
    if (pcb->ooseq != NULL) {    
      DEBUGF(TCP_DEBUG, ("tcp_pcb_purge: data left on ->ooseq\n"));
    }
#endif
#endif /* TCP_DEBUG */
    tcp_segs_free(pcb->unsent,pcb->ssid);
#if TCP_QUEUE_OOSEQ
    tcp_segs_free(pcb->ooseq,pcb->ssid);
#endif /* TCP_QUEUE_OOSEQ */
    tcp_segs_free(pcb->unacked,pcb->ssid);
    pcb->unacked = pcb->unsent =
#if TCP_QUEUE_OOSEQ
      pcb->ooseq =
#endif /* TCP_QUEUE_OOSEQ */
      NULL;
  }
}

/* Purges the PCB and removes it from a PCB list. Any delayed ACKs 
 * are sent first. */
void
tcp_pcb_remove(struct tcp_pcb **pcblist, struct tcp_pcb *pcb)
{
  TCP_RMV(pcblist, pcb);

  tcp_pcb_purge(pcb);
  
  /* if there is an outstanding delayed ACKs, send it */
  if (pcb->state != TIME_WAIT &&
      pcb->state != LISTEN &&
      pcb->flags & TF_ACK_DELAY) {
    pcb->flags |= TF_ACK_NOW;
    tcp_output(pcb);
  }  
  pcb->state = CLOSED;

  ERIP_ASSERT("tcp_pcb_remove: tcp_pcbs_sane()", tcp_pcbs_sane());
}

/* Calculates a new initial sequence number for new connections.*/
uint32_t
tcp_next_iss(void)
{
  static uint32_t iss = 6510;
  
  iss += tcp_ticks;       /* XXX */
  return iss;
}

#if TCP_DEBUG || TCP_INPUT_DEBUG || TCP_OUTPUT_DEBUG
void
tcp_debug_print(struct tcp_hdr *tcphdr)
{
  DEBUGF(TCP_DEBUG, ("TCP header:\n"));
  DEBUGF(TCP_DEBUG, ("+-------------------------------+\n"));
  DEBUGF(TCP_DEBUG, ("|      %04x     |      %04x     |(src port,dest port)\n",
		     tcphdr->src, tcphdr->dest));
  DEBUGF(TCP_DEBUG, ("+-------------------------------+\n"));
  DEBUGF(TCP_DEBUG, ("|            %08lu           | (seq no)\n",
		     tcphdr->seqno));
  DEBUGF(TCP_DEBUG, ("+-------------------------------+\n"));
  DEBUGF(TCP_DEBUG, ("|            %08lu           | (ack no)\n",
		     tcphdr->ackno));
  DEBUGF(TCP_DEBUG, ("+-------------------------------+\n"));
  DEBUGF(TCP_DEBUG, ("| %2u |    |%u%u%u%u%u|    %5u      | (offset, flags (",
		     TCPH_OFFSET(tcphdr),
		     TCPH_FLAGS(tcphdr) >> 4 & 1,
		     TCPH_FLAGS(tcphdr) >> 4 & 1,
		     TCPH_FLAGS(tcphdr) >> 3 & 1,
		     TCPH_FLAGS(tcphdr) >> 2 & 1,
		     TCPH_FLAGS(tcphdr) >> 1 & 1,
		     TCPH_FLAGS(tcphdr) & 1,
		     tcphdr->wnd));
  tcp_debug_print_flags(TCPH_FLAGS(tcphdr));
  DEBUGF(TCP_DEBUG, ("), win)\n"));
  DEBUGF(TCP_DEBUG, ("+-------------------------------+\n"));
  DEBUGF(TCP_DEBUG, ("|    0x%04x     |     %5u     | (chksum, urgp)\n",
		     ntohs(tcphdr->chksum), ntohs(tcphdr->urgp)));
  DEBUGF(TCP_DEBUG, ("+-------------------------------+\n"));
}


void
tcp_debug_print_state(enum tcp_state s)
{
  DEBUGF(TCP_DEBUG, ("State: "));
  switch (s) {
  case CLOSED:
    DEBUGF(TCP_DEBUG, ("CLOSED\n"));
    break;
  case LISTEN:
    DEBUGF(TCP_DEBUG, ("LISTEN\n"));
    break;
  case SYN_SENT:
    DEBUGF(TCP_DEBUG, ("SYN_SENT\n"));
    break;
  case SYN_RCVD:
    DEBUGF(TCP_DEBUG, ("SYN_RCVD\n"));
    break;
  case ESTABLISHED:
    DEBUGF(TCP_DEBUG, ("ESTABLISHED\n"));
    break;
  case FIN_WAIT_1:
    DEBUGF(TCP_DEBUG, ("FIN_WAIT_1\n"));
    break;
  case FIN_WAIT_2:
    DEBUGF(TCP_DEBUG, ("FIN_WAIT_2\n"));
    break;
  case CLOSE_WAIT:
    DEBUGF(TCP_DEBUG, ("CLOSE_WAIT\n"));
    break;
  case CLOSING:
    DEBUGF(TCP_DEBUG, ("CLOSING\n"));
    break;
  case LAST_ACK:
    DEBUGF(TCP_DEBUG, ("LAST_ACK\n"));
    break;
  case TIME_WAIT:
    DEBUGF(TCP_DEBUG, ("TIME_WAIT\n"));
    break;
  }
}


void
tcp_debug_print_flags(uint8_t flags)
{
  if (flags & TCP_FIN) {
    DEBUGF(TCP_DEBUG, ("FIN "));
  }
  if (flags & TCP_SYN) {
    DEBUGF(TCP_DEBUG, ("SYN "));
  }
  if (flags & TCP_RST) {
    DEBUGF(TCP_DEBUG, ("RST "));
  }
  if (flags & TCP_PSH) {
    DEBUGF(TCP_DEBUG, ("PSH "));
  }
  if (flags & TCP_ACK) {
    DEBUGF(TCP_DEBUG, ("ACK "));
  }
  if (flags & TCP_URG) {
    DEBUGF(TCP_DEBUG, ("URG "));
  }
}


void
tcp_debug_print_pcbs(void)
{
  struct tcp_pcb *pcb;
  DEBUGF(TCP_DEBUG, ("Active PCB states:\n"));
  for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
    DEBUGF(TCP_DEBUG, 
	   ("Local port %u, foreign port %u snd_nxt %lu rcv_nxt %lu ",
	    pcb->local_port, pcb->remote_port,
	    pcb->snd_nxt, pcb->rcv_nxt));
    tcp_debug_print_state(pcb->state);
  }    
  DEBUGF(TCP_DEBUG, ("Listen PCB states:\n"));
  for(pcb = (struct tcp_pcb *)tcp_listen_pcbs; pcb != NULL; pcb = pcb->next) {
    DEBUGF(TCP_DEBUG, 
	   ("Local port %u, foreign port %u snd_nxt %lu rcv_nxt %lu ",
	    pcb->local_port, pcb->remote_port,
	    pcb->snd_nxt, pcb->rcv_nxt));
    tcp_debug_print_state(pcb->state);
  }    
  DEBUGF(TCP_DEBUG, ("TIME-WAIT PCB states:\n"));
  for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
    DEBUGF(TCP_DEBUG, 
	   ("Local port %u, foreign port %u snd_nxt %lu rcv_nxt %lu ",
	    pcb->local_port, pcb->remote_port,
	    pcb->snd_nxt, pcb->rcv_nxt));
    tcp_debug_print_state(pcb->state);
  }    
}


int
tcp_pcbs_sane(void)
{
  struct tcp_pcb *pcb;
  for(pcb = tcp_active_pcbs; pcb != NULL; pcb = pcb->next) {
    ERIP_ASSERT("tcp_pcbs_sane: active pcb->state != CLOSED", 
		pcb->state != CLOSED);
    ERIP_ASSERT("tcp_pcbs_sane: active pcb->state != LISTEN", 
		pcb->state != LISTEN);
    ERIP_ASSERT("tcp_pcbs_sane: active pcb->state != TIME-WAIT", 
		pcb->state != TIME_WAIT);
  }
  for(pcb = tcp_tw_pcbs; pcb != NULL; pcb = pcb->next) {
    ERIP_ASSERT("tcp_pcbs_sane: tw pcb->state == TIME-WAIT", 
		pcb->state == TIME_WAIT);
  }
  return 1;
}
#endif /* TCP_DEBUG */
