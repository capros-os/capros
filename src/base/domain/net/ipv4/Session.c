/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime distribution,
 * and is derived from the EROS Operating System runtime distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>

#include <idl/capros/key.h>
#include <idl/capros/net/ipv4/netsys.h>
#include <idl/capros/Stream.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/NetSysKey.h>

#include <string.h>

#include "Session.h"
#include "keyring.h"

#include "include/pbuf.h"
#include "include/udp.h"
#include "include/icmp.h"
#include "include/tcp.h"
#include "include/memp.h"

#define DEBUGSESSION if(0)

#define MIN(x,y) (x < y ? x : y)

#define TIMEOUT_AGENT_GRANULARITY   10

/* Maintains a list of all active sessions */
static struct session ActiveSessions[MAX_SESSIONS];
/* Denotes the number of active sessions */
static uint32_t ActiveNo;

/* array of ports -- 65536 max (1 for used 0 for free )*/
static uint8_t ports[MAX_PORTS];

/* Maintains a list of icmp ping sessions */
static struct IcmpSession icmpsession[MAX_ICMP_SESSIONS];

/* Data from clients is received into this buffer */
char rcv_buffer[MAX_BUF_SIZE];

/* Extern globals */
extern struct udp_pcb *udp_pcbs;

/* Returns a unique sesssion id */
/* FIX: We may require a better Session Id creator here */
uint32_t 
new_session() {
  static int SessionId;
  return (++SessionId);
}

/* Init the active session list*/
void 
init_sessions() {
  int i;
  
  for(i=0;i<MAX_SESSIONS;i++) {
    ActiveSessions[i].ssid = -1;
  }
  for(i=0;i<MAX_ICMP_SESSIONS;i++) {
    icmpsession[i].ssid = -1;
    icmpsession[i].listening = 0;
  }
  
  bzero(&ports[0],MAX_PORTS);
  return;
}

/* We have been called by the timeout agent.
 * run through the list of pcbs and update the
 * timeouts of each pcb. If any of the pcb is overtime
 * give it the poison pill.
 * Granularity of the timeout agent = 10ms
 */
void 
session_timeouts_update() 
{
  struct udp_pcb *ipcb;
  struct tcp_pcb *tpcb;
  
  /* Run through the list of udp_pcbs */
  for(ipcb = udp_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
    /* For each pcb keep reducing the time */
    
    /* Check whether our client has specified a timeout */
    if(ipcb->timeout > 0) {
      ipcb->timeout -= TIMEOUT_AGENT_GRANULARITY;
      DEBUGSESSION kprintf(KR_OSTREAM,"ipcb->timeout = %d, ssid = %d",
			   ipcb->timeout,ipcb->ssid);
      if(ipcb->timeout <= 0 ) { /* if timeout reached */
	DEBUGSESSION kprintf(KR_OSTREAM,"Session woken up %d",ipcb->ssid);
	ipcb->listening = 1;
	ipcb->overtime = 1;
	wakeSession(ipcb->ssid);
      }
    }
    /* else continue with the next pcb */
  }
  
  /* Similarily with the tcp lists */
  for(tpcb = tcp_active_pcbs; tpcb!= NULL; tpcb = tpcb->next) {
    /* For each pcb keep reducing the time */
    
    /* Check whether our client has specified a timeout */
    if(tpcb->timeout > 0) {
      tpcb->timeout -= TIMEOUT_AGENT_GRANULARITY;
      DEBUGSESSION kprintf(KR_OSTREAM,"tpcb->timeout = %d, ssid = %d",
			   tpcb->timeout,tpcb->ssid);
      if(tpcb->timeout <= 0 ) { /* if timeout reached */
	DEBUGSESSION kprintf(KR_OSTREAM,"Session woken up %d",tpcb->ssid);
	wakeSession(tpcb->ssid);
	tpcb->session_state = ACTIVE_TIMEDOUT;
      }
    }
    /* else continue with the next pcb */
  }
}


uint32_t 
IsIcmpListening(int index) {
  return icmpsession[index].listening;
}

void 
setIcmpListening(int index) {
  icmpsession[index].listening = 1;
}

void 
resetIcmpListening(int index) {
  icmpsession[index].listening = 0;
}

uint32_t 
icmpsession_open(uint32_t ssid) 
{
  int i;

  /* If already existing return -1 */
  for(i=0;i<MAX_ICMP_SESSIONS;i++) {
    if(icmpsession[i].ssid == ssid)  return -1;
  }

  for(i=0;i<MAX_ICMP_SESSIONS;i++) {
    if(icmpsession[i].ssid == -1) {
      icmpsession[i].ssid = ssid;
      return 0;
    }
  }
  return -1;
}


uint32_t 
icmpsession_close(uint32_t ssid) 
{
  int i;

  for(i=0;i<MAX_ICMP_SESSIONS;i++) {
    if(icmpsession[i].ssid == ssid) {
      icmpsession[i].ssid = -1;
      icmpsession[i].listening = 0;
      return 0;
    }
  }
  return -1;
}

uint32_t 
find_icmpsession(uint32_t ssid) 
{
  int i;

  for(i=0;i<MAX_ICMP_SESSIONS;i++) {
    if(icmpsession[i].ssid == ssid) return i;
  }
  return -1;
}

uint32_t 
tcp_after_connected(void *arg, struct tcp_pcb *pcb, uint32_t err) 
{
  /* We have a connection unblock the client */
  if(!err) pcb->session_state = ACTIVE_ESTABLISHED;
  else pcb->session_state = ACTIVE_NOCONNECT;
  pcb->timeout = 0;
  wakeSession(pcb->ssid);
  kprintf(KR_OSTREAM,"TCP_CONNECTED : Test the connection establised %d",err);
  
  return RC_OK;
}

static void
stream_write(cap_t strm, const char *s, size_t len)
{
  size_t i;

  for (i = 0; i < len; i++) {
    (void) capros_Stream_write(strm, s[i]);
    if(s[i] == '\r') capros_Stream_write(strm,'\n');
  }
}

void
stream_writes(cap_t strm, const char *s)
{
  stream_write(strm, s, strlen(s));
}

uint32_t
tcp_after_recv(void *arg,struct tcp_pcb *ipcb,struct pbuf *p,uint32_t err)
{
  uint32_t totlen=0;
      
  ipcb->timeout = 0;
#if 0
  {
    struct pbuf *q;
    uint32_t i;
    for(q = p; q != NULL; q = q->next) {
      for(i=0;i<q->len;i++) {
	((char *)rcv_buffer)[totlen++] = ((char *)q->payload)[i];
      }
    }
  }
#endif
  totlen = p->tot_len;
  //stream_write(KR_CONSTREAM,&rcv_buffer[0],totlen);
  tcp_recved(ipcb,totlen);
  pbuf_free(p);
  
  return RC_OK;  
}

uint32_t 
tcp_listen_connected(void *arg, struct tcp_pcb *pcb, uint32_t err) 
{
  /* We have a connection unblock the client */
  DEBUGSESSION
    kprintf(KR_OSTREAM,"**********TCP_Listen Connected %d********",err);
  tcp_recv(pcb,tcp_after_recv);
  return RC_OK;
}


/* Check whether any other process (ssid) is not using this 
 * port already 
 * @return The index of the ActiveSession
 *         -1 otherwise
 */
struct session*  
create_session(uint16_t port,uint32_t ssid,uint8_t type) 
{
  int i;
  
  if(ActiveNo < MAX_SESSIONS) {
    /* create a new session */
    for(i=0;i<MAX_SESSIONS;i++) 
      if(ActiveSessions[i].ssid == -1) break;
    
    if(type == UDP_SESSION) {
      struct udp_pcb *p;
      p = udp_new();
      if(p==NULL) return NULL;
      else ActiveSessions[i].pcb.udp = p;
    }

    if(type == TCP_SESSION) {
      struct tcp_pcb *p;
      p = tcp_new();
      if(p==NULL) return NULL;
      else ActiveSessions[i].pcb.tcp = p;
    }
    ActiveSessions[i].ssid = ssid;
    ActiveSessions[i].type = type;
    
    ActiveNo++;
    return &ActiveSessions[i];
  }else {
    /* All sessions exhausted */
    return NULL;
  }
    
  return NULL;
}


/* Find corresponding session */
int
find_session(uint32_t ssid) 
{
  int i;
  
  /* Check if port is already in use (even if by same process) */
  for(i=0;i<MAX_SESSIONS;i++) {
    if(ActiveSessions[i].ssid == ssid) {
      return i;
    }
  }
    
  return -1;
}

/* Block timer */
void 
parkSession(Message *msg) {
  msg->invType = IT_Retry;
  msg->snd_w1 = RETRY_SET_LIK|RETRY_SET_WAKEINFO;
  msg->snd_w2 = msg->rcv_w1; /* wakeinfo value */
  msg->snd_key0 = KR_PARK_WRAP;
}

/* wake timer */
void
wakeSession(uint32_t parkingNo) {
  node_wake_some_no_retry(KR_PARK_NODE,0,0,parkingNo);
}


/* Main dispatching code */
uint32_t 
SessionRequest(Message *msg) 
{
  /* rcv_w3 gets bashed by the kernel due to SEND_WORD flag in
   * the wrapper call invocation */
  uint32_t ssid = msg->rcv_w3;
  switch(msg->rcv_code) {
    
    /* UDP Functions */ 
  case OC_NetSys_UDPConnect:
    {
      struct ip_addr remote_ip;
      uint16_t remote_port;
      struct udp_pcb *ipcb = NULL;
      
      remote_ip.addr = msg->rcv_w1;
      remote_port = (uint16_t)msg->rcv_w2;
      
      DEBUGSESSION kprintf(KR_OSTREAM,"Connect::ssid=%x,ip=%x,port=%x",ssid,
			   remote_ip.addr,remote_port);

      /* Find in udp_pcb list if there exists such an ssid. If not udp_new
       * a udp_pcb and add to the linked list */
      for(ipcb = udp_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      
      if(ipcb == NULL) {
	/* Need to create a new udp_pcb */
	struct udp_pcb *p;
	
	p = udp_new();
	if(p == NULL) msg->snd_code = RC_NetSys_MEMPExhausted;
	else  {
	  p->ssid = ssid; /* Stamp our pcb */
	  msg->snd_code = udp_connect(p,&remote_ip,remote_port);
	}
      }else {
	/* We have our pcb just call udp_connect on it */
	msg->snd_code = udp_connect(ipcb,&remote_ip,remote_port);
      }
      return 1;
    }
  

  case OC_NetSys_UDPBind:
    {

      struct ip_addr local_ip;
      uint16_t local_port;
      struct udp_pcb *ipcb = NULL;
      
      local_ip.addr = msg->rcv_w1;
      if (local_ip.addr == 0) local_ip.addr = NETIF.ip_addr.addr;
      local_port = (uint16_t)msg->rcv_w2;
      
      DEBUGSESSION kprintf(KR_OSTREAM,"bind::ssid=%x,ip=%x,port=%x",
			   ssid,local_ip.addr,local_port);
      
      if(ports[local_port]) msg->snd_code = RC_NetSys_PortInUse;
      else {
	
	/* Find in udp_pcb list if there exists such an ssid. If not udp_new
	 * a udp_pcb and add to the linked list */
	for(ipcb = udp_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	  if(ipcb->ssid == ssid) break;
	}
	
	if(ipcb == NULL) {
	  /* Need to create a new udp_pcb */
	  struct udp_pcb *p;
	  
	  p = udp_new();
	  if(p == NULL) msg->snd_code = RC_NetSys_MEMPExhausted;
	  else {
	    p -> ssid = ssid; /* Stamp our pcb */
	    msg->snd_code = udp_bind(p,&local_ip,local_port);
	    ports[local_port] = 1;
	  }
	}else {
	  /* We have our pcb just call udp_bind on it */
	  msg->snd_code = udp_bind(ipcb,&local_ip,local_port);
	  ports[local_port] = 1;
	}
      }
      return 1;
    }

  case OC_NetSys_UDPSend:
    {
      struct pbuf *p = NULL;
      struct udp_pcb *ipcb = NULL;
      
      DEBUGSESSION kprintf(KR_OSTREAM,"UDP send on ssid=%x",ssid);
      
      /* Find in udp_pcb list if there exists such an ssid. If no such
       * ssid - return error */
      for(ipcb = udp_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      
      DEBUGSESSION kprintf(KR_OSTREAM,"1.ipcb.ssid = %d",ipcb->ssid);
      
      if(ipcb == NULL) msg->snd_code = RC_NetSys_NoExistingSession;
      else {
	/* Allocate a pbuf to take in data and do a udp_send on ipcb */	
	uint16_t data_len = MIN(msg->rcv_limit,msg->rcv_sent);
	
	p = pbuf_alloc(PBUF_IP,UDP_HLEN + data_len,PBUF_RAM);
	if(p == NULL)  msg->snd_code = RC_NetSys_PbufsExhausted;
      	else {
	  p ->payload += UDP_HLEN;
	  memcpy((void *)((char*)p->payload),rcv_buffer,data_len);
	  msg->snd_code = udp_send(ipcb,p);	
	}
      }
      return 1;
    }

  case OC_NetSys_UDPReceive:
    {
      struct udp_pcb *ipcb = NULL;
            
      DEBUGSESSION kprintf(KR_OSTREAM,"UDP receive");
      
      /* Find in udp_pcb list if there exists such an ssid. If no such
       * ssid - return error */
      for(ipcb = udp_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      if(ipcb == NULL) msg->snd_code = RC_NetSys_NoExistingSession;
      else {
	if(ipcb->local_port == 0) msg->snd_code = RC_NetSys_UDPNoBind;
	else {
	  /* Are we listening or have we received data */
	  if(!ipcb->listening) { 
	    ipcb->timeout = msg->rcv_w1;
	    ipcb->overtime = 0;
	    ipcb->timeup = 0;
	    ipcb->listening = 1;
	    parkSession(msg);
	    return 1;
	  }else {
	    if(ipcb->overtime == 1) {
	      /* The client has timed out - poison pill return */
	      msg->snd_data = NULL;
	      msg->snd_len = 0;
	      msg->snd_code = RC_NetSys_UDPReceiveTimedOut;
	      ipcb->timeout = 0; /* reset the timeout */
	      ipcb->overtime = 0; /* and the overtime flag */
	      ipcb->listening = 0;
	      kprintf(KR_OSTREAM,"Session given timeout poison pill");
	      return 1;
	    }else {
	      /* We have been woken up by the network system and asked to
	       * retry since there is some data for us. So copy out data */
	      struct pbuf *q;
	      int totlen = 0;
	      int i;
	      
	      q = ipcb->pbuf;
	      
	      for(q = ipcb->pbuf; q != NULL; q = q->next) {
		for(i=0;i<q->len;i++) {
		  ((char *)rcv_buffer)[totlen++] = ((char *)q->payload)[i];
		}
	      }
	      
	      msg->snd_data = &rcv_buffer[0];
	      msg->snd_len = totlen;
	      msg->snd_code = RC_OK;
	      ipcb->listening = 0;
	      pbuf_free(ipcb->pbuf);
	      return 1;
	    }
	  }
	}
      }
      return 1;
    }

  case OC_NetSys_UDPClose:
    {
      struct udp_pcb *ipcb = NULL;
      
      DEBUGSESSION kprintf(KR_OSTREAM,"UDP close");
      
      /* find the pcb, free it and any local ports, pbuf associated with it */
      for(ipcb = udp_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      
      if(ipcb == NULL) msg->snd_code = RC_NetSys_NoExistingSession;
      else {
	ipcb->ssid = -1;
	pbuf_free(ipcb->pbuf);
	ports[ipcb->local_port] = 0;
	udp_remove(ipcb);
      }
      return 1;
    }
  
  case OC_NetSys_ICMPOpen:
    {
      msg->snd_code = icmpsession_open(ssid);
      return 1;
    }
  case OC_NetSys_ICMPPing:
    {
      if(find_icmpsession(ssid) == -1) {
	msg->snd_code = RC_NetSys_NoExistingSession;
	return 1;
      }else {
	struct ip_addr dest,src;
	struct pbuf *p;
	
	p = pbuf_alloc(PBUF_IP,MIN(msg->rcv_limit,msg->rcv_sent),PBUF_RAM);
	if(p == NULL) {
	  msg->snd_code = RC_NetSys_PbufsExhausted;
	}else {
	  int i;
	  
	  for(i=0;i<MIN(msg->rcv_limit,msg->rcv_sent);i++) {
	    ((char *)p->payload)[i] = rcv_buffer[i];
	  }
	  dest.addr = msg->rcv_w1;
	  src.addr = 0x0;
	  msg->snd_code = ip_output(p,&src,&dest,255,IP_PROTO_ICMP);
	  pbuf_free(p);
	}
	return 1;
      }
    }
  case OC_NetSys_ICMPReceive:
    {
      int index = find_icmpsession(ssid);
      
      if(index == -1) {
	msg->snd_code = RC_NetSys_NoExistingSession;
	return 1;
      }else {
	if(!IsIcmpListening(index)) { 
	  setIcmpListening(index);
	  parkSession(msg);
	  return 1;
	}
	else {
	  msg->snd_w1 = RC_NetSys_PingReplySuccess;
	  resetIcmpListening(index);
	  return 1;
	}
      }
    }
  case OC_NetSys_ICMPClose:
    {
      msg->snd_code = icmpsession_close(ssid);
      return 1;
    }
  case OC_eros_domain_net_ipv4_netsys_tcp_listen:
    {
      struct tcp_pcb *ipcb = NULL;
     
      /* Find in tcp_pcb list if there exists such an pcb */
      for(ipcb = tcp_active_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      if (ipcb != NULL){
	ipcb = tcp_listen(ipcb);
	tcp_accept(ipcb,tcp_listen_connected);
	msg->snd_code = RC_OK;
      }else {
	msg->snd_code = RC_NetSys_NoExistingSession;
      }
      return 1;
    }

  case OC_eros_domain_net_ipv4_netsys_tcp_bind:
    {
      struct ip_addr local_ip;
      uint16_t local_port;
      struct tcp_pcb *ipcb = NULL;
      
      local_ip.addr = msg->rcv_w1;
      if (local_ip.addr == 0) local_ip.addr = NETIF.ip_addr.addr;
      local_port = (uint16_t)msg->rcv_w2;
      
      DEBUGSESSION kprintf(KR_OSTREAM,"bind::ssid=%x,ip=%x,port=%d",
			   ssid,local_ip.addr,local_port);
      
      if(ports[local_port]) msg->snd_code = RC_NetSys_PortInUse;
      else {
	/* Find in tcp_pcb list if there exists such an ssid. If not tcp_new
	 * a tcp_pcb and add to the linked list */
	for(ipcb = tcp_active_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	  if(ipcb->ssid == ssid) break;
	}
	
	if(ipcb == NULL) {
	  /* Need to create a new tcp_pcb */
	  struct tcp_pcb *p;
	  
	  p = tcp_new();
	  if(p == NULL) msg->snd_code = RC_NetSys_MEMPExhausted;
	  else {
	    p -> ssid = ssid; /* Stamp our pcb */
	    msg->snd_code = tcp_bind(p,&local_ip,local_port);
	    ports[local_port] = 1;
	  }
	}else {
	  /* We have our pcb just call tcp_bind on it */
	  msg->snd_code = tcp_bind(ipcb,&local_ip,local_port);
	  ports[local_port] = 1;
	}
      }
      return 1;

    }

  case OC_eros_domain_net_ipv4_netsys_tcp_connect:
    {
      struct ip_addr remote_ip;
      uint16_t remote_port;
      struct tcp_pcb *ipcb = NULL;
      result_t result;
      
      remote_ip.addr = msg->rcv_w1;
      remote_port = (uint16_t)msg->rcv_w2;
      
      DEBUGSESSION kprintf(KR_OSTREAM,"Connect::ssid=%x,ip=%x,port=%d",ssid,
			   remote_ip.addr,remote_port);

      /* Find in tcp_pcb list if there exists such an ssid. If not tcp_new
       * a tcp_pcb and add to the linked list */
      for(ipcb = tcp_active_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      
      switch(ipcb->session_state) {
      case ACTIVE_PRECONNECT:
	{
	  if(ipcb == NULL) {
	    /* Need to create a new tcp_pcb */
	    ipcb = tcp_new();
	    ipcb->local_ip =  NETIF.ip_addr;
	    kprintf(KR_OSTREAM,"Newing a tcp");
	    if(ipcb == NULL) {
	      msg->snd_code = RC_NetSys_MEMPExhausted;
	      return 1;
	    }
	  }else {
	    if(ipcb->session_state != ACTIVE_PRECONNECT) {
	      msg->snd_code = RC_NetSys_TCPAlreadyConnected;
	      return 1;
	    }
	  }
	  ipcb->ssid = ssid; /* Stamp our pcb */
	  result = tcp_connect(ipcb,&remote_ip,remote_port,
			       tcp_after_connected);
	  kprintf(KR_OSTREAM,"Our local port = %d result = %d\n",
		  ipcb->local_port,result);
	  if(result != RC_OK) {
	    msg->snd_code = RC_NetSys_TCPConnectFailed;
	    return 1;
	  }else {
	    /* The net system has sent out a handshake initiation.
	     * Park and retry when connect is successful */
	    ipcb->timeout = 1000;
	    kprintf(KR_OSTREAM,"Session %d put to sleep",ssid);
	    parkSession(msg);
	    ipcb->session_state = ACTIVE_CONNECTING;
	    return 1;
	  }
	  return 1;
	  break;
	}
      case ACTIVE_ESTABLISHED:
	{
	  /* This is a retried call, after the network stack woke up
	   * the connecting client as a result of a successful connection
	   * Hence return back to the client with a success code */
	  
	  if(ipcb == NULL) { /* This should never happen!! BAD*/
	    msg->snd_code = RC_NetSys_NoExistingSession;
	  }else {
	    ipcb->session_state = ACTIVE_ESTABLISHED;
	    msg->snd_code = RC_OK;
	  }
	  return 1;
	  break;
	}
      case ACTIVE_TIMEDOUT:
	{
	  /* The connect call has timed out on the host. This is a retried
	   * call after the timeout agent gave this client the 'poison pill'
	   * on the connect blocking call */
	  kprintf(KR_OSTREAM,"ssid connect call poison pill",ssid);
	  if(ipcb == NULL) { /* This should never happen!! BAD*/
	    msg->snd_code = RC_NetSys_NoExistingSession;
	  }else {
	    ipcb->session_state = ACTIVE_PRECONNECT;
	    msg->snd_code = RC_NetSys_TCPConnectTimedOut;
	  }
	  return 1;
	  break;
	}
      default:
	kprintf(KR_OSTREAM,"ssid connect call default");
	msg->snd_code = RC_NetSys_TCPConnectFailed;
	tcp_pcb_remove(&tcp_active_pcbs,ipcb);            
	memp_free(MEMP_TCP_PCB,ipcb);
	return 1;
	break;
      }
      return 1;
    }
  case OC_NetSys_TCPSend:
    {
      struct tcp_pcb *ipcb = NULL;
      uint32_t copy = msg->rcv_w1;
      uint32_t err ;
      DEBUGSESSION kprintf(KR_OSTREAM,"TCP send on ssid=%x",ssid);
      
     
      /* Find in tcp_pcb list if there exists such an ssid. If no such
       * ssid - return error */
      for(ipcb = tcp_active_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      
      DEBUGSESSION kprintf(KR_OSTREAM,"1.ipcb.ssid = %d",ipcb->ssid);
      
      if(ipcb == NULL) msg->snd_code = RC_NetSys_NoExistingSession;
      else {
	/* Allocate a pbuf to take in data and do a tcp_send on ipcb */	
	uint16_t data_len = MIN(msg->rcv_limit,msg->rcv_sent);
	
	err = tcp_write(ipcb,&rcv_buffer[0],data_len,copy);
	if (err == RC_OK && ipcb->unacked == NULL) {
	  msg->snd_code = tcp_output(ipcb);
	}
      }
      tcp_recv(ipcb,tcp_after_recv);
      return 1;
    }
  case OC_NetSys_TCPReceive:
    { 
      struct tcp_pcb *ipcb = NULL;
      //int len = msg->rcv_w1;
      DEBUGSESSION kprintf(KR_OSTREAM,"TCP receive");
      
      /* Find in tcp_pcb list if there exists such an ssid. If no such
       * ssid - return error */
      for(ipcb = tcp_active_pcbs; ipcb!= NULL; ipcb = ipcb->next)  {
	if(ipcb->ssid == ssid)  {
	  DEBUGSESSION  kprintf(KR_OSTREAM,"TCP Receive:Found pcb");
	  break;
	}
      }
      if(ipcb == NULL) {
	DEBUGSESSION kprintf(KR_OSTREAM,"TCP Receive:Didnt find pcb");
	msg->snd_code = RC_NetSys_NoExistingSession;
      }
      else {
	DEBUGSESSION kprintf(KR_OSTREAM,"TCP Receive:b4 calling tcp_recv");
	tcp_recv(ipcb,tcp_after_recv);
	kprintf(KR_OSTREAM,"TCP Receive:after calling tcp_recv");
	msg->snd_data = &rcv_buffer[0];
	//msg->snd_len = totlen;
	msg->snd_code = RC_OK;
	//pbuf_free(pbuf);
      }
      return 1;
    }
  case OC_eros_domain_net_ipv4_netsys_tcp_close:
    {
      struct tcp_pcb *ipcb = NULL;
      
      DEBUGSESSION kprintf(KR_OSTREAM,"TCP close being called");
      
      /* find the pcb, free it and any local ports, pbuf associated with it */
      for(ipcb = tcp_active_pcbs; ipcb!= NULL; ipcb = ipcb->next) {
	if(ipcb->ssid == ssid) break;
      }
      
      if(ipcb == NULL) msg->snd_code = RC_NetSys_NoExistingSession;
      else {
	ipcb->ssid = -1;
	ports[ipcb->local_port] = 0;
	tcp_close(ipcb);
      }
      msg->snd_code = RC_OK;      
      return 1;
    }
  default:
    {
      msg->snd_code = RC_capros_key_UnknownRequest;
    }
  }
  
  return 1;
}
