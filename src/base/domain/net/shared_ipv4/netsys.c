/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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

#include <string.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/stdarg.h>
#include <eros/cap-instr.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/net/shared_ipv4/netsys.h>
#include <idl/capros/net/enet/enet.h>

#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/EnetKey.h>

#include <forwarder.h>
#include <addrspace/addrspace.h>

#include "constituents.h"

#include "netsyskeys.h"             /* All slots defined here */
#include "Session.h"

/* The shared memory manager for the netsys */
#include "memmgr/mmgr_commands.h" 

#include "netif/netif.h"         /* Network interface declarations */
#include "include/memp.h"        /* memp - allocation for pcbs */
#include "include/mem.h"         /* mem - allocation for RAM */
#include "include/udp.h"         /* udp functions */
#include "include/dhcp.h"        /* dhcp */
#include "include/etharp.h"      /* ARP table entries */
#include "include/icmp.h"        /* icmp routines */
#include "include/tcp.h"         /* tcp functions */
#include "include/pstore.h"
#include "include/TxRxqueue.h"   /* Transmission Queue */

#define DEBUGNETSYS   if(0)

/* Globals */
extern char* rcv_buffer; 
struct pstore_queue *TxPstoreQ;   /* The Tx pstore Queue */
int release_xmitr = 0;
int recv_docked = 0;              /* recv helper docked with us */
int xmit_docked = 0;              /* xmit helper docked with us */ 
int xmit_running = 0;

/** Initialization routine
 * 1. Probe for network card ( for now only lance is supported ).
 * 2. Initialize the network card.
 * 3. Initialize memp(pcb store) & pstore
 * 4. Initialize IP,UDP,TCP layers.
 */
uint32_t 
Initialize() 
{
  uint32_t result;
  
  /* Initialize the memp pool of memory. pstores are allocated from here */
  memp_init();
  
  /* Initialize the mem RAM memory. Storag7e for PSTORE_RAM comes from here */
  mem_init();
  
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

/* Pass over the client space keys to the enet domain */
uint32_t 
pass_enet_client_keys(cap_t key0,cap_t key1,cap_t key2,cap_t key3,int ssid)
{
  Message msg;
  result_t result;
  
  /* Buy a node and stick these keys in. Then pass the node key */
  result = spcbank_buy_nodes(KR_BANK, 1, KR_SCRATCH, KR_VOID, KR_VOID);
  if (result != RC_OK)
    return result;
  
  node_swap(KR_SCRATCH,0,key0,KR_VOID);
  node_swap(KR_SCRATCH,1,key1,KR_VOID);
  node_swap(KR_SCRATCH,2,key2,KR_VOID);
  node_swap(KR_SCRATCH,3,key3,KR_VOID);
    
  memset(&msg,0,sizeof(msg));
  msg.snd_invKey = KR_ENET;
  msg.snd_code = OC_eros_domain_net_enet_enet_map_client_space;
  msg.snd_key0 = KR_SCRATCH;
  
  /* FIX:: This is the stack id slot. Should be assigned by
   * the enet and retrieved through the wrapped start key passed 
   * by the enet */
  msg.snd_w1 = 1;
  msg.snd_w2 = ssid;
  CALL(&msg);
  return RC_OK;
}


/* The loop which is run each time we are interrupted by the xmit helper,
 * the recv helper, or a client request to send */
static inline void 
stackpingpong(int MAX_TRIES) 
{
  int continuework_rx = 1; /* continuing rx work */
  int i;

  for(i=0;i<MAX_TRIES;i++) {
    /*FIX:Add client ring servicing routine */
    while(continuework_rx) {
      continuework_rx = rx_service();
      if(continuework_rx) release_xmitr = 0;
    }
  }
  
  if(recv_docked) release_xmitr = 0;
  if(release_xmitr && xmit_docked && !xmit_running) {
    node_wake_some_no_retry(KR_PARK_NODE,0,0,/*ssid = 0*/0);
  }
  return;
}


/* Invoke functions based on the interface info in the 
 * keyInfo field of the received start key */
static int
ProcessRequest(Message *msg) 
{
  /* Dispatch the request on the appropriate interface */
  switch(msg->rcv_keyInfo) {
  case eros_domain_net_shared_ipv4_netsys_NETWORK_SYSTEM_INTERFACE:
    {
      DEBUGNETSYS kprintf(KR_OSTREAM,"network system interface");

      if(msg->rcv_code == OC_NetSys_GetSessionCreatorKey) {
	/* Make a start key to the session creator. This key will get handed 
	 * out and used by clients to create unique sessions */
	capros_Process_makeStartKey(KR_SELF,
			       eros_domain_net_shared_ipv4_netsys_SESSION_CREATOR_INTERFACE,
			       KR_SCRATCH);
	msg->snd_key0 = KR_SCRATCH;
	msg->snd_code = RC_OK;
      }
      
      if(msg->rcv_code == OC_NetSys_GetNetConfig) {
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
  case eros_domain_net_shared_ipv4_netsys_SESSION_CREATOR_INTERFACE:
    {
      /* until proven otherwise */
      uint32_t result = RC_NetSys_NoSessionAvailable; 
      uint32_t ssid;

      DEBUGNETSYS kprintf(KR_OSTREAM,"session creator interface");
      
      if(msg->rcv_code == OC_NetSys_GetNewSessionKey) {
	uint32_t xmit_client_buffer;
	uint32_t xmit_stack_buffer;
	uint32_t recv_client_buffer;
	uint32_t recv_stack_buffer;
	
	/* Get the new session id */
	ssid = new_session();

	/* Map the client's sub space into our address space */
	result = mmgr_MapClient(KR_CLIENT_BANK, KR_XMIT_CLIENT_BUF, 
				&xmit_client_buffer);
	if(result != RC_OK) {
	  kprintf(KR_OSTREAM,"Netsys unsuccessfully mapped client %d",result);
	  return RC_NetSys_BankRupt;
	}
	

	/* Map the client's sub space into our address space */
	result = mmgr_MapClient(KR_CLIENT_BANK, KR_XMIT_STACK_BUF, 
				&xmit_stack_buffer);
	if(result != RC_OK) {
	  kprintf(KR_OSTREAM,"Netsys unsuccessfully mapped client %d",result);
	  return RC_NetSys_BankRupt;
	}

	/* Map the client's sub space into our address space */
	result = mmgr_MapClient(KR_CLIENT_BANK, KR_RCV_CLIENT_BUF, 
				&recv_client_buffer);
	if(result != RC_OK) {
	  kprintf(KR_OSTREAM,"Netsys unsuccessfully mapped client %d",result);
	  return RC_NetSys_BankRupt;
	}
	
	/* Map the client's sub space into our address space */
	result = mmgr_MapClient(KR_CLIENT_BANK, KR_RCV_STACK_BUF, 
				&recv_stack_buffer);
	if(result != RC_OK) {
	  kprintf(KR_OSTREAM,"Netsys unsuccessfully mapped client %d",result);
	  return RC_NetSys_BankRupt;
	}
	
	/* Initialize the pstore structures in the 4 sectors */
	pstore_init(0,xmit_client_buffer);
	pstore_init(1,xmit_stack_buffer);
	pstore_init(2,recv_client_buffer);
	pstore_init(3,recv_stack_buffer);

	/* Pass the sub space keys to enet so that enet can map these
	 * spaces too */
	result = pass_enet_client_keys(KR_XMIT_CLIENT_BUF,KR_XMIT_STACK_BUF,
				       KR_RCV_CLIENT_BUF,KR_RCV_STACK_BUF,
				       ssid);
	if(result != RC_OK) {
	  kprintf(KR_OSTREAM,"Netsys:Enet client keys not mapped %d",result);
	  return RC_ENET_client_space_full;
	}
	
	/* Make this a read-only space for the client */
	//node_make_node_key(KR_SCRATCH2,3/*lss=3*/,SEGPRM_RO,KR_RCV_CLIENT_BUF);
		
	/* Make a start key to the session interface. This key will be
	 * wrapped in each unique session forwarder key that's handed out
	 * to clients */
	capros_Process_makeStartKey(KR_SELF,
			       eros_domain_net_shared_ipv4_netsys_SESSION_INTERFACE,
			       KR_SCRATCH);
  	result = forwarder_create(KR_CLIENT_BANK,KR_NEW_SESSION,KR_NEW_NODE,
				  KR_SCRATCH,
                                  capros_Forwarder_sendWord,
				  ssid);
	if(result != RC_OK) {
	  msg->snd_code = result;
	  return RC_NetSys_BankRupt;
	}
	
	msg->snd_key0 = KR_NEW_SESSION;

	/* The sub space created from the client's bank. The client
	 * must map this into his address space to access data */
	msg->snd_key1 = KR_XMIT_CLIENT_BUF;  
	msg->snd_key2 = KR_RCV_CLIENT_BUF;  
	
	/* Before we return, stash the client's space bank key in an
	 * unused slot of the forwarder.  When the session is closed,
	 * we'll make an attempt to return storage to the space bank on
	 * behalf of the client. */
	capros_Forwarder_swapSlot(KR_NEW_NODE, STASH_CLIENT_BANK, KR_CLIENT_BANK, KR_VOID);
	
	/* Now Add this newly created session to a list of sessions.
	 * This is essential to map the session id to the buffer addresses */
	msg->snd_code = activate_session(ssid,xmit_client_buffer,
					 xmit_stack_buffer,
					 recv_client_buffer,
					 recv_stack_buffer);
      }else if( msg->rcv_code == OC_NetSys_CloseSession) {
	/* FIX::Return the allocated space to the client and unmap the 
	 * space in our address space */
	//teardown_session(ssid);
      }
    }
    break;
  case eros_domain_net_shared_ipv4_netsys_SESSION_INTERFACE:
    {
      DEBUGNETSYS kprintf(KR_OSTREAM,"session type interface");
      return SessionRequest(msg);      
    }
    break;
  case eros_domain_net_shared_ipv4_netsys_TIMEOUT_AGENT_INTERFACE:
    {
      /* Our timeout agent has called.
       * Wake up any clients who have potentially 
       * timed out. Also use this opportunity to call
       * the tcp_tmr function for servicing tcp
       * timeouts 
       */
      tcp_tmr(); 
      session_timeouts_update();
      stackpingpong(1);
      
      return 1;
    }
  case eros_domain_net_shared_ipv4_netsys_XMIT_HELPER_INTERFACE:
    {
      xmit_docked = 1;
      xmit_running = 1;
      stackpingpong(1);
      xmit_running = 0;
      
      /* Check if the pstoreQ is empty if yes park the xmit helper */
      if(!release_xmitr) {
	msg->rcv_w1 = 0;
	parkSession(msg);
      }else {
	/* Go ahead and return the call successully. The xmit helper 
	 * will in turn call the enet */
	release_xmitr = 0;
	xmit_docked = 0;
      }
      return 1;
    }
  case eros_domain_net_shared_ipv4_netsys_RECV_HELPER_INTERFACE:
    {
      /* We have been called by the receive helper. Indicating that data
       * has arrived for us on pstoreRxQ. Check it out 
       * Look if there is any new buffer in the ring filled up in
       * either our stack_space or the client's stack space */
      recv_docked = 1;
      stackpingpong(1);
      recv_docked = 0;
      return 1;
    }
  default :
    {
      kprintf(KR_OSTREAM,"Unknown type invocation");
      msg->snd_code = RC_capros_key_UnknownRequest;
    }
  }
  return 1;
}

/* Create a region of memory from the stack's bank to be shared between
 * the stack and the enet domains. This is used to xmit and receive 
 * data packets like etharp replies, queries and icmp replies etc. */
uint32_t
create_stack_enet_space() 
{
  result_t result;
  uint32_t stack_enet_space_RW;
  uint32_t stack_enet_space_RO;
  Message msg;
  
  result = mmgr_MapClient(KR_BANK, KR_XMIT_STACK_BUF,&stack_enet_space_RO);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Netsys unsuccessfully mappedenetstack space %d",
	    result);
    return RC_NetSys_BankRupt;
  }

  result = mmgr_MapClient(KR_BANK, KR_RCV_STACK_BUF,&stack_enet_space_RW);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Netsys unsuccessfully mappedenetstack space %d",
	    result);
    return RC_NetSys_BankRupt;
  }
  
  /* format the shared space */
  pstore_init(XMIT_STACK_SPACE,stack_enet_space_RO);
  pstore_init(RECV_STACK_SPACE,stack_enet_space_RW);
  
  pstoreQ_init(stack_enet_space_RO,&TxPstoreQ);
  
  DEBUGNETSYS {
   kprintf(KR_OSTREAM,"nTX/Rx Queue starts at %08x",&TxPstoreQ[0]);
  }
  
  /* ssid = 0:: our own transmit & receive buffers */
  activate_session(0,stack_enet_space_RO,stack_enet_space_RO,
		   stack_enet_space_RW,stack_enet_space_RW);
  
  DEBUGNETSYS debug_active_sessions();

  /* Call enet and pass both these keys */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_ENET;
  msg.snd_code =  OC_eros_domain_net_enet_enet_map_stack_space;
  msg.snd_key0 = KR_XMIT_STACK_BUF;
  msg.snd_key1 = KR_RCV_STACK_BUF;
  CALL(&msg);
 
  return RC_OK;
}

/* Construct the enet domain */
uint32_t 
startEnet() 
{
  result_t result;
  Message msg;
  
  /* construct the Enet domain */
  result = constructor_request(KR_ENET,KR_BANK,KR_SCHED,
                               KR_CONSTREAM,KR_ENET);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "netsys:Constructing xmit helper...[FAILED]\n");
    return result;
  }
  
  /* Call enet and pass both these keys */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_ENET;
  msg.snd_key0 = KR_BANK;
  msg.snd_code =  OC_eros_domain_net_enet_enet_get_wrapper;
  msg.rcv_key1 = KR_ENET_BLOCKED;
  CALL(&msg);
  
  DEBUGNETSYS kprintf(KR_OSTREAM,"Netsys: enet started successfully");

  /* Now get the appropriate hardware information */
  msg.snd_invKey = KR_ENET;
  msg.snd_code =  OC_eros_domain_net_enet_enet_gethwconfig;
  msg.rcv_data = &NETIF;
  msg.rcv_limit = sizeof(NETIF);
  CALL(&msg);
  
  DEBUGNETSYS {
    kprintf(KR_OSTREAM,"Netsys::MAC Addr %02x:%02x:%02x:%02x:%02x:%02x\n",
  	  NETIF.hwaddr[0],NETIF.hwaddr[1],NETIF.hwaddr[2],
	  NETIF.hwaddr[3],NETIF.hwaddr[4],NETIF.hwaddr[5]);
    kprintf(KR_OSTREAM,"Netsys:: Netif MTU = %d",NETIF.mtu);
  }
  
  return RC_OK;
}

/* Start the helpers - xmit helper and the recv helper and hand them 
 * our key as well as key to enet */
uint32_t 
startXRhelpers() 
{
  result_t result;
  Message msg;

  /* make a interface specific start key to self  */
  capros_Process_makeStartKey(KR_SELF,
			 eros_domain_net_shared_ipv4_netsys_XMIT_HELPER_INTERFACE,
			 KR_SCRATCH);
  /* create a forwarder so that we can block this caller */
  result = forwarder_create(KR_BANK,KR_SCRATCH2,KR_NEW_NODE, KR_SCRATCH,
                            capros_Forwarder_sendWord,
			    /*ssid = 0*/0 );

  /* Pass it our start key */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_XMIT_HELPER;
  msg.snd_key0 = KR_SCRATCH2;
  msg.snd_key1 = KR_ENET;
  CALL(&msg);
  
  /* make a interface specific start key to self  */
  capros_Process_makeStartKey(KR_SELF,
			 eros_domain_net_shared_ipv4_netsys_RECV_HELPER_INTERFACE,
			 KR_SCRATCH);
  
  /* Pass it our start key */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_RECV_HELPER;
  msg.snd_key0 = KR_SCRATCH;
  msg.snd_key1 = KR_ENET;
  CALL(&msg);

  DEBUGNETSYS kprintf(KR_OSTREAM,"starting XR helpers done ");
  return RC_OK;
}


/* Start the timeout helper. This helps us keep timeouts of sessions
 * and also acts as a tcp tmr for retransmissions etc. */
uint32_t 
startTimeoutAgent(cap_t KrAlarm) 
{
  Message msg;
  
  /* make a interface specific start key to self  */
  capros_Process_makeStartKey(KR_SELF,
			 eros_domain_net_shared_ipv4_netsys_TIMEOUT_AGENT_INTERFACE,
			 KR_SCRATCH);
  
  /* Pass it our start key */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_TIMEOUT_AGENT;
  msg.snd_key0 = KR_SCRATCH;
  CALL(&msg);
  DEBUGNETSYS kprintf(KR_OSTREAM, "NetSys:Constructing Alarm ... [SUCCESS]");
  
  return RC_OK;
}

/* After we have prep'd the addrspace for mapping. We still need to 
 * fill up the slots in 17 - 23 with lss 3 nodes. So that we can use 
 * lss 3 subspaces while mapping clients */
static uint32_t
netsys_map_lss_three_layer(cap_t kr_self, cap_t kr_bank,
                           cap_t kr_scratch, cap_t kr_node, uint32_t next_slot)
{
  uint32_t slot;
  uint32_t lss_three = 3;
  uint32_t result;

  capros_Process_getAddrSpace(kr_self, kr_scratch);
  for (slot = next_slot; slot < EROS_NODE_SIZE; slot++) {
    result = addrspace_new_space(kr_bank, lss_three, kr_node);
    if (result != RC_OK)
      return result;

    result = node_swap(kr_scratch, slot, kr_node, KR_VOID);
    if (result != RC_OK)
      return result;
  }
  return RC_OK;
}

int 
main(void)
{
  result_t result;
  
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_MEMMAP_C,KR_MEMMAP_C);
  node_extended_copy(KR_CONSTIT,KC_SLEEP,KR_SLEEP);
  node_extended_copy(KR_CONSTIT,KC_ENET,KR_ENET);
  node_extended_copy(KR_CONSTIT,KC_XMIT_HELPER_C,KR_XMIT_HELPER);
  node_extended_copy(KR_CONSTIT,KC_RECV_HELPER_C,KR_RECV_HELPER);
  node_extended_copy(KR_CONSTIT,KC_CONSTREAM_C,KR_CONSTREAM);
  node_extended_copy(KR_CONSTIT,KC_TIMEOUT_AGENT_C,KR_TIMEOUT_AGENT);
  node_extended_copy(KR_CONSTIT,KC_ZSC,KR_ZSC);

  /* Prepare our addrspace for memory mapping of client sub spaces & 
   * DMA-able ranges */
  if (addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, 
                                 KR_NEW_NODE) != RC_OK)
    kdprintf(KR_OSTREAM, "**ERROR: netsys call to prep addrspace failed!\n");
  
  /* Set up client shared subspace */
  kprintf(KR_OSTREAM, "NetSystem calling map_lss_three_layer "
	  "with next slot=%u...", 17);
  if (netsys_map_lss_three_layer(KR_SELF, KR_BANK, KR_SCRATCH, 
				 KR_NEW_NODE,17) != RC_OK)
    kdprintf(KR_OSTREAM, "**ERROR: netsys call to map lss three "
    	     "failed!\n");

  /* Start the alarm process - we may need it later */
  result = startTimeoutAgent(KR_TIMEOUT_AGENT);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys: Starting Alarm Process ... [FAILED]");
    return -1; /* FIX:: Better way of signalling failure */
  }
  
  /* Make a generic start key to self  */
  capros_Process_makeStartKey(KR_SELF,
                         eros_domain_net_shared_ipv4_netsys_NETWORK_SYSTEM_INTERFACE,
                         KR_START);
  
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
    kprintf(KR_OSTREAM, "** ERROR: couldn't block a forwarder for parking "
            "waiting-client keys...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }
  
  /* FIX:: For now we construct the enet. We must move it into
   * some other constructor later */
  result = startEnet();
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys: Starting Enet ... [FAILED]");
    return -1; /* FIX:: Better way of signalling failure */
  }
  
  /* Initialize the sessions manager */
  init_sessions();
  
  result = create_stack_enet_space();
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys:creating stack enet space ... [FAILED]");
    return -1; /* FIX:: Better way of signalling failure */
  }
  
  result = startXRhelpers();
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys: Starting XR helpers ... [FAILED]");
    return -1; /* FIX:: Better way of signalling failure */
  }
 
  /* Initialize the network subsystem */
  result = Initialize();

  {
    Message msg;
    
    memset(&msg,0,sizeof(Message));
    msg.snd_invKey = KR_RETURN;
    msg.snd_key0   = KR_START; /* Send back the generic start key */
    msg.snd_rsmkey = KR_RETURN;
    msg.snd_code = result;
    
    msg.rcv_key0   = KR_ARG(0);
    msg.rcv_key1   = KR_ARG(1);
    msg.rcv_key2   = KR_ARG(2);
    msg.rcv_rsmkey = KR_RETURN;
    msg.rcv_data = &rcv_buffer;
    msg.rcv_limit  = MAX_BUF_SIZE;
    msg.invType = IT_PReturn;
    
    do {
      INVOKECAP(&msg);
      msg.snd_key0 = KR_VOID;
      msg.invType = IT_PReturn;
    } while (ProcessRequest(&msg));
  }
  
  return 0;
}
