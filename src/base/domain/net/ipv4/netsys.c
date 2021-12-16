/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution,
 * and is derived from the EROS Operating System distribution.
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

/* Network Subsystem */
#include <string.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/stdarg.h>
#include <eros/cap-instr.h>
#include <eros/KeyConst.h>

#include <forwarder.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/arch/i386/Process.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/net/ipv4/netsys.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/ConstructorKey.h>
#include <domain/drivers/NetKey.h>
#include <domain/NetSysKey.h>

#include "constituents.h"

#include "keyring.h"             /* All slots defined here */
#include "Session.h"
#include "netif/netif.h"         /* Network interface declarations */
#include "include/memp.h"        /* memp - allocation for pcbs */
#include "include/mem.h"         /* mem - allocation for RAM */
#include "include/udp.h"         /* udp functions */
#include "include/dhcp.h"        /* dhcp */
#include "include/etharp.h"      /* ARP table entries */
#include "include/icmp.h"        /* icmp routines */
#include "include/tcp.h"         /* tcp functions */

/* This service implements these interfaces */
#define NETWORK_SYSTEM_INTERFACE    0x000Bu
#define SESSION_CREATOR_INTERFACE   0x000Cu
#define SESSION_INTERFACE           0x000Du
#define HELPER_INTERFACE            0x000Eu
#define TIMEOUT_AGENT_INTERFACE     0x000Fu

#define DEBUGNETSYS   if(0)

/* Globals */
int total_interrupts = 0;
extern char* rcv_buffer;


/* Block timer */
void 
block_alarm(Message *msg) {
  msg->invType = IT_Retry;
  msg->snd_w1 = RETRY_SET_LIK|RETRY_SET_WAKEINFO;
  msg->snd_w2 = msg->rcv_w1; /* wakeinfo value */
  msg->snd_key0 = KR_PARK_WRAP;
}

/* wake timer */
void
wake_alarm() {
  node_wake_some_no_retry(KR_PARK_NODE,0,0,TIMEOUT_AGENT_INTERFACE);
}


/** Initialization routine
 * 1. Probe for network card ( for now only lance is supported ).
 * 2. Initialize the network card.
 * 3. Initialize memp(pcb store) & pbuf(pbuf store)
 * 4. Initialize IP,UDP,TCP layers.
 */
uint32_t 
Initialize() 
{
  uint32_t result;
  
  /* Probe for a lance card ( the only card we know ) */
  result = netif_probe();
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys: net card Probe (%d)... [ FAILED ]",result);
    return result;
  }
  
  /* Initialize the sessions manager */
  init_sessions();

  /* Initialize the memp pool of memory. pbufs are allocated from here */
  memp_init();
  
  /* Initialize the mem RAM memory. Storage for PBUF_RAM comes from here */
  mem_init();
  
  /* Initialize pbuf memory. Storage for sending, receiving packets 
   * (pbuf) comes from here */
  pbuf_init();
  
  /* Initialize the ARP table */
  etharp_init();
  
  /* Initialize the protocol layers ip,udp & tcp */
  udp_init();
  tcp_init();

  /* Start the dhcp client - attempt to get us an ip address */
  result = dhcp_start(&NETIF);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys:dhcp start ... [ FAILED ]",result);
    result = RC_NetSys_DHCPInitFailed;
    return result;
  }
  
  return RC_OK;
}

/* Invoke functions based on the interface info in the 
 * keyInfo field of the received start key */
int 
ProcessRequest(Message *msg) 
{
  /* Dispatch the request on the appropriate interface */
  switch(msg->rcv_keyInfo) {
  case NETWORK_SYSTEM_INTERFACE:
    {
      DEBUGNETSYS kprintf(KR_OSTREAM,"network system interface");
      
      if(msg->rcv_code == OC_NetSys_GetSessionCreatorKey) {
	msg->snd_key0 = KR_SESSION_CREATOR;
	msg->snd_code = RC_OK;
      }
      
      if(msg->rcv_code == OC_eros_domain_net_ipv4_netsys_get_netconfig) {
	kprintf(KR_OSTREAM,
		"NETIF: IP address of interface %c%c set to "
		"%u.%u.%u.%u",
		NETIF.name[0], NETIF.name[1], 
		(uint8_t)(ntohl(NETIF.ip_addr.addr) >> 24 & 0xff),
		(uint8_t)(ntohl(NETIF.ip_addr.addr) >> 16 & 0xff),
		(uint8_t)(ntohl(NETIF.ip_addr.addr) >> 8 & 0xff),
		(uint8_t)(ntohl(NETIF.ip_addr.addr) & 0xff));
  
	if(!ip_addr_isany(&NETIF.ip_addr)) {
	  msg->snd_w1 = NETIF.ip_addr.addr;
	  msg->snd_w2 = NETIF.netmask.addr;
	  msg->snd_w3 = NETIF.gw.addr;
	  msg->snd_code = RC_OK;
	}else {
	  msg->snd_code = RC_NetSys_NetworkNotConfigured;
	}
      }
    }
    break;
  case SESSION_CREATOR_INTERFACE:
    {
      /* until proven otherwise */
      uint32_t result = RC_NetSys_NoSessionAvailable; 
      uint32_t ssid;

      DEBUGNETSYS kprintf(KR_OSTREAM,"session creator interface");
      
      if(msg->rcv_code == OC_NetSys_GetNewSessionKey) {
	ssid = new_session();
	result = forwarder_create(KR_CLIENT_BANK,KR_NEW_SESSION,KR_NEW_NODE,
				  KR_SESSION_TYPE,
                                  capros_Forwarder_sendWord,
				  ssid);
	if(result != RC_OK) {
	  msg->snd_code = result;
	  return 1;
	}
	
	msg->snd_key0 = KR_NEW_SESSION;
	msg->snd_code = RC_OK;
	
	/* Before we return, stash the client's space bank key in an
	 * unused slot of the forwarder.  When the session is closed,
	 * we'll make an attempt to return storage to the space bank on
	 * behalf of the client. */
	capros_Forwarder_setBlocked(KR_NEW_NODE, STASH_CLIENT_BANK,
                                  KR_CLIENT_BANK, KR_VOID);
      }
    }
    break;
  case SESSION_INTERFACE:
    {
      DEBUGNETSYS kprintf(KR_OSTREAM,"session type interface");
      return SessionRequest(msg);      
    }
    break;
  case HELPER_INTERFACE:
    {
      total_interrupts ++;
      netif_interrupt();
      return 1;
    }
  case TIMEOUT_AGENT_INTERFACE:
    {
      /* Our timeout agent has called.
       * Wake up any clients who have potentially 
       * timed out. Also use this opportunity to call
       * the tcp_tmr function for servicing tcp
       * timeouts 
       */
      //msg->rcv_w1 = TIMEOUT_AGENT_INTERFACE;
      tcp_tmr(); /* Called the tcp */
      session_timeouts_update();
      
      DEBUGNETSYS kprintf(KR_OSTREAM,"timeout agent type interface");
      return 1;
    }
  default :
    {
      kprintf(KR_OSTREAM,"default request");
      msg->snd_code = RC_capros_key_UnknownRequest;
    }
  }
  return 1;
}

uint32_t 
startTimeoutAgent(cap_t KrAlarm) 
{
  Message msg;
  
  /* Pass it our start key */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_TIMEOUT_AGENT;
  msg.snd_key0 = KR_TIMEOUT_AGENT_TYPE;
  CALL(&msg);
  DEBUGNETSYS kprintf(KR_OSTREAM, "NetSys:Constructing Alarm ... [SUCCESS]");
  
  return RC_OK;
}

int 
main(void)
{
  Message msg;
  uint32_t result;
  
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);
  node_extended_copy(KR_CONSTIT,KC_MEMMAP_C,KR_MEMMAP_C);
  node_extended_copy(KR_CONSTIT,KC_PCI_PROBE_C,KR_PCI_PROBE_C);
  node_extended_copy(KR_CONSTIT,KC_PHYSRANGE,KR_PHYSRANGE);
  node_extended_copy(KR_CONSTIT,KC_SLEEP,KR_SLEEP);
  node_extended_copy(KR_CONSTIT,KC_HELPER,KR_HELPER);
  node_extended_copy(KR_CONSTIT,KC_CONSTREAM_C,KR_CONSTREAM);
  node_extended_copy(KR_CONSTIT,KC_TIMEOUT_AGENT,KR_TIMEOUT_AGENT);
  
  /* Move the DEVPRIVS key to the ProcIOSpace so we can do i/o calls */
  capros_Process_setIOSpace(KR_SELF, KR_DEVPRIVS);
  
  /* Make a generic start key to self  */
  capros_Process_makeStartKey(KR_SELF,TIMEOUT_AGENT_INTERFACE,
			 KR_TIMEOUT_AGENT_TYPE);
  
  /* Start the alarm process - we may need it later */
  result = startTimeoutAgent(KR_TIMEOUT_AGENT);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys: Starting Alarm Process ... [FAILED]");
    return -1; /* FIX:: Better way of signalling failure */
  }
  
  /* Make a generic start key to self  */
  capros_Process_makeStartKey(KR_SELF,NETWORK_SYSTEM_INTERFACE,KR_START);
  
  /* Make a start key to the session creator. This key will get handed 
   * out and used by clients to create unique sessions */
  capros_Process_makeStartKey(KR_SELF,SESSION_CREATOR_INTERFACE,KR_SESSION_CREATOR);
  
  /* Make a start key to the session interface. This key will be
   * wrapped in each unique session forwarder key that's handed out
   * to clients */
  capros_Process_makeStartKey(KR_SELF,SESSION_INTERFACE,KR_SESSION_TYPE);
  
  /* Make a start key for the helper. The helper uses this key to notify
   * us of IRQ5 events */
  capros_Process_makeStartKey(KR_SELF,HELPER_INTERFACE,KR_HELPER_TYPE);
  
  /* Create a forwarder for "parking" client keys that are waiting
   * for network input */
  if (forwarder_create(KR_BANK, KR_PARK_WRAP, KR_PARK_NODE, KR_START,
                       0, 0) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't create a forwarder for parking "
            "waiting-client keys...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* We want the forwarder to be initially blocked. */
  if (capros_Forwarder_setBlocked(KR_PARK_NODE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't create a forwarder for parking "
            "waiting-client keys...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Initialize the network subsystem */
  result = Initialize();
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START; /* Send back the generic start key */
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = result;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0   = KR_ARG(0);
  msg.rcv_key1   = KR_ARG(1);
  msg.rcv_key2   = KR_ARG(2);
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = &rcv_buffer;
  msg.rcv_limit  = MAX_BUF_SIZE;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  msg.invType = IT_PReturn;
  
  do {
    INVOKECAP(&msg);
    msg.snd_key0 = KR_VOID;
    msg.invType = IT_PReturn;
  } while (ProcessRequest(&msg));

  
  return 0;
}
