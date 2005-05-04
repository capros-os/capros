/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* A simple no frills http server. It binds a tcp socket and listens 
 * on it for connections. It uses shared memory to communicate between
 * the app and the netsys */
#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/ProcessKey.h>
#include <eros/endian.h>
#include <eros/KeyConst.h>

#include <idl/eros/Sleep.h>

#include <domain/SpaceBankKey.h>
#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <idl/eros/domain/net/shared_ipv4/netsys.h>
#include <addrspace/addrspace.h>

#include <ctype.h>
#include <string.h>

#include "mapping_table.h"
#include "map.h"
#include "webs_keys.h"

//#include "webpages/google.h"
#include "constituents.h"

/* The largest size any domain currently has for sharing or otherwise
 * mapping memory is 1GB.  This domain maps an array of 32-bit values
 * into that region and the offset into this region starts at zero. */
#define _1GB_ 0x40000000u
#define MAX_BUFFER_OFFSET ((_1GB_ / sizeof(uint32_t)) - 1)

/* Globals */
struct mapping_table mt[4]; /* mapping table (only indices 0,2 are used) */   


/* After we have prep'd the addrspace for mapping. We still need to 
 * fill up the slots in 16 - 23 with lss 3 nodes. So that we can use 
 * lss 3 subspaces while mapping clients */
static uint32_t
enet_map_lss_three_layer(cap_t kr_self, cap_t kr_bank,
			 cap_t kr_scratch, cap_t kr_node, uint32_t next_slot)
{
  uint32_t slot;
  uint32_t lss_three = 3;
  uint32_t result;

  process_copy(kr_self, ProcAddrSpace, kr_scratch);
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
  uint32_t ipaddr,netmask,gateway;
  uint32_t dummy=0;   /* We use a wrapper node, so w1 is bashed by kernel */
  short port = 80;    /* The default port for webs */
  uint32_t xmit_buffer_addr, recv_buffer_addr;
  Message msg;
  
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_NETSYS_C,KR_NETSYS_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP,KR_SLEEP);

  /* Prepare our default address space for mapping in additional
   * memory. */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, KR_NEW_SUBSPACE);
  
  if (enet_map_lss_three_layer(KR_SELF, KR_BANK, KR_SCRATCH, 
			       KR_NEW_SUBSPACE,16) != RC_OK)
    kdprintf(KR_OSTREAM, "**ERROR: Webs call to map lss three "
    	     "failed!\n");
  
  /* Create a subbank to use for window creation */
  if (spcbank_create_subbank(KR_BANK, KR_SUB_BANK) != RC_OK) {
    kprintf(KR_OSTREAM, "shared_ttcp failed to create sub bank.\n");
    return -1;
  }

  /* Construct the netsys domain */
  result = constructor_request(KR_NETSYS_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_NETSYS_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "nettest:Constructing NETSYS...[FAILED]\n");
  }

  /* Sleep for sometime to allow the netsys to configure itself
   * using DHCP */
  eros_Sleep_sleep(KR_SLEEP,2000);

  /* Check if we have been configured */
  result = eros_domain_net_shared_ipv4_netsys_get_netconfig(KR_NETSYS_S,
							    &ipaddr,
							    &netmask,
							    &gateway);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Network configuration not done");
    return 0;
  }

  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_NETSYS_S;
  msg.rcv_key0 = KR_NETSYS;
  msg.snd_code = OC_NetSys_GetSessionCreatorKey;
  CALL(&msg); /* Receive a session creator key */
  
  /* We now have netsys session creator key. 
   * So go ahead and create a session*/
  msg.snd_invKey = KR_NETSYS;
  msg.snd_key0 = KR_BANK;
  msg.rcv_key0 = KR_NETSYS;
  msg.rcv_key1 = KR_XMIT_BUF;
  msg.rcv_key2 = KR_RECV_BUF;
  msg.snd_code = OC_NetSys_GetNewSessionKey;
  CALL(&msg);

  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"shared_shared_ipv4 test could not get Session key");
    return -1;
  }
  
  /* Map the new addr space into webs' space */
  result = map_single_buff(KR_BANK,KR_XMIT_BUF,&xmit_buffer_addr);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Webs: unable to map XMIT_BUF",result);
    return 1; 
  }
  
  result = map_single_buff(KR_BANK,KR_RECV_BUF,&recv_buffer_addr);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"Webs: unable to map RECV_BUF",result);
    return 1; 
  }
  
  /* Construct the mapping table */
  mt[XMIT_CLIENT_SPACE].start_address = xmit_buffer_addr;
  mt[XMIT_CLIENT_SPACE].sector = XMIT_CLIENT_SPACE;
  mt[XMIT_CLIENT_SPACE].cur_p = 0;
  
  mt[RECV_CLIENT_SPACE].start_address = recv_buffer_addr;
  mt[RECV_CLIENT_SPACE].sector = RECV_CLIENT_SPACE;
  mt[RECV_CLIENT_SPACE].cur_p = 0;

  msg.rcv_key0 = KR_VOID;
  
  /* Now do the equivalent of opening a stream socket and binding 
   * it to the port 80 */
  result = eros_domain_net_shared_ipv4_netsys_tcp_bind(KR_NETSYS,dummy,0,port);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Binding %d TCP port ... [FAILED]",port);
    return 1;
  }
  
  /* We are the server and should listen for the connections */
  result = eros_domain_net_shared_ipv4_netsys_tcp_listen(KR_NETSYS);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Listening on 5001 TCP  ... [FAILED]");
    return 1;
  }
  
  
  return 0;
}
