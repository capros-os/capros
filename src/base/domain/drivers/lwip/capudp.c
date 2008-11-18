/*
 * Copyright (C) 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linuxk/lsync.h>
#include <eros/Invoke.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/Process.h>
#include <idl/capros/IP.h>
#include <idl/capros/UDPPort.h>
#include <domain/assert.h>

#include <lwip/pbuf.h>
#include <lwip/udp.h>

#include "cap.h"

#define dbg_tx 1
#define dbg_rx 2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define maxRecvQPBufs 5

struct recvQItem {
  struct pbuf * pbuf;
  uint32_t ipAddr;	// in host format
  u16_t port;
};

struct UDPPort {
  struct udp_pcb * pcb;
  bool receiving;

  struct pbuf sendPbuf;

  /* recvQ is a circular buffer of datagrams received.
   * recvQIn is where the next item will be put.
   * recvQOut is where the next item to be removed is.
   * recvQNum is the number of items in the buffer. */
  struct recvQItem recvQ[maxRecvQPBufs];
  struct recvQItem * recvQIn;
  struct recvQItem * recvQOut;
  unsigned int recvQNum;
};

static inline void
ReturnToReceiver(cap_t receiver,
  void * data, unsigned int bytesReceived,
  uint32_t rc, uint32_t ipAddr, unsigned int port)
{
  Message Msg = {
    .snd_invKey = receiver,
    .snd_code = rc,
    .snd_w1 = ipAddr,
    .snd_w2 = port,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = bytesReceived,
    .snd_data = data
  };
  PSEND(&Msg);	// prompt send
}

static inline void
rqi_ReturnToReceiver(struct recvQItem * rqi, cap_t receiver)
{
  struct pbuf * p = rqi->pbuf;
  ReturnToReceiver(receiver, p->payload, p->len,
                   RC_OK, rqi->ipAddr, rqi->port);
  pbuf_free(p);
}

// recv_udp is called when a datagram is received from the network.
void
recv_udp(void * arg, struct udp_pcb * pcb,
  struct pbuf * p, struct ip_addr * ipaddr, u16_t port)
{
  struct UDPPort * sock = arg;
  assert(sock);
  assert(p->tot_len == p->len);
	// must be all in one buffer, else need code to copy data

  DEBUG(rx) kprintf(KR_OSTREAM, "recv_udp\n");

  if (sock->recvQNum >= maxRecvQPBufs) {	// queue is full
kdprintf(KR_OSTREAM, "recv_udp: queue full, discarding datagram!\n");////
    return;
  }
  // Save the datagram in the queue:
  struct recvQItem * rqi = sock->recvQIn;
  rqi->pbuf = p;
  rqi->ipAddr = ntohl(ipaddr->addr);
  rqi->port = port;

  // Wake any receiver.
  if (sock->receiving) {
    result_t result;

    // Can only be receiving if the buffer was empty:
    assert(sock->recvQOut == rqi);

    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
    result = capros_Node_getSlotExtended(KR_KEYSTORE,
                                         slot+udp_receiver, KR_TEMP0);
    assert(result == RC_OK);
    sock->receiving = false;

    rqi_ReturnToReceiver(rqi, KR_TEMP0);
    // Don't bother to increment recvQIn and recvQOut.
  } else {
    if (++sock->recvQIn >= &sock->recvQ[maxRecvQPBufs])
      sock->recvQIn = &sock->recvQ[0];	// wrap around
    sock->recvQNum++;
  }
}

void
UDPGetMaxSizes(Message * msg)
{
  uint32_t mms, mtu;
  struct ip_addr ipaddr = {
    .addr = htonl(msg->rcv_w1)
  };
  struct netif * netif = ip_route(&ipaddr);
  if (netif)
    mtu = netif->mtu;	// 0 if not specified
  else
    mtu = 0;
  if (mtu) {
    assert(mtu > IP_HLEN + UDP_HLEN);
    mms = LWIP_MIN(mtu - (IP_HLEN + UDP_HLEN), MMS_LIMIT);
  } else
    mms = MMS_LIMIT;
  msg->snd_w1 = msg->snd_w2 = mms;
}

void
UDPCreate(Message * msg)
{
  result_t result;
  err_t err;

  unsigned int localPort = msg->rcv_w1;

  struct UDPPort * sock = malloc(sizeof(struct UDPPort));
  if (!sock) {
    msg->snd_code = RC_capros_IP_NoMem;
    goto errExit0;
  }

  struct udp_pcb * pcb = udp_new();
  if (!pcb) {
    msg->snd_code = RC_capros_IP_NoMem;
    goto errExit1;
  }

  if (localPort != capros_IP_LocalPortAny) {	// he specified a local port
    err = udp_bind(pcb, IP_ADDR_ANY, localPort);
    if (err != ERR_OK) {
      assert(err == ERR_USE);
      msg->snd_code = RC_capros_IP_Already;
      goto errExit2;
    }
  }

  // Allocate capability slots for this connection.
  // They are addressed using the sock address.
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          slot, slot+udp_numSlots-1);
  if (result != RC_OK) {
    goto errExit3;
  }

  // Allocate the forwarder now.
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otForwarder,
		KR_TEMP1);
  if (result != RC_OK) {
    goto errExit4;
  }
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+udp_forwarder,
                                        KR_TEMP1, KR_VOID);
  assert(result == RC_OK);

  udp_recv(pcb, &recv_udp, sock);	// specify callback
  sock->pcb = pcb;
  sock->receiving = false;
  sock->recvQIn = sock->recvQOut = &sock->recvQ[0];
  sock->recvQNum = 0;
  // Initialize constant fields in the pbuf:
  sock->sendPbuf.type = PBUF_REF;
  sock->sendPbuf.next = NULL;
  sock->sendPbuf.ref = 1;
  sock->sendPbuf.flags = 0;

  // Create the UDPPort capability to return.
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_UDPPort, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Forwarder_swapTarget(KR_TEMP1, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  uint32_t dummy;
  result = capros_Forwarder_swapDataWord(KR_TEMP1, (uint32_t)sock, &dummy);
  assert(result == RC_OK);
  result = capros_Forwarder_getOpaqueForwarder(KR_TEMP1,
                      capros_Forwarder_sendWord, KR_TEMP1);
  assert(result == RC_OK);

  msg->snd_key0 = KR_TEMP1;
  return;

errExit4:
  result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
                                            slot, slot+udp_numSlots-1);
errExit3:
errExit2:
  udp_remove(pcb);
errExit1:
  free(sock);
errExit0:
  return;
}

void
UDPDestroy(Message * msg)
{
  result_t result;
  struct UDPPort * sock = (struct UDPPort *)msg->rcv_w3;
	// word from forwarder

  if (sock->receiving) {
    // Return to the receiver as though the capability is already gone:
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
    result = capros_Node_getSlotExtended(KR_KEYSTORE,
                                         slot+udp_receiver, KR_TEMP0);
    assert(result == RC_OK);
    sock->receiving = false;

    ReturnToReceiver(KR_TEMP0, NULL, 0, RC_capros_key_Void, 0, 0);
  }

  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+udp_forwarder,
                                       KR_TEMP0);
  assert(result == RC_OK);
  result = capros_SpaceBank_free1(KR_BANK, KR_TEMP0);
  // Freeing the forwarder invalidates all the capabilities to it.

  result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
                                            slot, slot+udp_numSlots-1);
  udp_remove(sock->pcb);
  free(sock);
}

void
UDPReceive(Message * msg)
{
  result_t result;
  struct UDPPort * sock = (struct UDPPort *)msg->rcv_w3;
	// word from forwarder

  if (sock->receiving) {
    msg->snd_code = RC_capros_TCPSocket_Already;
    return;
  }

  if (sock->recvQNum == 0) {	// there is no data now
    DEBUG(rx) kprintf(KR_OSTREAM, "UDPReceive: waiting\n");
    // Save the caller:
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
    result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+udp_receiver,
                                          KR_RETURN, KR_VOID);
    assert(result == RC_OK);

    sock->receiving = true;
  } else {			// deliver data
    DEBUG(rx) kprintf(KR_OSTREAM, "UDPReceive: got data\n");
    /* Rather than send the message to the caller by RETURNing to KR_RETURN,
    we will PSEND to KR_RETURN and then RETURN to KR_VOID.
    This allows us to free the pbuf after it is used. */
    rqi_ReturnToReceiver(sock->recvQOut, KR_RETURN);

    if (++sock->recvQOut >= &sock->recvQ[maxRecvQPBufs])
      sock->recvQOut = &sock->recvQ[0];	// wrap around
    --sock->recvQNum;
  }
  msg->snd_invKey = KR_VOID;
}

void
UDPSend(Message * msg)
{
  err_t err;
  struct UDPPort * sock = (struct UDPPort *)msg->rcv_w3;
	// word from forwarder

  uint32_t ipAddr = msg->rcv_w1;	// host format
  unsigned int portNum = msg->rcv_w2;
  uint32_t len = LWIP_MIN(msg->rcv_limit, msg->rcv_sent);

  struct ip_addr ipa = {
    .addr = htonl(ipAddr)
  };

  err = udp_connect(sock->pcb, &ipa, portNum);
  switch (err) {
  default:
    assert(false);

  case ERR_USE:
    msg->snd_code = RC_capros_UDPPort_NoPort;
    return;

  case ERR_RTE:
    msg->snd_code = RC_capros_UDPPort_NoRoute;
    return;

  case ERR_OK:
    break;
  }

  struct pbuf * p = &sock->sendPbuf;
  p->payload = msg->rcv_data;
  p->len = p->tot_len = len;

  err = udp_send(sock->pcb, p);
  assert(err == ERR_OK);	//// FIXME need to handle
}
