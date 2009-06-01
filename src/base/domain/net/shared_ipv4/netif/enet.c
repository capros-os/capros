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

/* This process interfaces the driver and the network stack */
#include <string.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/arch/i386/Process.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/DevPrivs.h>
#include <idl/capros/net/enet/enet.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/PciProbeKey.h>
#include <domain/drivers/NetKey.h>
#include <domain/EnetKey.h>

#include <addrspace/addrspace.h>
#include <forwarder.h>

#include "netif.h"
#include "netutils.h"
#include "enetkeys.h"
#include "enet_map.h"
#include "enet_session.h"
#include "enet.h"

#include "../include/opt.h"
#include "../include/udp.h"
#include "../include/tcp.h"

#include "lance.h"
#include "_3c905c.h"
#include "tg3.h"

#include "constituents.h"

#define DEBUG_ENET  if(0)

/* All the cards and vendors we know */
#define AMD           0x1022
#define LANCE         0x2621

#define THREE_COM     0x10b7
#define TORNADO       0x9200

#define BCOM          0x173B
#define ALTIMA        0x03EA

/* Globals */
struct pci_dev_data net_device;   /* The singular network card */
struct udp_pcb *udp_pcbs;
struct tcp_pcb *tcp_active_pcbs;
struct tcp_pcb *tcp_tw_pcbs;      
struct tcp_pcb_listen *tcp_listen_pcbs;  
int release_rcvr = 0;           /* Have we received anything on the nic */
int stack_mapped = 0;           /* Has the stack mapped the memory */
int recv_docked = 0;

#define STATS if(0)

/* Measurement counter */
int total_interrupts = 0;
uint32_t Qstart = PSTORE_POOL_SIZE*(PSTORE_POOL_BUFSIZE
				    + sizeof(struct pstore));

/* externs */
extern struct enet_client_session ActiveSessions[MAX_SESSIONS];

static inline uint32_t 
ProcessRequest(Message *msg) 
{
  result_t result;

  switch(msg->rcv_code) {
  case OC_eros_domain_net_enet_enet_gethwconfig:
    {
      /* Send back the netif structure */
      msg->snd_data = &NETIF;
      msg->snd_len = sizeof(NETIF);
      msg->snd_code = RC_OK;
      break;
    }
  case OC_eros_domain_net_enet_enet_get_wrapper:
    {
      capros_Process_makeStartKey(KR_SELF,0,KR_SCRATCH);
      result = forwarder_create(KR_ARG(0),KR_BLOCKER,KR_NEW_NODE, KR_SCRATCH,
                                capros_Forwarder_sendWord,
			        1);
      if(result != RC_OK) {
	msg->snd_code = result;
	return 1;
      }
      
      msg->snd_key1 = KR_BLOCKER;
      msg->snd_code = RC_OK;
      break;
    }
  case OC_eros_domain_net_enet_enet_irq_arrived:
    {
      //static int i;
      //kprintf(KR_OSTREAM,"Enet:irq in %d",++i);
      STATS{
	int start_addr  = ActiveSessions[0].mt[XMIT_STACK_SPACE].start_address;
	int *TxQ;
	total_interrupts++;
	TxQ = (void *)(2*Qstart + start_addr);
	TxQ[0] = total_interrupts;
      }
      NETIF.interrupt(); /* interrupts are disabled here */
      
      while(NETIF.rx() || tx_service()) {
      }
      if(release_rcvr && recv_docked) 	{
	wakeSession(/*Receiver thread*/1);
      }
	
      NETIF.enable_ints();
      //kprintf(KR_OSTREAM,"Enet:irq out %d",i);
      return 1;
    }
  case OC_eros_domain_net_enet_enet_recv_queue:
    {
      /* If we have something to send to the stack then return else 
       * park this client */
      //static int i,j,k;
            
      //kprintf(KR_OSTREAM,"Enet:Recv In");
      while(NETIF.rx() || tx_service()){}
      if(!release_rcvr) {
	msg->rcv_w1 = 1;
	parkSession(msg);
	recv_docked = 1;
	//kprintf(KR_OSTREAM,"Enet:Recv Parked  %d",++j);
      }else {
	/* Go ahead and return the call successully. The xmit helper 
	 * will in turn call the enet */
	release_rcvr = 0;
	//kprintf(KR_OSTREAM,"Enet:Recv Released  %d",++k);
	recv_docked = 0;
      }
      
      return 1;
    }
  case OC_eros_domain_net_enet_enet_xmit_queue:
    {
      /* FIX:: For now we assume only 1 stack but this is bound to
       * change. Then stack id must be unique and decided by enet 
       * But for now stack 0 = enet 0
       *         & stack 1 = enet 1 :: as for ssids */
      
      /* We have been to look for possibility of transmission.
       * So look at the ring buffers for fresh data posted and
       * transmit them */
      //static int i;
      
      //kprintf(KR_OSTREAM,"Enet:xmit In");
      while(NETIF.rx() || tx_service()) {}
      release_rcvr = 0;
      //kprintf(KR_OSTREAM,"Enet:xmit Out %d",i);
      
      return 1;
    }
  case OC_eros_domain_net_enet_enet_map_client_space:
    {
      uint32_t xmit_client_buffer;
      uint32_t xmit_stack_buffer;
      uint32_t rcv_client_buffer;
      uint32_t rcv_stack_buffer;
      
      DEBUG_ENET kprintf(KR_OSTREAM,"enet :client map stackid=%d ssid=%d",
			 msg->rcv_w1,msg->rcv_w2);
      
      node_extended_copy(KR_ARG(0),0,KR_XMIT_CLIENT_BUF);
      node_extended_copy(KR_ARG(0),1,KR_XMIT_STACK_BUF);
      node_extended_copy(KR_ARG(0),2,KR_RCV_CLIENT_BUF);
      node_extended_copy(KR_ARG(0),3,KR_RCV_STACK_BUF);
      
      /* Map the client's sub space into our address space */
      result = enet_MapClient(KR_BANK,KR_XMIT_CLIENT_BUF,KR_XMIT_STACK_BUF,
			      KR_RCV_CLIENT_BUF,KR_RCV_STACK_BUF,
			      &xmit_client_buffer,&xmit_stack_buffer,
			      &rcv_client_buffer,&rcv_stack_buffer);
			      
      if(result != RC_OK) {
	kprintf(KR_OSTREAM,"enet unsuccessfully mapped client %d",result);
	msg->snd_code =  RC_ENET_client_space_full;
	break;
      }
      
      /* The stack's id comes in w1, Client's id in rcv_w2*/
      msg->snd_code = enet_create_client_session(msg->rcv_w1,msg->rcv_w2,
						 xmit_client_buffer,
						 xmit_stack_buffer,
						 rcv_client_buffer,
						 rcv_stack_buffer);
    }
    break;
  case OC_eros_domain_net_enet_enet_map_stack_space:
    {
      uint32_t sector1_buffer;
      uint32_t sector2_buffer;
      uint32_t start_addr = 0;
      uint32_t Qstart = PSTORE_POOL_SIZE*(PSTORE_POOL_BUFSIZE
				      + sizeof(struct pstore));
      struct pstore_queue *TxQ;
      
      DEBUG_ENET kprintf(KR_OSTREAM,"enet : map stack interface");
      
      /* Map the client's sub space into our address space */
      result = enet_MapStack(KR_BANK,KR_ARG(0),KR_ARG(1), 
			     &sector1_buffer,&sector2_buffer);
      if(result != RC_OK) {
	kprintf(KR_OSTREAM,"enet unsuccessfully mapped client %d",result);
	msg->snd_code =  RC_ENET_stack_space_full;
	break;
      }
      
      stack_mapped = 1;
      enet_create_stack_session(1,sector1_buffer,sector2_buffer);
      
      /* Some predecided location from where we start our queues in the
       * shared area */
      start_addr = ActiveSessions[0].mt[XMIT_STACK_SPACE].start_address;
      TxQ = (void *)(2*Qstart + start_addr);
      
      udp_pcbs = (void *)((uint32_t)&TxQ[Qsize] + 
			  sizeof(struct pstore_queue)) + 4;
      
      tcp_active_pcbs = (void *)((uint32_t)&udp_pcbs[0] - 4 +
				 MEMP_NUM_UDP_PCB*(sizeof(struct udp_pcb) 
						   + 4) + 4);
      tcp_listen_pcbs = (void *)((uint32_t)&tcp_active_pcbs[0] -4 +
				 MEMP_NUM_TCP_PCB*(sizeof(struct tcp_pcb)
						   + 4) +4);
      DEBUG_ENET kprintf(KR_OSTREAM,"udp pcb start at %08x",&udp_pcbs[0]);
      DEBUG_ENET kprintf(KR_OSTREAM,"tcp pcb start at %08x",
			 &tcp_active_pcbs[0]);
      DEBUG_ENET kprintf(KR_OSTREAM,"tcp listen pcbs start at %08x",
			 &tcp_listen_pcbs[0]);
    }
    break;
  default:
    kprintf(KR_OSTREAM,"enet: default process request");
  }
  
  return 1;
}


/* After we have prep'd the addrspace for mapping. We still need to 
 * fill up the slots in 17 - 23 with lss 3 nodes. So that we can use 
 * lss 3 subspaces while mapping clients */
static uint32_t
enet_map_lss_three_layer(cap_t kr_self, cap_t kr_bank,
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

/* We do a pci probe looking for the cards we know and then
 * initialize the respective interface */
int
main(void)
{
  Message msg;
  result_t result;
  uint32_t total;
  
  node_extended_copy(KR_CONSTIT,KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT,KC_HELPER_C,KR_HELPER);
  node_extended_copy(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);
  node_extended_copy(KR_CONSTIT,KC_PHYSRANGE,KR_PHYSRANGE);
  node_extended_copy(KR_CONSTIT,KC_PCI_PROBE_C,KR_PCI_PROBE_C);
  node_extended_copy(KR_CONSTIT,KC_MEMMAP_C,KR_MEMMAP_C);
  node_extended_copy(KR_CONSTIT,KC_SLEEP,KR_SLEEP);

  COPY_KEYREG(KR_ARG(0),KR_CONSTREAM);

  /* Move the DEVPRIVS key to the ProcIOSpace so we can do i/o calls */
  capros_Process_setIOSpace(KR_SELF, KR_DEVPRIVS);
   
  /* Prepare address space for mapping */
  if (addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, 
                                 KR_NEW_NODE) != RC_OK)
    kdprintf(KR_OSTREAM, "**ERROR: enet call to prep addrspace failed!\n");
  
  /* Construct the pci probe domain */
  result = constructor_request(KR_PCI_PROBE_C,KR_BANK,KR_SCHED,
                               KR_VOID,KR_PCI_PROBE_C);
  if(result!=RC_OK) {
    msg.snd_code =  RC_PCI_PROBE_ERROR;
    goto RETURN_TO_CONSTRUCTOR;
  }
  
  /* Initialize the pci probe */
  result = pciprobe_initialize(KR_PCI_PROBE_C);
  if(result!=RC_OK) {
    msg.snd_code = RC_PCI_PROBE_ERROR;
    goto RETURN_TO_CONSTRUCTOR;
  }
  DEBUG_ENET kprintf(KR_OSTREAM,"PciProbe init ...  %s.\n",
		     (result == RC_OK) ? "SUCCESS" : "FAILED");

  /* First we look for AMD devices */
  result = pciprobe_vendor_total(KR_PCI_PROBE_C,AMD, &total);
  DEBUG_ENET {
    kprintf(KR_OSTREAM,"Searching for AMD Devices ... %s",
	    (result ==RC_OK) ? "SUCCESS" : "FAILED");
    kprintf(KR_OSTREAM,"No. of AMD Devices = %d",total);
  }
  
  if(total) {
    /* Get the Device ID to check if it is AMD_LANCE (PCNetPCI - II) 
     * From pci_ids.h the lance card had an ID 0x2000 */
    pciprobe_vendor_next(KR_PCI_PROBE_C,AMD,0,&net_device);
    DEBUG_ENET 
      kprintf(KR_OSTREAM,"RCV: 00:%02x [%04x,%04x] [%04x/%04x] BR[0] %x IRQ %d,MASTER=%s", 
	      net_device.devfn, net_device.subsystem_vendor,
	      net_device.subsystem_device,net_device.vendor,
	      net_device.device,net_device.base_address[0],
	      net_device.irq,net_device.master?"YES":"NO");
    
    /* Go ahead and initialise the lance device */
    result = lance_probe(&net_device,&NETIF);
    if(result!=RC_OK) {
      kprintf(KR_OSTREAM,"Initialization of lance failed");
      msg.snd_code =  RC_ENET_INIT_FAILED;
    }
  }else {   
    /* If we have failed look for broadcom gigabit card */
    pciprobe_vendor_total(KR_PCI_PROBE_C,BCOM, &total);
    DEBUG_ENET  kprintf(KR_OSTREAM,"No of 3COM Devices found = %d",total);
    
    if(!total) {
      /* Get the Device ID to check if it is BCOM Altima*/
      pciprobe_vendor_next(KR_PCI_PROBE_C,BCOM,0,&net_device);
      DEBUG_ENET
	kprintf(KR_OSTREAM,"RCV:00:%02x [%04x/%04x] BR[0] %x IRQ %d,MASTER=%s",
		net_device.devfn, net_device.vendor,
		net_device.device,net_device.base_address[0],
		net_device.irq,net_device.master?"YES":"NO");
      /* Go ahead and initialise the tornado */
      result = altima_probe(&net_device,&NETIF);
      if(result!=RC_OK) {
	kprintf(KR_OSTREAM,"Initialization of altima failed");
	msg.snd_code = RC_ENET_INIT_FAILED;
      }
    }else {
      /* If we have failed, look for 3com cards */
      pciprobe_vendor_total(KR_PCI_PROBE_C,THREE_COM, &total);
      DEBUG_ENET  kprintf(KR_OSTREAM,"No of 3COM Devices found = %d",total);
      
      if(total) {
	/* Get the Device ID to check if it is 3COM 3c905c
	 * From pci_ids.h the 3com card had an ID 0x9200*/
	pciprobe_vendor_next(KR_PCI_PROBE_C,THREE_COM,0,&net_device);
	DEBUG_ENET
	  kprintf(KR_OSTREAM,
		  "RCV:00:%02x [%04x/%04x] BR[0] %x IRQ %d,MASTER=%s",
		  net_device.devfn, net_device.vendor,
		  net_device.device,net_device.base_address[0],
		  net_device.irq,net_device.master?"YES":"NO");
	/* Go ahead and initialise the tornado */
	result = _3c905c_probe(&net_device,&NETIF);
	if(result!=RC_OK) {
	  kprintf(KR_OSTREAM,"Initialization of tornado failed");
	  msg.snd_code = RC_ENET_INIT_FAILED;
	}
      }else {
	kprintf(KR_OSTREAM,"No supported network cards");
	return RC_ENET_UNSUPPORTED_HW;
      }
    }
  }
  /* Allocate the irq associated with the network device */
  result = capros_DevPrivs_allocIRQ(KR_DEVPRIVS,net_device.irq);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"IRQ %d not allocated",net_device.irq);
    msg.snd_code =  RC_ENET_IRQ_ALLOC_FAILED;
  }else   
    kprintf(KR_OSTREAM,"Allocating IRQ %d ... [SUCCESS]",net_device.irq);

  /* Start the helper process which waits on the pci irq for us and 
   * signals us of irq arrival */
  if((result=StartHelper(net_device.irq))!= RC_OK) {
    kprintf(KR_OSTREAM,"Starting Helper ... [FAILED %d]",result);
    msg.snd_code = RC_ENET_HELPER_FAILED;
  }
 
 RETURN_TO_CONSTRUCTOR:  
  /* Make a start key to pass back to constructor */
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);

  DEBUG_ENET kprintf(KR_OSTREAM, "Enet calling map_lss_three_layer "
		     "with next slot=%u...", 18);
  
  if (enet_map_lss_three_layer(KR_SELF, KR_BANK, KR_SCRATCH, 
			       KR_NEW_NODE,18) != RC_OK)
    kdprintf(KR_OSTREAM, "**ERROR: Enet call to map lss three "
    	     "failed!\n");
  
  /* Create a wrapper key for "parking" client keys that are waiting
   * for network input */
  if (forwarder_create(KR_BANK, KR_PARK_WRAP, KR_PARK_NODE, KR_START,
                       0, 0) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't create a forwarder for parking "
            "waiting-client keys...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Want it initially blocked. */
  if (capros_Forwarder_setBlocked(KR_PARK_NODE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't block forwarder for parking "
            "waiting-client keys...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Initialize the sessions data structure */
  init_active_sessions();
  
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START;
  msg.snd_rsmkey = KR_RETURN;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.invType = IT_PReturn;
  do { 
    INVOKECAP(&msg);
    msg.snd_key0 = KR_VOID;
  } while (ProcessRequest(&msg));

  return 0;
}
