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

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <string.h>
#include <stdlib.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#undef TIME_WAIT
#include <eros/Invoke.h>
#include <disk/NPODescr.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Forwarder.h>
#include <idl/capros/Process.h>
#include <idl/capros/TCPIP.h>
#include <idl/capros/TCPIPInt.h>
#include <idl/capros/NPLink.h>
#include <domain/assert.h>
#include <eros/machine/cap-instr.h>

#include <lwip/stats.h>
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <netif/etharp.h>
#include <ipv4/lwip/ip.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>
#include <lwip/init.h>

#include "cap.h"

#define dbg_tx 1
#define dbg_rx 2
#define dbg_conn 4
#define dbg_cap  8

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_tx | dbg_rx | dbg_conn | dbg_cap )

#define DEBUG(x) if (dbg_##x & dbg_flags)

// KeyInfo values for start capabilities to this process:
#define keyInfo_TCPIP 0
#define keyInfo_Timer 1
#define keyInfo_Device 2
#define keyInfo_Socket 3
#define keyInfo_ListenSocket 4

/* A TCPSocket capability is an opaque capability to a forwarder
 * whose target is a start capability to this process
 * with keyInfo_Socket.
 *
 * A TCPListenSocket capability is an opaque capability to a forwarder
 * whose target is a start capability to this process
 * with keyInfo_ListenSocket.
 *
 * A TCPIP capability is a start capability to this process
 * with keyInfo_TCPIP.
 */

/* Order codes on timer key: */
#define OC_timer_arp 0
#define OC_timer_tcp 1

/* Define slots for keys in KR_KEYSTORE: */
enum {
  timerKey = LKSN_APP,
  lastKeyStoreCap = timerKey
};

/******************************* Timers **********************************/

struct timeoutData {
  struct timer_list tl;
  uint32_t orderCode;
  unsigned long interval;
};

static void
periodicTimeoutFunction(unsigned long data)
{
  struct timeoutData * td = (struct timeoutData *)data;
  result_t result;

  result = capros_Node_getSlotExtended(KR_KEYSTORE, timerKey, KR_TEMP0);
  assert(result == RC_OK);

  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_code = td->orderCode,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
  };
  SEND(&Msg);

  // Requeue the timer:
  mod_timer_duration(&td->tl, td->interval);
}

#define periodicTimer(name, duration) \
struct timeoutData name##TimeoutData = { \
  .tl = { \
    .function = &periodicTimeoutFunction, \
    .data = (unsigned long) &name##TimeoutData \
  }, \
  .orderCode = OC_timer_##name, \
  .interval = (duration)	/* in jiffies */ \
};

periodicTimer(arp, (ARP_TMR_INTERVAL * 1000) / TICK_USEC)
periodicTimer(tcp, (TCP_TMR_INTERVAL * 1000) / TICK_USEC)

enum TCPSk_state {
  TCPSk_state_None,
  TCPSk_state_Connect,
  TCPSk_state_Listen,
  TCPSk_state_Close
};

#define maxRecvQPBufs 5

struct TCPSocket {
  struct tcp_pcb * pcb;
  int TCPSk_state;
  bool reading;
  bool writing;
  const unsigned char * writeData; // if writing == true, the next data to write
  unsigned int writeLen;   // if writing == true, the amount of data to write
  u8_t writePush;	// TCP_WRITE_FLAG_MORE or 0

  /* recvQ is a circular buffer of pbufs of data received.
   * recvQIn is where the next pbuf will be put.
   * recvQOut is where the next pbuf to be removed is.
   * recvQNum is the number of pbufs in the buffer. */
  struct pbuf * recvQ[maxRecvQPBufs];
  struct pbuf * * recvQIn;
  struct pbuf * * recvQOut;
  unsigned int recvQNum;
  unsigned int readerMaxLen;
  /* The reading process can read as little as one byte at a time,
   * so we must keep track of how much of the first pbuf chain has been read. */
  struct pbuf * curRecvPbuf;
  unsigned int curRecvBytesProcessed;	// number of bytes of curRecvPbuf
		// that have already been delivered
};

/*************************** TCP Reading ******************************/

#define sndBufWords ((capros_TCPSocket_maxReadLength \
         + sizeof(unsigned long)-1) / sizeof(unsigned long))
unsigned long sndBuf[sndBufWords];

static bool
DeliverDataNow(struct TCPSocket * sock, unsigned int maxLen)
{
  if (sock->recvQNum == 0)
    return false;	// there is no data to deliver

  if (sock->TCPSk_state == TCPSk_state_Close)
    return true;	// deliver final data

  struct pbuf * * pp = sock->recvQOut;
  unsigned long totLen = 0;
  int i;
  for (i = sock->recvQNum; i-- > 0; ) {
    struct pbuf * p = *pp;
    if (p->flags & PBUF_FLAG_PUSH)
      return true;
    totLen += p->tot_len;
    if (totLen >= maxLen)
      return true;	// we have all the data he can take

    if (++pp >= &sock->recvQ[maxRecvQPBufs])
      pp = &sock->recvQ[0];	// wrap around
  }

  /* We might also want to return true if we need to free up pbufs. */
  return false;
}

unsigned int
GatherRecvData(struct TCPSocket * sock, unsigned int maxLen)
{
  assert(sock->recvQNum);

  uint8_t * outp = (uint8_t *)&sndBuf[0];

  while (1) {
    assert(sock->recvQNum);
    struct pbuf * p = sock->curRecvPbuf;
    unsigned int remaining = p->len - sock->curRecvBytesProcessed;
		// bytes remaining to be processed in this pbuf
    unsigned int len;
    if (remaining > maxLen)	// min of remaining and maxLen
      len = maxLen;
    else
      len = remaining;

    uint8_t * payload = (uint8_t *)p->payload;
    payload += sock->curRecvBytesProcessed;
    // FIXME: eliminate this copy if it's large
    memcpy(outp, payload, len);
    outp += len;
    maxLen -= len;

    // Did we use up this pbuf?
    if (len < remaining) {	// we didn't
      sock->curRecvBytesProcessed += len;
      assert(maxLen == 0);
      break;
    } else {
      sock->curRecvBytesProcessed = 0;
      p = p->next;
      if (p) {	// there is another pbuf in this chain
		// note we might have to check tot_len instead of NULL
        sock->curRecvPbuf = p;
      } else {	// finished this chain
        pbuf_free(* sock->recvQOut);
        if (++sock->recvQOut >= &sock->recvQ[maxRecvQPBufs])
          sock->recvQOut = &sock->recvQ[0];	// wrap around
        if (--sock->recvQNum == 0)
          break;	// no more buffers
      }
    }
  }
  return outp - (uint8_t *)&sndBuf[0];	// number of bytes copied
}

// recv_tcp is called when data is received from the network.
static err_t
recv_tcp(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  struct TCPSocket * sock = arg;
  assert(sock);

  DEBUG(rx) kprintf(KR_OSTREAM, "recv_tcp err=%d\n", err);

  assert(err == ERR_OK);	// else how to handle it?

  if (!p) {	// NULL means connection is closed
printk("recv_tcp NULL pbuf\n");////
    assert(sock->TCPSk_state == TCPSk_state_None);
    sock->TCPSk_state = TCPSk_state_Close;
  } else {
    if (sock->recvQNum >= maxRecvQPBufs) {	// queue is full
      return ERR_MEM;
    }
    * sock->recvQIn = p;	// note, p is NULL if the connection was closed
    if (++sock->recvQIn >= &sock->recvQ[maxRecvQPBufs])
      sock->recvQIn = &sock->recvQ[0];	// wrap around
    if (sock->recvQNum++ == 0) {		// the first entry
      sock->curRecvPbuf = p;
      sock->curRecvBytesProcessed = 0;
    }
  }

  // Wake any reader.
  if (sock->reading) {
    if (DeliverDataNow(sock, sock->readerMaxLen)) {
      result_t result;
      unsigned int bytesRead = GatherRecvData(sock, sock->readerMaxLen);

      // Return to reader.
      const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
      result = capros_Node_getSlotExtended(KR_KEYSTORE,
                                           slot+sco_reader, KR_TEMP0);
      assert(result == RC_OK);

      Message Msg = {
        .snd_invKey = KR_TEMP0,
        .snd_w1 = bytesRead,
        .snd_w2 = 0,	// urgent flag is not implemented yet
        .snd_w3 = 0,
        .snd_key0 = KR_VOID,
        .snd_key1 = KR_VOID,
        .snd_key2 = KR_VOID,
        .snd_rsmkey = KR_VOID,
        .snd_len = bytesRead,
        .snd_data = sndBuf
      };
      Msg.snd_code = RC_OK;

      PSEND(&Msg);	// prompt send
    }
  }

  if (sock->TCPSk_state == TCPSk_state_Close
      && sock->recvQNum == 0) {	// closed and no more data to deliver
    kdprintf(KR_OSTREAM, "Close - not yet implemented");////
  }
  return ERR_OK;
}

// Read TCP data.
static void
TCPRead(Message * msg)
{
  struct TCPSocket * sock = (struct TCPSocket *)msg->rcv_w3;
	// word from forwarder

  uint32_t maxLen = msg->rcv_w1;
  if (maxLen <= 0 || maxLen > capros_TCPSocket_maxReadLength) {
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  if (sock->reading) {
    msg->snd_code = RC_capros_TCPSocket_Already;
    return;
  }

  if (DeliverDataNow(sock, maxLen)) {
    // Return data immediately.
    unsigned int bytesRead = GatherRecvData(sock, maxLen);
    msg->snd_data = sndBuf;
    msg->snd_len = bytesRead;
    msg->snd_w1 = bytesRead;
    msg->snd_w2 = 0;	// urgent flag is not implemented yet
  } else {
    result_t result;
    // Save the caller:
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
    result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_reader,
                                          KR_RETURN, KR_VOID);
    assert(result == RC_OK);
    msg->snd_invKey = KR_VOID;

    sock->readerMaxLen = maxLen;
    sock->reading = true;
  }
}

/*************************** TCP Writing ******************************/

#define rcvBufWords ((capros_TCPSocket_maxWriteLength \
         + sizeof(unsigned long)-1) / sizeof(unsigned long))
/* We have two receive buffers. One may hold data from a write operation
 * while the other is used to receive other messages.
 * This avoids copying the data being written. */
unsigned long rcvBufA[rcvBufWords];
unsigned long rcvBufB[rcvBufWords];
unsigned char * curRcvBuf = (unsigned char *)&rcvBufA;

static void
writeFinished(struct TCPSocket * sock, err_t err)
{
  result_t result;
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;

  if (err != ERR_OK) {
    kdprintf(KR_OSTREAM, "writeFinished err=%d!\n", err);
  }

  // Return to writer.
  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+sco_writer, KR_TEMP0);
  assert(result == RC_OK);

  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
  };

  Msg.snd_code = RC_OK;

  PSEND(&Msg);	// prompt send
}

static void
do_writemore(struct TCPSocket * sock)
{
  err_t err;
  unsigned int push = sock->writePush;
  unsigned int len = sock->writeLen;
  unsigned int avail = tcp_sndbuf(sock->pcb);

  DEBUG(tx) printk("do_writemore(%#x) len=%d avail=%d\n",
                   sock, len, avail);

  if (len > avail) {
    len = avail;	// send no more than we can
    push = 0;		// not the last data, don't push yet
  }

  err = tcp_write(sock->pcb, sock->writeData, len, push);
  switch (err) {
  default:
    kdprintf(KR_OSTREAM, "tcp_write err %d!\n", err);
    writeFinished(sock, err);
    break;

  case ERR_MEM:
    /* ERR_MEM is a temporary error,
     * so we wait for sent_tcp or poll_tcp to be called. */
    // Try tcp_output anyway:
    tcp_output(sock->pcb);
    break;

  case ERR_OK:
    err = tcp_output_nagle(sock->pcb);
    sock->writeData += len;
    if ((sock->writeLen -= len) == 0)
      writeFinished(sock, err);
  }
}

// Write TCP data.
static void
TCPWrite(Message * msg)
{
  result_t result;

  struct TCPSocket * sock = (struct TCPSocket *)msg->rcv_w3;
	// word from forwarder

  uint32_t totalLen = msg->rcv_w1;
  if (totalLen > capros_TCPSocket_maxWriteLength) {
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  uint8_t flags = msg->rcv_w2;
  if (flags & ~(capros_TCPSocket_flagPush | capros_TCPSocket_flagUrgent)) {
    // Invalid flag bits.
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  //// Urgent flag is not implemented yet:
  assert(!(flags & capros_TCPSocket_flagUrgent));

  if (sock->writing) {
    msg->snd_code = RC_capros_TCPSocket_Already;
    return;
  }

  sock->writeData = curRcvBuf;
  sock->writeLen = totalLen;
  bool push = flags & (capros_TCPSocket_flagPush
                       | capros_TCPSocket_flagUrgent);	// urgent implies push
  sock->writePush = push ? TCP_WRITE_FLAG_MORE : 0;
  sock->writing = true;

  // Switch receive buffers:
  curRcvBuf = (unsigned char *)
             ((mem_ptr_t)curRcvBuf ^ ((mem_ptr_t)rcvBufA ^ (mem_ptr_t)rcvBufB));

  // Save the caller:
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_writer,
                                        KR_RETURN, KR_VOID);
  assert(result == RC_OK);
  msg->snd_invKey = KR_VOID;

  do_writemore(sock);
}

/*************************** TCP Connecting ******************************/

static err_t
sent_tcp(void *arg, struct tcp_pcb *pcb, u16_t len)
{
  printk("sent_tcp\n");
////
  return ERR_OK;
}

static err_t
poll_tcp(void *arg, struct tcp_pcb *pcb)
{
////
  return ERR_OK;
}

static void
CompleteConnection(struct TCPSocket * sock, err_t err)
{
  result_t result;
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;

  sock->TCPSk_state = TCPSk_state_None;

  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
  };

  uint32_t rc;
  switch (err) {
  default:
    kdprintf(KR_OSTREAM, "CompleteConnection err %d\n", err);

  case ERR_RST:
    rc = RC_capros_TCPIP_Refused;
    break;
  case ERR_OK:
    // Return a TCPSocket capability:
    result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+sco_forwarder,
                                         KR_TEMP1);
    assert(result == RC_OK);
    result = capros_Process_makeStartKey(KR_SELF, keyInfo_Socket, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_Forwarder_swapTarget(KR_TEMP1, KR_TEMP0, KR_VOID);
    assert(result == RC_OK);
    uint32_t dummy;
    result = capros_Forwarder_swapDataWord(KR_TEMP1, (uint32_t)sock, &dummy);
    assert(result == RC_OK);
    result = capros_Forwarder_getOpaqueForwarder(KR_TEMP1,
                        capros_Forwarder_sendWord, KR_TEMP1);
    assert(result == RC_OK);

    Msg.snd_key0 = KR_TEMP1;	// override default of KR_VOID
    rc = RC_OK;
    break;
  }
  Msg.snd_code = rc;

  // Return to caller of Connect.
  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+sco_connecter,
                                       KR_TEMP0);
  assert(result == RC_OK);

  PSEND(&Msg);	// prompt send
}

static err_t
do_connected(void * arg, struct tcp_pcb *pcb, err_t err)
{
  struct TCPSocket * sock = arg;
  assert(sock);
  assert(sock->TCPSk_state == TCPSk_state_Connect);

  DEBUG(conn) printk("do_connected %d\n", err);

  CompleteConnection(sock, err);

  return ERR_OK;
}

static void
err_tcp(void * arg, err_t err)
{
  struct TCPSocket * sock = arg;

  printk("err_tcp %d sock %#x state %d\n", err, sock, sock->TCPSk_state);////

  switch (sock->TCPSk_state) {
  default: ;
    assert(false);

  case TCPSk_state_Connect:
    CompleteConnection(sock, err);
    break;
  }
}

// Create an ipv4 connection.
static void
TCPConnect(Message * msg)
{
  err_t err;
  result_t result;

  uint32_t ipAddr = msg->rcv_w1;	// host format
  unsigned int portNum = msg->rcv_w2;

  struct TCPSocket * sock = malloc(sizeof(struct TCPSocket));
  if (!sock) {
    msg->snd_code = RC_capros_TCPIP_NoMem;
    return;
  }

  // Allocate capability slots for this connection.
  // They are addressed using the sock address.
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          slot, slot+sco_numSlots-1);
  if (result != RC_OK) {
    msg->snd_code = RC_capros_TCPIP_NoMem;
    goto errExit1;
  }

  // Allocate the forwarder now. If this fails we don't want to disturb
  // the target of the connection.
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otForwarder,
		KR_TEMP0);
  if (result != RC_OK) {
    msg->snd_code = RC_capros_TCPIP_NoMem;
    goto errExit2;
  }
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_forwarder,
                                        KR_TEMP0, KR_VOID);
  assert(result == RC_OK);

  struct tcp_pcb * pcb = tcp_new();
  if (!pcb) {
    msg->snd_code = RC_capros_TCPIP_NoMem;
    goto errExit3;
  }

  sock->pcb = pcb;
  sock->TCPSk_state = TCPSk_state_None;
  sock->writing = false;
  sock->reading = false;
  sock->recvQIn = sock->recvQOut = &sock->recvQ[0];
  sock->recvQNum = 0;
  // Set callbacks:
  tcp_arg(pcb, sock);
  tcp_recv(pcb, &recv_tcp);
  tcp_sent(pcb, &sent_tcp);
  tcp_poll(pcb, &poll_tcp, 4);
  tcp_err(pcb, &err_tcp);

  // Now bind it to a local port.
  err = tcp_bind(pcb, IP_ADDR_ANY/*???*/, 0 /* find a free port */);
  if (err != ERR_OK) {
    msg->snd_code = RC_capros_TCPIP_NoMem;	// bogus
    goto errExit;
  }

  // Connect.
  sock->TCPSk_state = TCPSk_state_Connect;
  struct ip_addr ipa = {
    .addr = htonl(ipAddr)
  };
  err = tcp_connect(pcb, &ipa, portNum, &do_connected);
  if (err != ERR_OK) {
    msg->snd_code = RC_capros_TCPIP_NoMem;	// bogus
    //// clean up
    goto errExit;
  }

  // Caller will wait until the connection is complete.
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_connecter,
                                        KR_RETURN, KR_VOID);
  assert(result == RC_OK);
  msg->snd_invKey = KR_VOID;
  return;

errExit:
  tcp_abort(pcb);
errExit3:
  result = capros_SpaceBank_free1(KR_BANK, KR_TEMP0);
errExit2:
  result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
                                            slot, slot+sco_numSlots-1);
errExit1:
  free(sock);
  return;
}

/*************************** main ******************************/

NORETURN void
driver_main(void)
{
  result_t result;
  Message Msg;
  Message * const msg = &Msg;

  // Ensure the receive buffers are allocated by VCSK:
  rcvBufA[0] = 0;
  rcvBufA[rcvBufWords-1] = 0;
  rcvBufB[0] = 0;
  rcvBufB[rcvBufWords-1] = 0;

  struct ip_addr ipaddr, ipmask, ipgw;
  uint32_t ipInt, msInt, gwInt;
  result = capros_Number_get(KR_IPAddrs, &ipInt, &msInt, &gwInt);
  assert(result == RC_OK);
  ipaddr.addr = htonl(ipInt);
  ipmask.addr = htonl(msInt);
  ipgw.addr   = htonl(gwInt);
  // Allocate slots in keystore:
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          LKSN_APP, lastKeyStoreCap);
  assert(result == RC_OK);

  // Make start cap for timers.
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_Timer, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, timerKey,
                                        KR_TEMP1, KR_VOID);
  assert(result == RC_OK);

  // Make start cap for interrupt thread.
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_Device, KR_DeviceEntry);
  assert(result == RC_OK);

  // Give our cap to nplink.
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_TCPIP, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_VOLSIZE, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_getSlot(KR_TEMP0, volsize_pvolsize, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_getSlot(KR_TEMP0, volsize_nplinkCap, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_NPLink_RegisterNPCap(KR_TEMP0, KR_TEMP1,
             IKT_capros_TCPIP, 0);
  assert(result == RC_OK);

  printk("EP93xx Ethernet driver started.\n");

  // Initialize the TCP/IP code:
  lwip_init();

  // Start the timers:
  mod_timer_duration(&arpTimeoutData.tl, (ARP_TMR_INTERVAL * 1000) / TICK_USEC);
  mod_timer_duration(&tcpTimeoutData.tl, (TCP_TMR_INTERVAL * 1000) / TICK_USEC);

  struct netif theNetIf;
  err_t devInitF(struct netif * netif);
  netif_add(&theNetIf, &ipaddr, &ipmask, &ipgw,
            NULL, &devInitF, &ethernet_input);
  netif_set_default(&theNetIf);
  netif_set_up(&theNetIf);

  msg->snd_invKey = KR_VOID;
  msg->snd_key0 = msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  /* The void key is not picky about the other parameters,
  so it's OK to leave them uninitialized. */

  for (;;) {
    msg->rcv_key0 = msg->rcv_key1 = msg->rcv_key2 = KR_VOID;
    msg->rcv_rsmkey = KR_RETURN;
    msg->rcv_limit = sizeof(rcvBufA);	// both are the same size
    msg->rcv_data = curRcvBuf;

    RETURN(&Msg);

    // Set up defaults for return:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;
    Msg.snd_key0 = KR_VOID;
    Msg.snd_key1 = KR_VOID;
    Msg.snd_key2 = KR_VOID;
    Msg.snd_rsmkey = KR_VOID;
    Msg.snd_len = 0;

    switch (Msg.rcv_keyInfo) {
    default:
      assert(false);

    case keyInfo_TCPIP:
      DEBUG(cap) kprintf(KR_OSTREAM, "Called TCPIP, oc=%#x\n", Msg.rcv_code);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_TCPIP;
        break;
  
      case OC_capros_TCPIP_connect:
        TCPConnect(&Msg);
        break;
  
      case OC_capros_TCPIP_listen:
        ////
        break;
      }
      break;

    case keyInfo_ListenSocket:
      DEBUG(cap) kprintf(KR_OSTREAM, "Called LsnSk, oc=%#x\n", Msg.rcv_code);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_TCPListenSocket;
        break;
  
      case OC_capros_TCPListenSocket_accept:
        ////
        break;
      }
      break;

    case keyInfo_Socket:
      DEBUG(cap) kprintf(KR_OSTREAM, "Called Sock, oc=%#x\n", Msg.rcv_code);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_TCPSocket;
        break;
  
      case OC_capros_TCPSocket_close:
        ////
        break;
  
      case OC_capros_TCPSocket_abort:
        ////
        break;
  
      case 0:	// OC_capros_TCPSocket_read
        TCPRead(&Msg);
        break;
  
      case 1:	// OC_capros_TCPSocket_write
        TCPWrite(&Msg);
        break;
      }
      break;

    case keyInfo_Timer:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_timer_arp:
        DEBUG(cap) kprintf(KR_OSTREAM, "TArp ");

        etharp_tmr();
        break;

      case OC_timer_tcp:
        DEBUG(cap) kprintf(KR_OSTREAM, "TTmr ");

        tcp_tmr();
        break;
      }
      break;

    case keyInfo_Device:
      DEBUG(cap) kprintf(KR_OSTREAM, "Called Dev, oc=%#x\n", Msg.rcv_code);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_TCPIPInt;
        break;
  
      case OC_capros_TCPIPInt_processInterrupt: ;
        // Call the specified function in this thread:
        void (*fcn)(uint32_t status) = (void (*)(uint32_t)) Msg.rcv_w1;
        (*fcn)(Msg.rcv_w2);
        break;
      }
      break;
    }	// end of switch (keyInfo)
  }	// end of loop forever
}
