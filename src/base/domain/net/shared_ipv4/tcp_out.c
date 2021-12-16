/* * Copyright (C) 2002, Jonathan S. Shapiro.
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

/*  The output functions of TCP. */
#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>

#include "include/mem.h"
#include "include/memp.h"
#include "include/inet.h"
#include "include/tcp.h"
#include "include/def.h"
#include "include/opt.h"
#include "include/stats.h"
#include "include/err.h"
#include "Session.h"

#include "netsyskeys.h"
#include "netif/netif.h"

#define DEBUG_TCPOUT if(0)

extern struct session ActiveSessions[MAX_SESSIONS];
/* Forward declarations.*/
static void tcp_output_segment(struct tcp_seg *seg, struct tcp_pcb *pcb);

uint32_t
tcp_send_ctrl(struct tcp_pcb *pcb,uint8_t flags)
{
  return tcp_enqueue(pcb, NULL, 0, flags, 1, NULL, 0);

}

uint32_t
tcp_write(struct tcp_pcb *pcb, const void *arg, uint16_t len, uint8_t copy)
{
  DEBUGF(DEBUG_TCPOUT, ("tcp_write(pcb=%x, arg=%x, len=%d, copy=%d)\n", 
			(void *)pcb, arg, len, copy));
  if (pcb->state == SYN_SENT ||
      pcb->state == SYN_RCVD ||
      pcb->state == ESTABLISHED ||
      pcb->state == CLOSE_WAIT) {
    if (len > 0) {
      return tcp_enqueue(pcb, (void *)arg, len, 0, copy, NULL, 0);
    }
    return RC_OK;
  } else {
    DEBUGF(DEBUG_TCPOUT,"tcp_write() called in invalid state\n");
    return ERR_CONN;
  }
}

uint32_t
tcp_enqueue(struct tcp_pcb *pcb, void *arg, uint16_t len,
	    uint8_t flags, uint8_t copy,
            uint8_t *optdata, uint8_t optlen)
{
  struct tcp_seg *seg, *useg, *queue;
  uint32_t left, seqno;
  uint16_t seglen;
  void *ptr;
  uint32_t queuelen;

  DEBUG_TCPOUT 
    kprintf(KR_OSTREAM,"tcp_enqueue(pcb=%x, arg=%x,len=%d,flags=%xe,copy=%d)", 
	    (void *)pcb, arg, len, flags, copy);
  left = len;
  ptr = arg;
  /* fail on too much data */
  if (len > pcb->snd_buf) {
    DEBUG_TCPOUT
      kprintf(KR_OSTREAM,"tcp_enqueue: too much data (len=%d > snd_buf=%d", 
	      len, pcb->snd_buf);
    return ERR_MEM;
  }

  /* seqno will be the sequence number of the first segment enqueued
   * by the call to this function. */
  seqno = pcb->snd_lbb;

  queue = NULL;
  DEBUGF(TCP_QLEN_DEBUG, ("tcp_enqueue: queuelen: %d\n", pcb->snd_queuelen));

  /* Check if the queue length exceeds the configured maximum queue
   * length. If so, we return an error. */
  queuelen = pcb->snd_queuelen;
  if (queuelen >= TCP_SND_QUEUELEN) {
    DEBUGF(TCP_OUTPUT_DEBUG | 3, 
	   ("tcp_enqueue: too long queue %d (max %d)\n", 
	    queuelen, TCP_SND_QUEUELEN));
    goto memerr;
  }   

  if (pcb->snd_queuelen != 0) {
    ERIP_ASSERT("tcp_enqueue: valid queue length", pcb->unacked != NULL ||
		pcb->unsent != NULL);      
  }

  seg = NULL;
  seglen = 0;

  /* First, break up the data into segments and tuck them together in
   * the local "queue" variable. */
  while (queue == NULL || left > 0) {

    /* The segment length should be the MSS if the data to be enqueued
     * is larger than the MSS. */
    seglen = left > pcb->mss? pcb->mss: left;

    /* Allocate memory for tcp_seg, and fill in fields. */
    seg = memp_alloc(MEMP_TCP_SEG);
    if (seg == NULL) {
      DEBUGF(TCP_OUTPUT_DEBUG | 2, 
	     ("tcp_enqueue: could not allocate memory for tcp_seg\n"));
      goto memerr;
    }
    seg->next = NULL;
    seg->p = NULL;

    if (queue == NULL) {
      queue = seg;
    }else {
      /* Attach the segment to the end of the queued segments. */
      for (useg = queue; useg->next != NULL; useg = useg->next);
      useg->next = seg;
    }
    
    /* If copy is set, memory should be allocated
     * and data copied into pstore, otherwise data comes from
     * ROM or other static memory, and need not be copied. If
     * optdata is != NULL, we have options instead of data. */
    if (optdata != NULL) {
      if ((seg->p = pstore_alloc(PSTORE_TRANSPORT,optlen,XMIT_STACK_SPACE,
				 pcb->ssid)) 
	  == NULL) {
        goto memerr;
      }
      ++queuelen;
      seg->dataptr = PSTORE_PAYLOAD(seg->p,pcb->ssid);
    }else if (copy) {
      if ((seg->p = pstore_alloc(PSTORE_TRANSPORT, seglen,XMIT_STACK_SPACE,
				 pcb->ssid))==NULL){
        DEBUGF(TCP_OUTPUT_DEBUG | 2, 
	       ("tcp_enqueue:couldn't alloc memory for pstore copy size %u\n", 
		seglen));	  
        goto memerr;
      }
      ++queuelen;
      if (arg != NULL) {
        memcpy(PSTORE_PAYLOAD(seg->p,pcb->ssid), ptr, seglen);
      }
      seg->dataptr = PSTORE_PAYLOAD(seg->p,pcb->ssid);
    }
#if 0
    else {     /* do not copy data */
      /* first, allocate a pstore for holding the data.
       * since the referenced data is available at least until it is sent 
       * out on the link (as it has to be ACKed by the remote party) we can
       * safely use PSTORE_ROM instead of PSTORE_REF here.
       */
      if ((p = pstore_alloc(PSTORE_TRANSPORT, seglen,XMIT_STACK_SPACE,
			    pcb->ssid)) == NULL) {
        DEBUGF(TCP_OUTPUT_DEBUG | 2, 
	       "tcp_enqueue: could not allocate memory for zero-copy pstore");
        goto memerr;
      }
      ++queuelen;
      PSTORE_PAYLOAD(p,pcb->ssid) = ptr;
      p->offset = 
	seg->dataptr = ptr;
      
      /* Second, allocate a pstore for the headers. */
      if ((seg->p = pstore_alloc(PSTORE_TRANSPORT, 0, PSTORE_RAM)) == NULL) {
        /* If allocation fails, we have to deallocate the data pstore as
         * well. */
        pstore_free(p,pcb->ssid);
        DEBUGF(TCP_OUTPUT_DEBUG | 2, 
	       ("tcp_enqueue: could not allocate memory for header pstore\n"));
        goto memerr;
      }
      ++queuelen;

      /* Chain the headers and data pstores together. */
      pstore_chain_no_ref(seg->p, p,pcb->ssid);
      //pstore_free(p,pcb->ssid);
      p = NULL;
    }
#endif

    /* Now that there are more segments queued, we check again if the
       length of the queue exceeds the configured maximum. */
    if (queuelen > TCP_SND_QUEUELEN) {
      DEBUGF(TCP_OUTPUT_DEBUG | 2, 
	     ("tcp_enqueue: queue too long %d (%d)\n", 
	      queuelen, TCP_SND_QUEUELEN)); 	
      goto memerr;
    }

    seg->len = seglen;
#if 0 /* Was commented out. TODO: can someone say why this is here? */
    if ((flags & TCP_SYN) || (flags & TCP_FIN)) { 
      ++seg->len;
    }
#endif
    /* Build TCP header. */
    if (pstore_header(seg->p, TCP_HLEN,pcb->ssid)) {
      DEBUGF(TCP_OUTPUT_DEBUG | 2, 
	     ("tcp_enqueue: no room for TCP header in pstore.\n"));
      goto memerr;
    }
    seg->tcphdr = PSTORE_PAYLOAD(seg->p,pcb->ssid);
    seg->tcphdr->src = htons(pcb->local_port);
    seg->tcphdr->dest = htons(pcb->remote_port);
    seg->tcphdr->seqno = htonl(seqno);
    seg->tcphdr->urgp = 0;
    TCPH_FLAGS_SET(seg->tcphdr, flags);
    /* don't fill in tcphdr->ackno and tcphdr->wnd until later */

    /* Copy the options into the header, if they are present. */
    if (optdata == NULL) {
      TCPH_OFFSET_SET(seg->tcphdr, 5 << 4);
    }else {
      TCPH_OFFSET_SET(seg->tcphdr, (5 + optlen / 4) << 4);
      /* Copy options into data portion of segment.
	 Options can thus only be sent in non data carrying
	 segments such as SYN|ACK. */
      memcpy(seg->dataptr, optdata, optlen);
    }
    DEBUG_TCPOUT kprintf(KR_OSTREAM,"tcp_enqueue: queueing 0x%x:0x%x (0x%x)",
			 ntohl(seg->tcphdr->seqno),
			 ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg),
			 flags);
  
    left -= seglen;
    seqno += seglen;
    ptr = (void *)((char *)ptr + seglen);
  }


  /* Now that the data to be enqueued has been broken up into TCP
     segments in the queue variable, we add them to the end of the
     pcb->unsent queue. */
  if (pcb->unsent == NULL) {
    useg = NULL;
  }
  else {
    for (useg = pcb->unsent; useg->next != NULL; useg = useg->next);
  }

  /* If there is room in the last pstore on the unsent queue,
     chain the first pstore on the queue together with that. */
  if (useg != NULL &&
      TCP_TCPLEN(useg) != 0 &&
      !(TCPH_FLAGS(useg->tcphdr) & (TCP_SYN | TCP_FIN)) &&
      !(flags & (TCP_SYN | TCP_FIN)) &&
      useg->len + queue->len <= pcb->mss) {
    /* Remove TCP header from first segment. */
    pstore_header(queue->p, -TCP_HLEN,pcb->ssid);
    pstore_chain_no_ref(useg->p, queue->p,pcb->ssid);
    /* Free buffer which was merged. Note that the previous pstore_chain call
     * will have incremented the ref count, so here the ref count will still
     * be 1 for the 1 pointer still being used on this buffer. */
    //pstore_free(queue->p,pcb->ssid);
    useg->len += queue->len;
    useg->next = queue->next;

    DEBUG_TCPOUT 
      kprintf(KR_OSTREAM,"tcp_enqueue: chaining, new len %d\n", useg->len);
    if (seg == queue) {
      seg = NULL;
    }
    memp_free(MEMP_TCP_SEG, queue);
  } else {      
    if (useg == NULL) {
      pcb->unsent = queue;
    }else {
      useg->next = queue;
    }
  }
  if ((flags & TCP_SYN) || (flags & TCP_FIN)) {
    ++len;
  }
  pcb->snd_lbb += len;
  pcb->snd_buf -= len;
  pcb->snd_queuelen = queuelen;
  DEBUG_TCPOUT
    kprintf(KR_OSTREAM,"tcp_enqueue: %d (after enqueued)", pcb->snd_queuelen);
  if (pcb->snd_queuelen != 0) {
    ERIP_ASSERT("tcp_enqueue: valid queue length", pcb->unacked != NULL ||
		pcb->unsent != NULL);

  }

  /* Set the PSH flag in the last segment that we enqueued, but only
     if the segment has data (indicated by seglen > 0). */
  if (seg != NULL && seglen > 0 && seg->tcphdr != NULL) {
    TCPH_FLAGS_SET(seg->tcphdr, TCPH_FLAGS(seg->tcphdr) | TCP_PSH);
  }

  return RC_OK;
 memerr:

  if (queue != NULL) {
    tcp_segs_free(queue,pcb->ssid);
  }
  if (pcb->snd_queuelen != 0) {
    ERIP_ASSERT("tcp_enqueue: valid queue length", pcb->unacked != NULL ||
		pcb->unsent != NULL);

  }
  DEBUGF(TCP_QLEN_DEBUG | DBG_STATE, 
	 ("tcp_enqueue: %d (with mem err)\n", pcb->snd_queuelen));
  return ERR_MEM;
}


/* find out what we can send and send it */
uint32_t
tcp_output(struct tcp_pcb *pcb)
{
  struct pstore *p;
  struct tcp_hdr *tcphdr;
  struct tcp_seg *seg, *useg;
  uint32_t wnd;
#if TCP_CWND_DEBUG
  int i = 0;
#endif /* TCP_CWND_DEBUG */
  
  /* First, check if we are invoked by the TCP input processing
     code. If so, we do not output anything. Instead, we rely on the
     input processing code to call us when input processing is done
     with. */
  if (tcp_input_pcb == pcb) {
    return RC_OK;
  }
  
  wnd = ERIP_MIN(pcb->snd_wnd, pcb->cwnd);
  
  seg = pcb->unsent;

  /* If the TF_ACK_NOW flag is set, we check if there is data that is
     to be sent. If data is to be sent out, we'll just piggyback our
     acknowledgement with the outgoing segment. If no data will be
     sent (either because the ->unsent queue is empty or because the
     window doesn't allow it) we'll have to construct an empty ACK
     segment and send it. */
  if (pcb->flags & TF_ACK_NOW &&
      (seg == NULL ||
        ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len > wnd)) {
    pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
    p = pstore_alloc(PSTORE_IP, TCP_HLEN, XMIT_STACK_SPACE,pcb->ssid);
    if (p == NULL) {
      kprintf(KR_OSTREAM,"tcp_output: (ACK) could not allocate pstore");
      return ERR_BUF;
    }
    DEBUG_TCPOUT kprintf(KR_OSTREAM,"tcp_output: sending ACK for %d", 
			 pcb->rcv_nxt);    
    
    tcphdr = PSTORE_PAYLOAD(p,pcb->ssid);
    tcphdr->src = htons(pcb->local_port);
    tcphdr->dest = htons(pcb->remote_port);
    tcphdr->seqno = htonl(pcb->snd_nxt);
    tcphdr->ackno = htonl(pcb->rcv_nxt);
    TCPH_FLAGS_SET(tcphdr, TCP_ACK);
    tcphdr->wnd = htons(pcb->rcv_wnd);
    tcphdr->urgp = 0;
    TCPH_OFFSET_SET(tcphdr, 5 << 4);
    
    tcphdr->chksum = 0;
    tcphdr->chksum = inet_chksum_pseudo(p,pcb->ssid,
					&(pcb->local_ip), &(pcb->remote_ip),
					IP_PROTO_TCP, p->tot_len);
    
    ip_output(p,pcb->ssid, &(pcb->local_ip), &(pcb->remote_ip), TCP_TTL,
	      IP_PROTO_TCP);
    
    /* Shouldn't this freed ?in the enet */
    //pstore_free(p,pcb->ssid);

    return RC_OK;
  }
  
  DEBUG_TCPOUT
    if (seg == NULL) {
      kprintf(KR_OSTREAM,"tcp_output: nothing to send (0x%x)\n", pcb->unsent);
    }
#if TCP_CWND_DEBUG
  if (seg == NULL) {
    DEBUGF(TCP_CWND_DEBUG, 
	   ("tcp_output:snd_wnd %lu, cwnd %lu,wnd %lu, seg == NULL, ack %lu\n",
	    pcb->snd_wnd, pcb->cwnd, wnd,
	    pcb->lastack));
  } else {
    kprintf(DEBUG_TCPOUT,
	    "tcp_output:snd_wnd %d,cwnd %d,wnd %d,effwnd %d,seq %d,ack %d",
	    pcb->snd_wnd, pcb->cwnd, wnd,
	    ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len,
	    ntohl(seg->tcphdr->seqno), pcb->lastack);
  }
#endif /* TCP_CWND_DEBUG */
  
  while (seg != NULL &&
	 ntohl(seg->tcphdr->seqno) - pcb->lastack + seg->len <= wnd) {
#if TCP_CWND_DEBUG
    DEBUGF(TCP_CWND_DEBUG,
	   ("tcp_output: snd_wnd %lu, cwnd %lu, wnd %lu, effwnd %lu,\
             seq %lu, ack %lu, i%d\n",
	    pcb->snd_wnd, pcb->cwnd, wnd,
	    ntohl(seg->tcphdr->seqno) + seg->len -
	    pcb->lastack,
	    ntohl(seg->tcphdr->seqno), pcb->lastack, i));
    ++i;
#endif /* TCP_CWND_DEBUG */

    pcb->unsent = seg->next;
    
    if (pcb->state != SYN_SENT) {
      TCPH_FLAGS_SET(seg->tcphdr, TCPH_FLAGS(seg->tcphdr) | TCP_ACK);
      pcb->flags &= ~(TF_ACK_DELAY | TF_ACK_NOW);
    }
    
    tcp_output_segment(seg, pcb);
    pcb->snd_nxt = ntohl(seg->tcphdr->seqno) + TCP_TCPLEN(seg);
    if (TCP_SEQ_LT(pcb->snd_max, pcb->snd_nxt)) {
      pcb->snd_max = pcb->snd_nxt;
    }
    /* put segment on unacknowledged list if length > 0 */
    if (TCP_TCPLEN(seg) > 0) {
      seg->next = NULL;
      if (pcb->unacked == NULL) {
        pcb->unacked = seg;
	
	
      } else {
#if 0
	useg = pcb->unacked;
	if (useg != NULL) {
	  for (useg = pcb->unacked; useg->next != NULL; useg = useg->next);
	}
#endif
	for (useg = pcb->unacked; useg->next != NULL; useg = useg->next);
        useg->next = seg;
      }
    } else {
      tcp_seg_free(seg,pcb->ssid);
    }
    seg = pcb->unsent;
  } 

  return RC_OK;
}

static void
tcp_output_segment(struct tcp_seg *seg, struct tcp_pcb *pcb)
{
  uint16_t len;
  
  /* The TCP header has already been constructed, but the ackno and
   * wnd fields remain. */
  seg->tcphdr->ackno = htonl(pcb->rcv_nxt);

  /* silly window avoidance */
  if (pcb->rcv_wnd < pcb->mss) {
    seg->tcphdr->wnd = 0;
  } else {
    /* advertise our receive window size in this TCP segment */
    seg->tcphdr->wnd = htons(pcb->rcv_wnd);
  }

  /* If we don't have a local IP address, we get one by
     calling ip_route(). */
  /*if (ip_addr_isany(&(pcb->local_ip))) {
    netif = ip_route(&(pcb->remote_ip));
    if (netif == NULL) {
    return;
    }
  */

  ip_addr_set(&(pcb->local_ip), &(NETIF.ip_addr));
  //}
  
  pcb->rtime = 0;
  
  if (pcb->rttest == 0) {
    pcb->rttest = tcp_ticks;
    pcb->rtseq = ntohl(seg->tcphdr->seqno);
    DEBUG_TCPOUT
      kprintf(KR_OSTREAM,"tcp_output_segment: rtseq %d", pcb->rtseq);
  }
  DEBUG_TCPOUT
    kprintf(KR_OSTREAM,"tcp_output_segment: %d:%d",
	    htonl(seg->tcphdr->seqno), 
	    htonl(seg->tcphdr->seqno) +  seg->len);
  
  len = (uint16_t)((uint8_t *)seg->tcphdr - 
		   (uint8_t *)PSTORE_PAYLOAD(seg->p,pcb->ssid));
  
  seg->p->len -= len;
  seg->p->tot_len -= len;
  
  //seg->p->payload = seg->tcphdr;
  seg->p->offset = seg->p->offset + len;
    
  seg->tcphdr->chksum = 0;
  seg->tcphdr->chksum = inet_chksum_pseudo(seg->p,pcb->ssid,
					   &(pcb->local_ip),
					   &(pcb->remote_ip),
					   IP_PROTO_TCP, seg->p->tot_len);
  
  ip_output(seg->p,pcb->ssid,&(pcb->local_ip), &(pcb->remote_ip), TCP_TTL,
	    IP_PROTO_TCP);
}

void
tcp_rst(uint32_t seqno, uint32_t ackno,
	struct ip_addr *local_ip, struct ip_addr *remote_ip,
	uint16_t local_port, uint16_t remote_port)
{
  struct pstore *p;
  struct tcp_hdr *tcphdr;
  p = pstore_alloc(PSTORE_IP, TCP_HLEN,XMIT_STACK_SPACE,/*ssid = 0*/0);
  if (p == NULL) {
    DEBUGF(TCP_DEBUG, ("tcp_rst: could not allocate memory for pstore\n"));
    return;
  }

  tcphdr = PSTORE_PAYLOAD(p,/*ssid =  0*/0);
  tcphdr->src = htons(local_port);
  tcphdr->dest = htons(remote_port);
  tcphdr->seqno = htonl(seqno);
  tcphdr->ackno = htonl(ackno);
  TCPH_FLAGS_SET(tcphdr, TCP_RST | TCP_ACK);
  tcphdr->wnd = htons(TCP_WND);
  tcphdr->urgp = 0;
  TCPH_OFFSET_SET(tcphdr, 5 << 4);
  
  tcphdr->chksum = 0;
  tcphdr->chksum = inet_chksum_pseudo(p,/*ssid = 0*/0, local_ip, remote_ip,
				      IP_PROTO_TCP, p->tot_len);

  ip_output(p, 0 /*From the stack's transmit segment*/,local_ip, 
	    remote_ip, TCP_TTL, IP_PROTO_TCP);
  /* Shouldn't this get freed in the driver */
  //pstore_free(p,0);
  DEBUG_TCPOUT
    kprintf(KR_OSTREAM,"tcp_rst: seqno %d ackno %d", seqno, ackno);
}

void
tcp_rexmit(struct tcp_pcb *pcb)
{
  struct tcp_seg *seg;

  if (pcb->unacked == NULL) {
    return;
  }
  
  /* Move all unacked segments to the unsent queue. */
  for (seg = pcb->unacked; seg->next != NULL; seg = seg->next);
  
  seg->next = pcb->unsent;
  pcb->unsent = pcb->unacked;
  
  pcb->unacked = NULL;
  
  
  pcb->snd_nxt = ntohl(pcb->unsent->tcphdr->seqno);
  
  ++pcb->nrtx;
  
  /* Don't take any rtt measurements after retransmitting. */    
  pcb->rttest = 0;
  
  /* Do the actual retransmission. */
  tcp_output(pcb);
}
