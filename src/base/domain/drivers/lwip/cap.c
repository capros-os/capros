/*
 * Copyright (C) 2008-2010, Strawberry Development Group.
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
#include <idl/capros/NPIP.h>
#include <idl/capros/UDPPort.h>
#include <idl/capros/IPInt.h>
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

#include <lwipCap.h>
#include "cap.h"

#define dbg_tx     0x01
#define dbg_rx     0x02
#define dbg_conn   0x04
#define dbg_cap    0x08
#define dbg_listen 0x10
#define dbg_errors 0x20
#define dbg_alloc  0x40
#define dbg_mem    0x80

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors | dbg_mem)

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* A TCPSocket capability is an opaque capability to a forwarder
 * whose target is a start capability to this process
 * with keyInfo_Socket.
 *
 * Likewise for TCPListenSocket and UDPPort.
 *
 * An IP capability is a start capability to this process
 * with keyInfo_IP.
 */

/* Order codes on timer key: */
#define OC_timer_arp 0
#define OC_timer_tcp 1

/* Define slots for keys in KR_KEYSTORE: */
enum {
  timerKey = LKSN_APP,
  lastKeyStoreCap = timerKey
};

struct netif theNetIf;

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
  TCPSk_state_Closing,	// we will send no more data,
			// and have not successfully called tcp_close
  TCPSk_state_Closed,	// we will send no more data,
			// and have successfully called tcp_close
  TCPSk_state_End
};

// Bits in TCPSk_remoteState:
#define TCPSk_rstate_Closed 0x1	// other end will send no more data.
#define TCPSk_rstate_Gone   0x2	// other end will send no more data,
				// and the pcb is freed.

#define maxRecvQPBufs 64

struct TCPSocket {
#ifdef MALLOC_DEBUG
  void * poison0;
  void * poison1;
#endif
  struct tcp_pcb * pcb;
  int TCPSk_state;
  bool receiving;   // true if there is a process receiving (in sco_receiver)
  bool sending;
  bool remoteCloseDelivered;
  int TCPSk_remoteState;

  /* Each TCPSocket has a send buffer allocated. sendBuf points to it.
  If sending, sendBuf has the data.
  If not sending, sendBuf is available to replace curRcvBuf. */
  unsigned char * sendBuf;
  const unsigned char * sendData; // if sending == true, the next data to send
  unsigned int sendLen;   // if sending == true, the amount of data to send
  u8_t sendPush;	// TCP_WRITE_FLAG_MORE or 0 to push

  /* recvQ is a circular buffer of pbufs of data received.
   * recvQIn is where the next pbuf will be put.
   * recvQOut is where the next pbuf to be removed is.
   * recvQNum is the number of pbufs in the buffer. */
  struct pbuf * * recvQIn;
  struct pbuf * * recvQOut;
  unsigned int recvQNum;
  unsigned int receiverMaxLen;
  /* The receiving process can read as little as one byte at a time,
   * so we must keep track of how much of the first pbuf chain has been read.
   * curRecvPbuf points to the same pbuf as recvQOut, or to a pbuf
   * later in the same chain. */
  struct pbuf * curRecvPbuf;
  unsigned int curRecvBytesProcessed;	// number of bytes of curRecvPbuf
		// that have already been delivered
  struct pbuf * recvQ[maxRecvQPBufs];
};

#define maxAcceptQEntries 5

struct TCPListenSocket {
  struct tcp_pcb * pcb;
  bool listening;

  /* acceptQ is a circular buffer of entries of connections received.
   * acceptQIn is where the next entry will be put.
   * acceptQOut is where the next entry to be removed is.
   * acceptQNum is the number of entries in the buffer. */
  struct TCPSocket * acceptQ[maxAcceptQEntries];
  struct TCPSocket * * acceptQIn;
  struct TCPSocket * * acceptQOut;
  unsigned int acceptQNum;
};

static inline bool
IsValidPointer(void * p)
{
  return (size_t)p <= (((size_t)1) << CAPROS_FAST_SPACE_LGSIZE); // kludge
}

void
ValidatePCB(struct tcp_pcb * pcb)
{
  assert(pcb->state <= TIME_WAIT);
  assert(pcb->tos == 0);
  assert(pcb->ttl == TCP_TTL);
  assert(IsValidPointer(pcb->callback_arg));
}

static void
ValidateSock(struct TCPSocket * sock)
{
#ifdef MALLOC_DEBUG
  assert(sock->poison0 == POISON1);
  assert(sock->poison1 == POISON1);
#endif
  assert(IsValidPointer(sock->sendBuf));
  assert(sock->TCPSk_state < TCPSk_state_End);
  assert((sock->TCPSk_remoteState & ~(TCPSk_rstate_Closed | TCPSk_rstate_Gone))
         == 0);
  if (sock->sending) {
    assert((sock->sendPush & ~TCP_WRITE_FLAG_MORE) == 0);
  }
  assert(sock->pcb->callback_arg == sock);
  ValidatePCB(sock->pcb);
}

static void
ValidateListenSocket(struct TCPListenSocket * sock)
{
  assert((sock->listening & ~1) == 0);
  assert(sock->pcb->callback_arg == sock);
  ValidatePCB(sock->pcb);
}

static void DestroyTCPConnection(struct TCPSocket * sock);

/*************************** TCP Receiving ******************************/

#define sndBufWords ((capros_TCPSocket_maxReceiveLength \
         + sizeof(unsigned long)-1) / sizeof(unsigned long))
unsigned long sndBuf[sndBufWords];

// Should we deliver received data to the client now?
static bool
DeliverDataNow(struct TCPSocket * sock, unsigned int maxLen)
{
  if (sock->TCPSk_remoteState)
    return true;	// deliver the final RemoteClosed

  if (sock->recvQNum == 0)
    return false;	// there is no data to deliver

  if (sock->TCPSk_state == TCPSk_state_Closing
      || sock->TCPSk_state == TCPSk_state_Closed)
    return true;	// deliver final data

  // We must deliver the data if the receiver can receive no more.
  // Also, we don't want to sit on more than about TCP_WND/2 bytes,
  // lest transmission be slowed.
  unsigned int deliveryLimit = maxLen < TCP_WND/2 ? maxLen : TCP_WND/2;

  struct pbuf * * pp = sock->recvQOut;
  unsigned long totLen = 0;

  int i;
  for (i = sock->recvQNum; i-- > 0; ) {
    struct pbuf * p = *pp;
    if (p->flags & PBUF_FLAG_PUSH)
      return true;
    totLen += p->tot_len;
    if (totLen >= deliveryLimit)
      return true;	// we have all the data he can take

    if (++pp >= &sock->recvQ[maxRecvQPBufs])
      pp = &sock->recvQ[0];	// wrap around
  }

  /* We might also want to return true if we need to free up pbufs. */
  return false;
}

static void
ConsumePbuf(struct TCPSocket * sock)
{
  assert(sock->recvQNum);
  --sock->recvQNum;
  pbuf_free(* sock->recvQOut);
#ifndef NDEBUG
  * sock->recvQOut = NULL;	// for safety
  sock->curRecvPbuf = NULL;
#endif
  if (++sock->recvQOut >= &sock->recvQ[maxRecvQPBufs])
    sock->recvQOut = &sock->recvQ[0];	// wrap around
}

uint8_t recvFlags;
/* Sets recvFlags and returns the number of bytes to send to client. */
static unsigned int
GatherRecvData(struct TCPSocket * sock, unsigned int maxLen)
{
  recvFlags = 0;
  if (! sock->recvQNum)
    return 0;		// No data. Delivering final RemoteClosed.

  uint8_t * outp = (uint8_t *)&sndBuf[0];

  while (1) {
    assert(sock->recvQNum);
    struct pbuf * p = sock->curRecvPbuf;
    if (p->flags & PBUF_FLAG_PUSH)
      recvFlags |= capros_TCPSocket_flagPush;
    // FIXME deal with Urgent flag, when lwip implements it
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
        ConsumePbuf(sock);
        if (sock->recvQNum == 0)
          break;	// no more buffers
        sock->curRecvPbuf = * sock->recvQOut;
      }
    }
  }
  unsigned int totalLen = outp - (uint8_t *)&sndBuf[0];	// num of bytes copied
  assert(totalLen);
  tcp_recved(sock->pcb, totalLen);	// let the sender send more
  return totalLen;
}

static void
ReturnToReceiver(struct TCPSocket * sock, unsigned int bytesReceived,
  uint32_t rc)
{
  result_t result;

  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_Node_getSlotExtended(KR_KEYSTORE,
                                       slot+sco_receiver, KR_TEMP0);
  assert(result == RC_OK);

  unsigned int flags = bytesReceived ? recvFlags : 0;
  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_code = rc,
    .snd_w1 = bytesReceived,
    .snd_w2 = flags,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = bytesReceived,
    .snd_data = sndBuf
  };
  PSEND(&Msg);	// prompt send
  sock->receiving = false;
  DEBUG(rx) kprintf(KR_OSTREAM, "lwip woke rcvr, rc=%#x, flgs=%#x\n",
                    rc, flags);
}

// Call when the other end will send no more.
static void
RemoteClosed(struct TCPSocket * sock)
{
  assert(sock->TCPSk_remoteState);
  if (sock->receiving) {
    assert(sock->recvQNum == 0);
    ReturnToReceiver(sock, 0, RC_capros_TCPSocket_RemoteClosed);
    sock->remoteCloseDelivered = true;
  }
}

void
WakeAnyReceiver(struct TCPSocket * sock)
{
  if (sock->receiving) {
    if (DeliverDataNow(sock, sock->receiverMaxLen)) {
      unsigned int bytesReceived = GatherRecvData(sock, sock->receiverMaxLen);
      if (bytesReceived)
        ReturnToReceiver(sock, bytesReceived, RC_OK);
      else
        RemoteClosed(sock);
    }
  }
}

static void
CheckFullyClosed(struct TCPSocket * sock)
{
  if (sock->TCPSk_state == TCPSk_state_Closed	// we will send no more
      && sock->remoteCloseDelivered) {	// we know they will send no more
    // No more data will flow over this connection. Destroy it.
    assert(! sock->sending);	// because of sock->TCPSk_state
    assert(! sock->receiving);	// because of sock->remoteCloseDelivered
    assert(sock->TCPSk_remoteState == TCPSk_rstate_Closed);
    /* Because we have successfully called tcp_close
       (which we know because sock->TCPSk_state == TCPSk_state_Closed)
       and the other end has closed
       (which we know because sock->remoteCloseDelivered),
       we know the lwip core will eventually free the pcb. */
    /* To ensure we only call DestroyTCPConnection once, we must
       ignore any calls to err_tcp etc.: */
    tcp_err(sock->pcb, NULL);
    tcp_sent(sock->pcb, NULL);
    tcp_poll(sock->pcb, NULL, 4);
#ifdef MALLOC_DEBUG
    // We don't expect any calls to recv_tcp, but this will catch:
    tcp_arg(sock->pcb, POISON4);
    sock->pcb = POISON5;
#endif
    DestroyTCPConnection(sock);
  }
}

// recv_tcp is called when data is received from the network.
// We are responsible for freeing the pbuf if we return ERR_OK.
static err_t
recv_tcp(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
  struct TCPSocket * sock = arg;
  ValidateSock(sock);

  DEBUG(rx) kprintf(KR_OSTREAM, "recv_tcp err=%d\n", err);

  assert(err == ERR_OK);	// else how to handle it?

  if (!p) {	// NULL means other end will send no more data
    DEBUG(conn) kprintf(KR_OSTREAM, "%s:%d: got NULL pbuf, rcvg=%d\n",
                        __FILE__, __LINE__, sock->receiving);
    assert(! sock->TCPSk_remoteState);
    sock->TCPSk_remoteState |= TCPSk_rstate_Closed;
    RemoteClosed(sock);
    CheckFullyClosed(sock);
  } else {
    if (sock->recvQNum >= maxRecvQPBufs) {	// queue is full
      DEBUG(errors) kdprintf(KR_OSTREAM, "Sock %#x recvQ full!\n", sock);
      return ERR_MEM;
    }
    * sock->recvQIn = p;	// note, p is NULL if the connection was closed
    if (++sock->recvQIn >= &sock->recvQ[maxRecvQPBufs])
      sock->recvQIn = &sock->recvQ[0];	// wrap around
    if (sock->recvQNum++ == 0) {		// the first entry
      sock->curRecvPbuf = p;
      sock->curRecvBytesProcessed = 0;
    }
    WakeAnyReceiver(sock);
  }
  return ERR_OK;
}

// Receive TCP data.
static void
TCPReceive(Message * msg)
{
  struct TCPSocket * sock = (struct TCPSocket *)msg->rcv_w3;
	// word from forwarder
  ValidateSock(sock);

  uint32_t maxLen = msg->rcv_w1;
  if (maxLen <= 0 || maxLen > capros_TCPSocket_maxReceiveLength) {
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  if (sock->receiving) {
    msg->snd_code = RC_capros_TCPSocket_Already;
    return;
  }

  if (DeliverDataNow(sock, maxLen)) {
    // Return data immediately.
    unsigned int bytesReceived = GatherRecvData(sock, maxLen);
    if (! bytesReceived) {
      msg->snd_code = RC_capros_TCPSocket_RemoteClosed;
      sock->remoteCloseDelivered = true;
    }
    msg->snd_data = sndBuf;
    msg->snd_len = bytesReceived;
    msg->snd_w1 = bytesReceived;
    msg->snd_w2 = recvFlags;
  } else {
    result_t result;
    // Save the caller:
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
    result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_receiver,
                                          KR_RETURN, KR_VOID);
    assert(result == RC_OK);
    msg->snd_invKey = KR_VOID;

    sock->receiverMaxLen = maxLen;
    sock->receiving = true;
  }
}

/*************************** TCP Writing ******************************/

/* The receive buffer size must be big enough for both a UDP message and
 * a TCP buffer. */
#define rcvBufSize (capros_TCPSocket_maxSendLength > MMS_LIMIT ? \
                    capros_TCPSocket_maxSendLength : MMS_LIMIT)
unsigned char * curRcvBuf;

static unsigned char *
AllocRcvBuf(void)
{
  unsigned char * buf = mem_malloc(rcvBufSize);
  if (buf) {
    // Ensure the receive buffer is allocated by VCSK:
    assert(rcvBufSize <= EROS_PAGE_SIZE);	// else need a loop below
    buf[0] = 0;
    buf[rcvBufSize-1] = 0;
  }
  return buf;
}

// Also used for close finished.
static void
sendFinished(struct TCPSocket * sock, err_t err)
{
  result_t result;
  uint32_t rc;
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;

  // Translate err to a CapROS return code.
  switch (err) {
  default:
    kdprintf(KR_OSTREAM, "sendFinished err=%d!\n", err);
  case ERR_OK:
    rc = RC_OK;
    break;
  case ERR_CLSD:
    rc = RC_capros_key_Void;
    break;
  }

  // Return to sender.
  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+sco_sender, KR_TEMP0);
  assert(result == RC_OK);

  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_code = rc,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
  };
  PSEND(&Msg);	// prompt send
  sock->sending = false;
}

static void
do_sendmore(struct TCPSocket * sock)
{
  err_t err;
  unsigned int len = sock->sendLen;	// could be zero
  unsigned int avail = tcp_sndbuf(sock->pcb);	// could be zero

  DEBUG(tx) printk("do_sendmore(%#x) len=%d avail=%d\n",
                   sock, len, avail);

  unsigned int push;
  if (len > avail) {
    len = avail;	// send no more than we can
    push = TCP_WRITE_FLAG_MORE;	// not the last data, don't push yet
  } else {
    push = sock->sendPush;
  }

  /* Unfortunately, there is no easy way to find out when sent data has been
     acknowledged, and its space can be reused. Buffering would be complex.
     Thus we copy the data by specifying TCP_WRITE_FLAG_COPY. */
  err = tcp_write(sock->pcb, sock->sendData, len, push | TCP_WRITE_FLAG_COPY);
  switch (err) {
  default:
    kdprintf(KR_OSTREAM, "tcp_write err %d!\n", err);
    sendFinished(sock, err);
    break;

  case ERR_MEM:
    DEBUG(errors) kdprintf(KR_OSTREAM, "tcp_write ERR_MEM.\n", err);
    /* ERR_MEM is a temporary error, or due to len > tcp_sndbuf(sock->pcb).
       It must be the former,
       so we wait for sent_tcp or poll_tcp to be called. */
    // Try tcp_output anyway:
    tcp_output(sock->pcb);
    break;

  case ERR_OK:
    err = tcp_output_nagle(sock->pcb);
    if (err != ERR_OK)
      DEBUG(errors) kdprintf(KR_OSTREAM, "tcp_output returned %d.\n", err);

    sock->sendData += len;
    if ((sock->sendLen -= len) == 0)
      sendFinished(sock, err);
  }
}

// Write TCP data.
static void
TCPSend(Message * msg)
{
  result_t result;

  struct TCPSocket * sock = (struct TCPSocket *)msg->rcv_w3;
	// word from forwarder
  ValidateSock(sock);

  if (msg->rcv_sent > capros_TCPSocket_maxSendLength) {
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  uint32_t totalLen = LWIP_MIN(msg->rcv_limit, msg->rcv_sent);
  uint8_t flags = msg->rcv_w1;
  if (flags & ~(capros_TCPSocket_flagPush | capros_TCPSocket_flagUrgent)) {
    // Invalid flag bits.
    msg->snd_code = RC_capros_key_RequestError;
    return;
  }

  //// Urgent flag is not implemented yet:
  assert(!(flags & capros_TCPSocket_flagUrgent));

  if (sock->sending || sock->TCPSk_state == TCPSk_state_Closed
      || sock->TCPSk_state == TCPSk_state_Closing) {
    msg->snd_code = RC_capros_TCPSocket_Already;
    return;
  }

  sock->sendData = curRcvBuf;
  sock->sendLen = totalLen;
  bool push = flags & (capros_TCPSocket_flagPush
                       | capros_TCPSocket_flagUrgent);	// urgent implies push
  sock->sendPush = push ? 0 : TCP_WRITE_FLAG_MORE;
  sock->sending = true;

  // Switch receive buffers. This saves copying the data.
  unsigned char * tmp = curRcvBuf;
  curRcvBuf = sock->sendBuf;
  sock->sendBuf = tmp;

  // Save the caller:
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_sender,
                                        KR_RETURN, KR_VOID);
  assert(result == RC_OK);
  msg->snd_invKey = KR_VOID;

  do_sendmore(sock);
}

// The caller is responsible for deallocating the tcp_pcb
static void
DestroyTCPConnection(struct TCPSocket * sock)
{
  result_t result;

  assert(sock->recvQNum == 0);
  assert(! sock->receiving);
  assert(! sock->sending);

  // Free the associated forwarder.
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+sco_forwarder,
                                       KR_TEMP0);
  assert(result == RC_OK);
  result = capros_SpaceBank_free1(KR_BANK, KR_TEMP0);
  // Freeing the forwarder invalidates all the capabilities to it.

  result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
                                            slot, slot+sco_numSlots-1);
  DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: Free'ed sock %#x and buffer.\n",
                       __FILE__, __LINE__, sock);

  assert(sock->sendBuf != curRcvBuf);
  mem_free(sock->sendBuf);
#ifdef MALLOC_DEBUG
  // Poison the sendBuf ptr.
  sock->sendBuf = POISON2;
#endif
  mem_free(sock);
}

/*************************** TCP Closing ******************************/

static void
TCPAbort(Message * msg)
{
  struct TCPSocket * sock = (struct TCPSocket *)msg->rcv_w3;
	// word from forwarder
  ValidateSock(sock);

  tcp_abort(sock->pcb);	// this calls err_tcp(..., ERR_ABRT)
}

static bool	// returns true if succeeded
TryClose(struct TCPSocket * sock)
{
  err_t err = tcp_close(sock->pcb);
  if (err == ERR_OK) {
    sock->TCPSk_state = TCPSk_state_Closed;
    return true;
  } else
    return false;
}

static void
TCPClose(Message * msg)
{
  struct TCPSocket * sock = (struct TCPSocket *)msg->rcv_w3;
	// word from forwarder
  ValidateSock(sock);

  if (sock->sending || sock->TCPSk_state == TCPSk_state_Closed) {
    msg->snd_code = RC_capros_TCPSocket_Already;
    return;
  }

  if (! TryClose(sock)) {	// could not close right now
    // Save the caller:
    result_t result;
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
    result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_sender,
                                          KR_RETURN, KR_VOID);
    assert(result == RC_OK);
    sock->sending = true;
    sock->TCPSk_state = TCPSk_state_Closing;
    msg->snd_invKey = KR_VOID;
  }
}

/*************************** TCP Connecting ******************************/

static err_t
poll_tcp(void *arg, struct tcp_pcb *pcb)
{
  struct TCPSocket * sock = arg;
  ValidateSock(sock);

  if (sock->sending) {
    if (sock->TCPSk_state == TCPSk_state_Closing) {
      DEBUG(mem) kprintf(KR_OSTREAM, "%s:%d: retry close\n",
                         __FILE__, __LINE__);
      // We are trying to close. Try again.
      if (TryClose(sock)) {	// succeeded in closing
        sendFinished(sock, ERR_OK);
      }
    } else {
      DEBUG(mem) kprintf(KR_OSTREAM, "%s:%d: retry send\n",
                         __FILE__, __LINE__);
      do_sendmore(sock);
    }
  }
  return ERR_OK;
}

static err_t
sent_tcp(void *arg, struct tcp_pcb *pcb, u16_t len)
{
  return poll_tcp(arg, pcb);
}

// On entry, KR_TEMP1 has forwarder cap.
// Creates socket capability in KR_TEMP1.
static void
CreateSocketCap(struct TCPSocket * sock)
{
  result_t result;

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
}

/* Complete a connection we initiated.
 */
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
    rc = RC_capros_IPDefs_Refused;
    break;

  case ERR_ABRT:
    rc = RC_capros_IPDefs_Aborted;
    break;

  case ERR_OK:
    // Return a TCPSocket capability:
    result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+sco_forwarder,
                                         KR_TEMP1);
    assert(result == RC_OK);
    CreateSocketCap(sock);

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
  ValidateSock(sock);
  assert(sock->TCPSk_state == TCPSk_state_Connect);

  DEBUG(conn) printk("do_connected %d\n", err);

  CompleteConnection(sock, err);

  return ERR_OK;
}

static void
err_tcp(void * arg, err_t err)
{
  struct TCPSocket * sock = arg;
  ValidateSock(sock);	// actually, the pcb may be freed by this time

  DEBUG(errors) kprintf(KR_OSTREAM,
                        "err_tcp %d sock %#x state %d rcvQNum %d\n",
                         err, sock, sock->TCPSk_state, sock->recvQNum);

  switch (err) {
  default:
    assert(false);	// unexpected error

  case ERR_ABRT:
  case ERR_RST:
    switch (sock->TCPSk_state) {
    case TCPSk_state_Connect:
      CompleteConnection(sock, err);
      // Fall into default
    default:
      DEBUG(conn) kprintf(KR_OSTREAM, "ERR_ABRT or RST, closing connection.\n");
      // Discard the recv queue.
      while (sock->recvQNum) {
        ConsumePbuf(sock);
      }

      sock->TCPSk_remoteState |= TCPSk_rstate_Gone;  // tcp_pcb will be freed
      RemoteClosed(sock);

      if (sock->sending) {
        sendFinished(sock, ERR_CLSD);
      }
      sock->TCPSk_state = TCPSk_state_Closed;
      DestroyTCPConnection(sock);
    }
  }
}

/* If successful, returns the socket, and leaves a capability to the
 * forwarder in KR_TEMP1. */
static struct TCPSocket *
CreateSocket(void)
{
  result_t result;

  struct TCPSocket * sock = mem_malloc(sizeof(struct TCPSocket));
  if (!sock)
    goto errExit0;
#ifdef MALLOC_DEBUG
  sock->poison0 = sock->poison1 = POISON1;
#endif

  /* buf must be mem_malloc'ed separately from sock, because it may be
  traded away from this socket. */
  unsigned char * buf = AllocRcvBuf();
  if (!buf)
    goto errExit1;
  DEBUG(alloc) kprintf(KR_OSTREAM, "%s:%d: Alloc'ed sock %#x and buffer.\n",
                       __FILE__, __LINE__, sock);

  // Allocate capability slots for this connection.
  // They are addressed using the sock address.
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          slot, slot+sco_numSlots-1);
  if (result != RC_OK) {
    goto errExit2;
  }

  // Allocate the forwarder now. If this fails we don't want to disturb
  // the target of the connection.
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otForwarder,
		KR_TEMP1);
  if (result != RC_OK) {
    goto errExit3;
  }
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_forwarder,
                                        KR_TEMP1, KR_VOID);
  assert(result == RC_OK);

  sock->sendBuf = buf;
  sock->TCPSk_state = TCPSk_state_None;
  sock->sending = false;
  sock->receiving = false;
  sock->remoteCloseDelivered = false;
  sock->TCPSk_remoteState = 0;
  sock->recvQIn = sock->recvQOut = &sock->recvQ[0];
  sock->recvQNum = 0;
  return sock;

errExit3:
  result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
                                            slot, slot+sco_numSlots-1);
errExit2:
  mem_free(buf);
errExit1:
  mem_free(sock);
errExit0:
  return NULL;
}

static void
SocketInit(struct TCPSocket * sock, struct tcp_pcb * pcb)
{
  sock->pcb = pcb;
  // Set callbacks:
  tcp_arg(pcb, sock);
  tcp_recv(pcb, &recv_tcp);
  tcp_sent(pcb, &sent_tcp);
  tcp_poll(pcb, &poll_tcp, 4);
  tcp_err(pcb, &err_tcp);
}

/*************************** TCP Listening ******************************/

static err_t
accept_tcp(void * arg, struct tcp_pcb *newpcb, err_t err)
{
  struct TCPListenSocket * ls = arg;
  ValidateListenSocket(ls);

  DEBUG(listen) printk("accept_tcp %#x\n", ls);
  if (ls->acceptQNum >= maxAcceptQEntries)
    return ERR_MEM;

  struct TCPSocket * sock = CreateSocket();
  if (!sock) {
    return ERR_MEM;
  }
  SocketInit(sock, newpcb);

  if (ls->listening) {
    DEBUG(listen) printk("accept_tcp waking up\n");
    result_t result;

    CreateSocketCap(sock);

    // Return to caller of Accept.
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)ls;
    result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+ls_accepter,
                                         KR_TEMP0);
    assert(result == RC_OK);
    ls->listening = false;

    Message Msg = {
      .snd_invKey = KR_TEMP0,
      .snd_code = RC_OK,
      .snd_w1 = 0,
      .snd_w2 = 0,
      .snd_w3 = 0,
      .snd_key0 = KR_TEMP1,
      .snd_key1 = KR_VOID,
      .snd_key2 = KR_VOID,
      .snd_rsmkey = KR_VOID,
      .snd_len = 0,
    };

    PSEND(&Msg);	// prompt send
  } else {
    DEBUG(listen) printk("accept_tcp queueing\n");
    // Add to the accept queue:
    *ls->acceptQIn = sock;
    if (++ls->acceptQIn >= &ls->acceptQ[maxAcceptQEntries])
      ls->acceptQIn = &ls->acceptQ[0];	// wrap around
    ls->acceptQNum++;
  }

  return ERR_OK;
}

static void
err_lstcp(void * arg, err_t err)
{
kdprintf(KR_OSTREAM, "err_lstcp %d\n", err);////
////
}

// Accept a connection.
static void
TCPAccept(Message * msg, struct TCPListenSocket * ls)
{
  result_t result;

  if (ls->listening) {
    msg->snd_code = RC_capros_TCPSocket_Already;
    return;
  }

  if (ls->acceptQNum) {	// there is a connection queued
    DEBUG(listen) printk("TCPAccept immediate return\n");
    struct TCPSocket * sock = * ls->acceptQOut;
    ValidateSock(sock);
    if (++ls->acceptQOut >= &ls->acceptQ[maxAcceptQEntries])
      ls->acceptQOut = &ls->acceptQ[0];	// wrap around
    ls->acceptQNum--;
    
    // Return a TCPSocket capability:
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;
    result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+sco_forwarder,
                                         KR_TEMP1);
    assert(result == RC_OK);
    CreateSocketCap(sock);

    msg->snd_key0 = KR_TEMP1;	// override default of KR_VOID
  } else {		// no connection, we must wait
    DEBUG(listen) printk("TCPAccept waiting\n");
    // Save the caller:
    result_t result;
    const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)ls;
    result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+ls_accepter,
                                          KR_RETURN, KR_VOID);
    assert(result == RC_OK);
    ls->listening = true;
    msg->snd_invKey = KR_VOID;
  }
}

// Listen on a local port.
static void
TCPListen(Message * msg)
{
  err_t err;
  result_t result;

  unsigned int portNum = msg->rcv_w1;

  struct TCPListenSocket * ls = mem_malloc(sizeof(struct TCPListenSocket));
  if (!ls) {
    msg->snd_code = RC_capros_IPDefs_NoMem;
    return;
  }

  // Allocate capability slots for this connection.
  // They are addressed using the ls address.
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)ls;
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          slot, slot+ls_numSlots-1);
  if (result != RC_OK) {
    msg->snd_code = RC_capros_IPDefs_NoMem;
    goto errExit1;
  }

  // Allocate the forwarder.
  result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otForwarder,
		KR_TEMP1);
  if (result != RC_OK) {
    msg->snd_code = RC_capros_IPDefs_NoMem;
    goto errExit2;
  }
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+ls_forwarder,
                                        KR_TEMP1, KR_VOID);
  assert(result == RC_OK);

  struct tcp_pcb * pcb = tcp_new();
  if (!pcb) {
    msg->snd_code = RC_capros_IPDefs_NoMem;
    goto errExit3;
  }

  ls->acceptQIn = ls->acceptQOut = &ls->acceptQ[0];
  ls->acceptQNum = 0;
  ls->listening = false;

  // Now bind it to the local port.
  err = tcp_bind(pcb, IP_ADDR_ANY/*???*/, portNum);
  if (err != ERR_OK) {
    msg->snd_code = RC_capros_IPDefs_Already;
    goto errExit4;
  }

  struct tcp_pcb * listenPcb = tcp_listen(pcb);
  if (!listenPcb) {
    msg->snd_code = RC_capros_IPDefs_NoMem;
    goto errExit4;
  }
  ls->pcb = listenPcb;
  // Set callbacks:
  tcp_arg(listenPcb, ls);
  tcp_accept(listenPcb, &accept_tcp);
  //tcp_poll(listenPcb, &poll_tcp, 4);
  tcp_err(listenPcb, &err_lstcp);

  // Return a TCPListenSocket capability:
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_ListenSocket, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Forwarder_swapTarget(KR_TEMP1, KR_TEMP0, KR_VOID);
  assert(result == RC_OK);
  uint32_t dummy;
  result = capros_Forwarder_swapDataWord(KR_TEMP1, (uint32_t)ls, &dummy);
  assert(result == RC_OK);
  result = capros_Forwarder_getOpaqueForwarder(KR_TEMP1,
                        capros_Forwarder_sendWord, KR_TEMP1);
  assert(result == RC_OK);

  msg->snd_key0 = KR_TEMP1;	// override default of KR_VOID
  return;

errExit4:
  err = tcp_close(pcb);
  assert(err == ERR_OK);	// this can't fail, because pcb was never used
errExit3:
  result = capros_SpaceBank_free1(KR_BANK, KR_TEMP1);
errExit2:
  result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
                                            slot, slot+ls_numSlots-1);
errExit1:
  mem_free(ls);
  return;
}

/*************************** TCP Connecting ******************************/

// Create an ipv4 connection.
static void
TCPConnect(Message * msg)
{
  err_t err;
  result_t result;

  uint32_t ipAddr = msg->rcv_w1;	// host format
  unsigned int portNum = msg->rcv_w2;

  struct TCPSocket * sock = CreateSocket();
  if (!sock) {
    msg->snd_code = RC_capros_IPDefs_NoMem;
    return;
  }
  const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)sock;

  struct tcp_pcb * pcb = tcp_new();
  if (!pcb) {
    msg->snd_code = RC_capros_IPDefs_NoMem;
    goto errExit3;
  }

  SocketInit(sock, pcb);

  // Connect.
  sock->TCPSk_state = TCPSk_state_Connect;
  struct ip_addr ipa = {
    .addr = htonl(ipAddr)
  };
  err = tcp_connect(pcb, &ipa, portNum, &do_connected);
  if (err != ERR_OK) {
    assert(err == ERR_MEM);
    msg->snd_code = RC_capros_IPDefs_NoMem;
    goto errExit;
  }

  // Caller will wait until the connection is complete.
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, slot+sco_connecter,
                                        KR_RETURN, KR_VOID);
  assert(result == RC_OK);
  msg->snd_invKey = KR_VOID;
  return;

errExit:
  err = tcp_close(pcb);
  assert(err == ERR_OK);	// should never fail
errExit3:
  DestroyTCPConnection(sock);
  return;
}

/*************************** cap_main ******************************/
// Called from architecture-specific driver_main().

result_t
cap_init(struct IPConfigv4 * ipconf)
{
  result_t result;

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

  printk("Ethernet driver started.\n");

  {
    // Touch each heap page to get VCSK to allocate it.
    // If there isn't enough space in the space bank, we want to know now.
    extern u8_t ram_heap[];
    u8_t * ram = LWIP_MEM_ALIGN(ram_heap);
    uint8_t * p;
    // Kludge because struct mem is local to core/mem.c:
#define sizeofStructMem (sizeof(mem_size_t) * 3)
    uint8_t * ramLast = &ram[LWIP_MEM_ALIGN_SIZE(MEM_SIZE)
                             + sizeofStructMem -1];
    for (p = ram; p <= ramLast; p += EROS_PAGE_SIZE)
      *p = 1;
    *ramLast = 1;
  }

  // Initialize the TCP/IP code:
  lwip_init();

  curRcvBuf = AllocRcvBuf();
  assert(curRcvBuf);

  // Start the timers:
  mod_timer_duration(&arpTimeoutData.tl, (ARP_TMR_INTERVAL * 1000) / TICK_USEC);
  mod_timer_duration(&tcpTimeoutData.tl, (TCP_TMR_INTERVAL * 1000) / TICK_USEC);

  err_t devInitF(struct netif * netif);
  netif_add(&theNetIf, &ipconf->addr, &ipconf->mask, &ipconf->gw,
            NULL, &devInitF, &ethernet_input);
  netif_set_default(&theNetIf);
  netif_set_up(&theNetIf);

  return RC_OK;
}

NORETURN void
cap_main(void)
{
  result_t result;
  Message Msg;
  Message * const msg = &Msg;

  /* Note: We cannot call NPLink until after cap_init has returned and
     its caller has returned to the PCI registry, because subsequently-
     discovered PCI devices may be necessary to support paging,
     which is necessary to fetch NPLink from disk. */
  // Give our cap to nplink.
  result = capros_Process_makeStartKey(KR_SELF, keyInfo_IP, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_VOLSIZE, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_getSlot(KR_TEMP0, volsize_pvolsize, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_getSlot(KR_TEMP0, volsize_nplinkCap, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_NPLink_RegisterNPCap(KR_TEMP0, KR_TEMP1,
             IKT_capros_NPIP, 0);
  assert(result == RC_OK);

  // Give the non-persistent prime space bank key to nplink.
  /* According to the normal pattern, the non-persistent space bank
     would do this itself.
     That would result in deadlock, because the space bank is needed by
     the disk driver which is needed to fetch nplink.
     So we do it here. */
  result = capros_NPLink_RegisterNPCap(KR_TEMP0, KR_BANK,
             IKT_capros_SpaceBank, 0);
  assert(result == RC_OK);

  msg->snd_invKey = KR_VOID;
  msg->snd_key0 = msg->snd_key1 = msg->snd_key2 = msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  /* The void key is not picky about the other parameters,
  so it's OK to leave them uninitialized. */

  for (;;) {
    msg->rcv_key0 = msg->rcv_key1 = msg->rcv_key2 = KR_VOID;
    msg->rcv_rsmkey = KR_RETURN;
    msg->rcv_limit = rcvBufSize;
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

    case keyInfo_IP:
      DEBUG(cap) kprintf(KR_OSTREAM, "Called IP, oc=%#x\n", Msg.rcv_code);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_NPIP;
        break;
  
      case OC_capros_NPIP_connect:
        TCPConnect(&Msg);
        break;
  
      case OC_capros_NPIP_listen:
        TCPListen(&Msg);
        break;
  
      case OC_capros_NPIP_createUDPPort:
        UDPCreate(&Msg);
        break;
      }
      break;

    case keyInfo_ListenSocket:
    {
      DEBUG(cap) kprintf(KR_OSTREAM, "Called LsnSk, oc=%#x\n", Msg.rcv_code);
      struct TCPListenSocket * ls = (struct TCPListenSocket *)Msg.rcv_w3;
      ValidateListenSocket(ls);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_TCPListenSocket;
        break;

      case OC_capros_key_destroy:
      {
        err_t err = tcp_close(ls->pcb);
        (void)err;
        assert(err == ERR_OK);

        const capros_Node_extAddr_t slot = (capros_Node_extAddr_t)ls;
        result = capros_Node_getSlotExtended(KR_KEYSTORE, slot+ls_forwarder,
                                             KR_TEMP1);
        assert(result == RC_OK);
        result = capros_SpaceBank_free1(KR_BANK, KR_TEMP1);
        result = capros_SuperNode_deallocateRange(KR_KEYSTORE,
                                            slot, slot+ls_numSlots-1);
        mem_free(ls);
        break;
      }
  
      case OC_capros_TCPListenSocket_accept:
        TCPAccept(&Msg, ls);
        break;
      }
      break;
    }

    case keyInfo_Socket:
      DEBUG(cap) kprintf(KR_OSTREAM, "Called Sock, oc=%#x\n", Msg.rcv_code);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_TCPSocket;
        break;
  
      case OC_capros_TCPSocket_getRemoteAddr:
      {
        struct TCPSocket * sock = (struct TCPSocket *)Msg.rcv_w3;
        ValidateSock(sock);
        Msg.snd_w1 = ntohl(sock->pcb->remote_ip.addr);
        Msg.snd_w2 = sock->pcb->remote_port;
        break;
      }
  
      case OC_capros_TCPSocket_close:
        TCPClose(&Msg);
        break;
  
      case OC_capros_key_destroy:
      case OC_capros_TCPSocket_abort:
        TCPAbort(&Msg);
        break;
  
      case 0:	// OC_capros_TCPSocket_receive
        TCPReceive(&Msg);
        break;
  
      case 1:	// OC_capros_TCPSocket_send
        TCPSend(&Msg);
        break;
      }
      break;

    case keyInfo_UDPPort:
      DEBUG(cap) kprintf(KR_OSTREAM, "Called UDPPort, oc=%#x\n", Msg.rcv_code);

      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_capros_key_getType:
        Msg.snd_w1 = IKT_capros_UDPPort;
        break;
  
      case OC_capros_key_destroy:
        UDPDestroy(&Msg);
        break;
  
      case OC_capros_UDPPort_getMaxSizes:
        UDPGetMaxSizes(&Msg);
        break;

      case 0:	// OC_capros_UDPPort_receive
        UDPReceive(&Msg);
        break;
  
      case 1:	// OC_capros_UDPPort_send
        UDPSend(&Msg);
        break;
      }
      break;

    case keyInfo_Timer:
      switch (Msg.rcv_code) {
      default:
        Msg.snd_code = RC_capros_key_UnknownRequest;
        break;

      case OC_timer_arp:
        etharp_tmr();
        break;

      case OC_timer_tcp:
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
        Msg.snd_w1 = IKT_capros_IPInt;
        break;
  
      case OC_capros_IPInt_processInterrupt: ;
        // Call the specified function in this thread:
        uint32_t (*fcn)(uint32_t status) = (uint32_t (*)(uint32_t)) Msg.rcv_w1;
        Msg.snd_w1 = (*fcn)(Msg.rcv_w2);
        break;
      }
      break;
    }	// end of switch (keyInfo)
  }	// end of loop forever
}
