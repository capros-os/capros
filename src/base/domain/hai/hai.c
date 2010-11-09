/*
 * Copyright (C) 2008-2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

/* HAI
   This persistent program receives an IP capability for an Ethernet network
   connected to a Home Automation Inc. Omni controller.
   This program monitors and controls the controller.
 */

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/key.h>
#include <idl/capros/Number.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/HAI.h>
#include <idl/capros/IP.h>
#include <idl/capros/RTC.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Logfile.h>
#include <idl/capros/Constructor.h>
#include <domain/cmte.h>
#include <domain/CMTETimer.h>
#include <domain/CMTEThread.h>
#include <domain/CMTESemaphore.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include <crypto/rijndael.h>

#define dbg_server 0x1
#define dbg_data   0x2
#define dbg_errors 0x4
#define dbg_rcvr   0x8

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors)

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Protocol 1 is Omni-Link as described in "HAI Network Communication
     Protocol Description" 20P09-1 Rev A May 2003
     and "Omni-Link Serial Protocol Description" 10P17 Rev H September 2004.
   Protocol 2 is Omni-Link II as described in document 20P00 Rev 2.16
     June 2008. */
#define PROTOCOL 2

#define KR_RTC     KR_CMTE(0)
#define KR_CONFIG1 KR_CMTE(1)
#define KR_CONFIG2 KR_CMTE(2)
#define KR_IP      KR_CMTE(3)
#define KR_LOGFILEC KR_CMTE(4)
#define KR_Socket  KR_CMTE(5)	// UDP port or TCP socket
#define KR_LOGFILE KR_CMTE(6)
// unused	   KR_CMTE(7)
//#define RESPONSE_TEST
#ifdef RESPONSE_TEST
#define KR_SysTrace KR_CMTE(8)	// also initialize this in hai.map
#endif

#define LKSN_Socket LKSN_CMTE

typedef capros_RTC_time_t RTC_time;		// real time, seconds

uint32_t HAIIpAddr, HAIPort;	// IP address and port of HAI
				// constant after initialization

RTC_time
getRTC(void)
{
  RTC_time t;
  result_t result = capros_RTC_getTime(KR_RTC, &t);
  assert(result == RC_OK);
  return t;
}

static void
SleepForNS(capros_Sleep_nanoseconds_t duration)
{
  result_t result = capros_Sleep_sleepForNanoseconds(KR_SLEEP, duration);
  assert(result == RC_OK || result == RC_capros_key_Restart);
}

CMTEMutex_DECLARE_Unlocked(messageLock);
CMTEMutex_DECLARE_Locked(sendLock);	// initially locked by Receiver thread

/************************ crypto stuff ******************************/

// These global variables are accessed only under sendLock.

#define keybits 128
unsigned long rkEncrypt[RKLENGTH(keybits)];
unsigned long rkDecrypt[RKLENGTH(keybits)];
int nrounds;
uint8_t privateKey[16];
uint8_t sessionKey[16];
uint8_t sessionID[5];

// Encrypted data comes in blocks of 16 bytes.
static inline unsigned int
cryptoRoundUp(unsigned int n)
{
  return (n+15) & (-16);
}

static void
UnpackPK(uint32_t pkn, uint8_t * p)
{
  int i;
  for (i = 4; i-- > 0; ) {
    *p++ = (pkn >> (i*8));
  }
}

/************************ protocol stuff ******************************/

enum {
  ol2mtyp_requestNewSess = 1,
  ol2mtyp_ackNewSess,
  ol2mtyp_reqSecConn,
  ol2mtyp_ackSecConn,
  ol2mtyp_clientSessTerm,
  ol2mtyp_contrSessTerm,
  ol2mtyp_rejectNewSess,
  ol2mtyp_dataMsg =
#if (PROTOCOL == 1)
    16
#else
    32
#endif
};

#if (PROTOCOL == 1)
#define startCharacter 0x5a
#define mtype_Ack 0x05
#define mtype_NAck 0x06
#else
#define startCharacter 0x21
#define mtype_Ack 0x01
#define mtype_NAck 0x02
enum objectType {
  ot_zone = 1,
  ot_unit,
  ot_button,
  ot_code,
  ot_area,
  ot_thermostat,
  ot_message,
  ot_auxSensor,
  ot_audioSource,
  ot_audioZone,
  ot_expansionEnclosure,
  ot_console
};
#endif

#define ol2HeaderSize 4	// size of the Omni-Link II application header
#define maxSendMsgLen 128 //// verify this
/* Largest message we can receive on the OmniPro II is
 * Object Status for all 512 units. 
 * Its size is 4 + 2 + 5*512 which is 2566. 
 * But the length field must fit in a byte, so the real max is 255.
 * Round up to a multiple of 16 for whole encryption blocks. */
#define maxRecvMsgLen 256
/* The + 16 below is because we encrypt block 1 into block 0, etc. */
#define sendBufSize (ol2HeaderSize + 16 + maxSendMsgLen)
// Send buffer, including the Omni-Link II application header.
// Protected by sendLock.
uint8_t sendBuf[sendBufSize];
	// sendBuf[3] always remains zero
// Where to put the unencrypted message to send:
#define sendMessage (sendBuf + ol2HeaderSize + 16)

/************************ synchronization stuff ******************************/

CMTESemaphore_DECLARE(messageSem, 0);

// sendState is protected by sendLock.
enum {
  mstate_Available,
  mstate_NeedSession,
  mstate_NeedResponse
} sendState = mstate_Available;

// When we are awakened from mstate_NeedResponse, we get back
// responseMessageLength and responseMessage.
// responseMessageLength is the length of the application data message,
// from the length byte, which includes 1 for the message type,
// and excludes the start character, length byte, and CRC.
int responseMessageLength;
uint8_t responseMessage[maxRecvMsgLen];
/* When sendState != mstate_Available,
 *   responseMessage and responseMessageLength are written only by
 *   the holder of sendLock.
 *   The holder of sendLock is waiting on messageSem, and will be
 *   awakened by the ReceiverThred.
 * When sendState == mstate_Available,
 *   responseMessage and responseMessageLength are written only by
 *   the ReceiverThread.
 */

/************************ message stuff ******************************/

uint16_t
CalcCRC(uint8_t * data, unsigned int len)
{
  uint16_t CRC = 0;
  int i;
  for (i = 0; i < len; i++, data++) {
    static const int Poly = 0xa001;	// CRC-16 polynomial
    int j;
    CRC ^= *data;
    for (j = 0; j < 8; j++) {
      bool flag = (CRC & 1);
      CRC >>= 1;
      if (flag)
        CRC ^= Poly;
    }
  }
  return CRC;
}

// sequenceNumber is protected by sendLock.
unsigned int sequenceNumber = 0;	// incremented to 1 before first use

// Must hold sendLock.
static void
SetSeqNo(uint16_t seqNo)
{
  sendBuf[0] = seqNo >> 8;
  sendBuf[1] = seqNo & 0xff;
}

// Must hold sendLock.
void
IncSeqNo(void)
{
  if (++sequenceNumber == 65535)
    sequenceNumber = 1;	// wrap to 1, not zero
}

/* Convert two bytes in Big Endian format (most significan byte first)
 * to a short. */
static unsigned short
BEToShort(uint8_t * p)
{
  uint8_t msb = *p++;
  return (msb << 8) + *p++;
}

/* Convert a short to two bytes in Big Endian format
 * (most significan byte first). */
static void
ShortToBE(unsigned short v, uint8_t * p)
{
  *p++ = (v >> 8);
  *p = v & 0xff;
}

void KeepAliveTimerFunction(unsigned long data);
CMTETimer_Define(sendTmr, &KeepAliveTimerFunction, 0);

// Send data that already has an Omni-Link II application header
// and already has any necessary encryption.
// The data is in sendBuf. len includes the Omni-Link II application header.
// Must hold sendLock.
// Returns false iff the port capability is void.
bool
NetSend(unsigned int len)
{
  result_t result;

  // Set timer for 4.5 minutes.
  CMTETimer_setDuration(&sendTmr, 270000000000ULL);
#if (PROTOCOL == 1)
  result = capros_UDPPort_send(KR_Socket, HAIIpAddr, HAIPort,
             len, sendBuf);
#else // TCP
  result = capros_TCPSocket_send(KR_Socket, len,
                                 capros_TCPSocket_flagPush, sendBuf);
#endif
  switch (result) {
  default: ;
    kdprintf(KR_OSTREAM, "HAI: TCPSocket_send got %#x\n", result);

  case RC_capros_key_Restart:
  case RC_capros_key_Void:
    return false;

  case RC_OK:
    return true;
  };
}

/* sendMessage has a packet of length totalLen.
 * sendBuf[2] has the message type of the packet.
 * Send the packet. */
// Must hold sendLock.
// Returns false iff the port capability is void.
static bool
EncryptedSend(unsigned int totalLen)
{
  // Set up sequence number:
  IncSeqNo();
  unsigned int seqNo = sequenceNumber;	// local copy
  SetSeqNo(seqNo);

  DEBUG(data) {
    kprintf(KR_OSTREAM, "EncryptedSend len=%d seq %d type %d",
          totalLen, seqNo, sendBuf[2]);
    int i;
    for (i = 0; i < totalLen; i++) {
      kprintf(KR_OSTREAM, " %#x", sendMessage[i]);
    }
    kprintf(KR_OSTREAM, "\n");
  }

  // Pad to a multiple of 16 bytes.
  while (totalLen & 0xf) {
    sendMessage[totalLen++] = 0;
  }
  assert(totalLen <= maxSendMsgLen);

  // Encrypt all blocks, moving them down next to the header:
  uint8_t * p;
  uint8_t * c = sendBuf + ol2HeaderSize;
  unsigned int blocks;
  for (blocks = totalLen / 16; blocks > 0; blocks--) {
    p = c + 16;
    *p ^= (seqNo >> 8);
    *(p+1) ^= (seqNo & 0xff); 
    rijndaelEncrypt(rkEncrypt, nrounds, p, c);
    c = p;
  }

#if 0
  DEBUG(data) {
    kprintf(KR_OSTREAM, "Encrypted");
    int i;
    for (i = 0; i < ol2HeaderSize+totalLen; i++) {
      kprintf(KR_OSTREAM, " %#x", sendBuf[i]);
    }
    kprintf(KR_OSTREAM, "\n");
  }
#endif
  return NetSend(ol2HeaderSize + totalLen);
}

/* Send an Omni-Link II application data message. 
 * The message is in sendMessage ff., except for the first two
 * bytes and the CRC, which this procedure fills in.
 * len is the value for the second byte of the message. */
// Must hold sendLock.
/* If the port capability is void, this procedure returns false.
 * Otherwise it returns true. */
bool
OL2Send(unsigned int len)
{
  sendMessage[0] = startCharacter;
  sendMessage[1] = len;

  uint16_t CRC = CalcCRC(&sendMessage[1], len+1);
  sendMessage[2 + len] = CRC & 0xff;
  sendMessage[2 + len + 1] = CRC >> 8;

  sendBuf[2] = ol2mtyp_dataMsg;

  return EncryptedSend(len + 4);
}

/**************************** receiver thread *************************/

/* The + 16 below is because we encrypt block 1 into block 0, etc. */
#define recvBufSize (ol2HeaderSize + 16 + maxRecvMsgLen)
// Receive buffer, including the Omni-Link II application header.
uint8_t recvrBuf[recvBufSize];

// Number of bytes of data in recvrBuf
// (data starts at the beginning of the buffer):
unsigned int recvrDataBytes = 0;

// Number of bytes of data in the current message.
unsigned int currentPacketBytes;

// Where to get the unencrypted received message:
#define recvrMessage (recvrBuf + ol2HeaderSize)

void
ConsumeMessage(void)
{
  assert(currentPacketBytes <= recvrDataBytes);

  recvrDataBytes -= currentPacketBytes;
  if (recvrDataBytes) {		// some data left
    /* Move the data down to the beginning of the buffer.
       This happens so rarely, we don't care that it's a little inefficient. */
    memmove(recvrBuf, recvrBuf+currentPacketBytes, recvrDataBytes);
  }
  currentPacketBytes = 0;
}

/* Ensure recvrBuf has at least minBytes bytes of data,
 * reading data from the net if necessary.
 * If the port capability is void, returns false, else true. */
bool
EnsureRecvData(unsigned int minBytes)
{
  result_t result;
  uint32_t lenRecvd;

  assert(minBytes <= recvBufSize);

  while (recvrDataBytes < minBytes) {
    unsigned int maxToReceive = recvBufSize - recvrDataBytes;
    uint8_t * whereToReceive = recvrBuf + recvrDataBytes;

#if (PROTOCOL == 1)
    uint32_t sourceIPAddr;
    uint16_t sourceIPPort;
    result = capros_UDPPort_receive(KR_Socket, maxToReceive,
			&sourceIPAddr, &sourceIPPort,
                          &lenRecvd, whereToReceive);
    if (result != RC_OK)
      return false;
    DEBUG(server) kprintf(KR_OSTREAM, "Received %d bytes from %#x:%d:\n",
            lenRecvd, sourceIPAddr, sourceIPPort);
#else // TCP
    uint8_t flagsRecvd;
    result = capros_TCPSocket_receive(KR_Socket, maxToReceive,
                                      &lenRecvd, &flagsRecvd, whereToReceive);
    switch (result) {
    default:
      assert(false);
    case RC_capros_TCPSocket_RemoteClosed:
      capros_key_destroy(KR_Socket);
    case RC_capros_key_Restart:
    case RC_capros_key_Void:
      return false;
    case RC_OK:
      break;
    }
    DEBUG(server) kprintf(KR_OSTREAM, "Received %d bytes\n", lenRecvd);
#endif

    DEBUG(data) {
      int i;
      for (i = 0; i < lenRecvd; i++) {
        kprintf(KR_OSTREAM, " %#x", whereToReceive[i]);
      }
      kprintf(KR_OSTREAM, "\n");
    }
    recvrDataBytes += lenRecvd;
  }
  currentPacketBytes = minBytes;
  return true;
}

unsigned int messageSeqNo;

static unsigned int
GetSeqNo(void)
{
  messageSeqNo = (recvrBuf[0] << 8) | recvrBuf[1];
  return messageSeqNo;
}

unsigned int decryptedBytes;

// Decrypt data from ol2HeaderSize+decryptedBytes to currentPacketBytes.
void
DecryptData(void)
{
  unsigned int decryptableBytes = currentPacketBytes - ol2HeaderSize;
  assert(! (decryptableBytes & (16-1)));	// must be a multiple of 16
  while (decryptedBytes < decryptableBytes) {
    // Decrypt one block.
    uint8_t temp[16];
    uint8_t * c = recvrBuf + ol2HeaderSize + decryptedBytes;
    rijndaelDecrypt(rkDecrypt, nrounds, c, temp);
    temp[0] ^= messageSeqNo >> 8;
    temp[1] ^= messageSeqNo & 0xff; 
    memcpy(c, temp, 16);
    DEBUG(data) {
      kprintf(KR_OSTREAM, "Decrypted block");
      int i;
      for (i = 0; i < 16; i++) {
        kprintf(KR_OSTREAM, " %#x", c[i]);
      }
      kprintf(KR_OSTREAM, "\n");
    }
    decryptedBytes += 16;
  }
}

bool
GetSequencedHeader(void)
{
  while (1) {
    if (! EnsureRecvData(ol2HeaderSize))
      return false;
    if (GetSeqNo() == sequenceNumber
        || messageSeqNo == 0)
      return true;
    DEBUG(errors)
      kprintf(KR_OSTREAM, "Expecting seq no %d got %d, discarding\n",
              sequenceNumber, messageSeqNo);
    assert(false);	// FIXME read and consume the discarded message
			// this really shouldn't happen with TCP
  }
}

/* 
 * Receive an OmniLink II application message.
 * Must hold sendLock.
 * If the session was terminated, this procedure returns -1.
 * Otherwise it returns the length of the data plus 1 for the message type
 *   (the length excludes the start character, length byte, and CRC).
 */
int
OL2Receive(void)
{
#ifdef RESPONSE_TEST
  // Measure time to this point.
  uint32_t keyType;
  result_t result = capros_key_getType(KR_SysTrace, &keyType);
  if (result) kdprintf(KR_OSTREAM, "SysTrace_getType got %#x.\n", result);
#endif
  ConsumeMessage();	// consume the previous message if any
  if (! GetSequencedHeader())
    return -1;
  switch (recvrBuf[2]) {		// message type in packet header
  default: ;
    DEBUG(errors)
      kdprintf(KR_OSTREAM, "HAI: expecting app msg, got %d!\n", recvrBuf[2]);
    // Fall into ol2mtyp_contrSessTerm
  case ol2mtyp_contrSessTerm:	// the session is gone
    DEBUG(rcvr) kdprintf(KR_OSTREAM, "HAI terminated session.\n");
    capros_key_destroy(KR_Socket);
    return -1;

  case ol2mtyp_dataMsg:
    break;
  }
  decryptedBytes = 0;
  // Get the minimum application data message:
  if (! EnsureRecvData(ol2HeaderSize + cryptoRoundUp(5)))
    return -1;
  DecryptData();

  /* Notifications for phone ring and off-hook have a bug: no start char
     and no CRC. */
  ////bool workaround = recvrMessage[2] == 0x37;
  bool workaround = false;
  if (! workaround) {
    assert(recvrMessage[0] == startCharacter);
  }
  unsigned int appLen = recvrMessage[1];  // length of data + 1 for msg type
  // Get the rest of this message:
  // +4 below is for start char, length byte, and CRC:
  if (! EnsureRecvData(ol2HeaderSize + cryptoRoundUp(appLen + 4)))
    return -1;
  DecryptData();
  if (! workaround) {
    uint16_t CRC = CalcCRC(&recvrMessage[1], appLen+1);
    assert(recvrMessage[2 + appLen] == (CRC & 0xff));
    assert(recvrMessage[2 + appLen + 1] == (CRC >> 8));
  }
  return appLen;
}

void
SessionTimerFunction(unsigned long data)
{
  result_t result;

  DEBUG(errors) kprintf(KR_OSTREAM, "HAI timed out setting up session.\n");

  /* Destroy the port/socket. This will abort any connection and
  wake up the receiver with an exception. */
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_Socket, KR_TEMP0);
  assert(result == RC_OK);
  capros_key_destroy(KR_TEMP0);
}

// Create a port.
static void
CreatePort(void)
{
  result_t result;

#if (PROTOCOL == 1)
  DEBUG(server) kprintf(KR_OSTREAM, "Creating UDP port.\n");
  result = capros_IP_createUDPPort(KR_IP, KR_Socket);
  assert(result == RC_OK);

  uint32_t maxReceiveSize, maxSendSize;
  result = capros_UDPPort_getMaxSizes(KR_Socket, HAIIpAddr,
			&maxReceiveSize, &maxSendSize);
  assert(result == RC_OK);
  DEBUG(server) kprintf(KR_OSTREAM, "Max size rcv %d snd %d\n", maxReceiveSize, maxSendSize);
  // We had better be able to send a message in a single packet:
  assert(maxSendSize >= maxSendMsgLen + ol2HeaderSize);
#else	// TCP
  DEBUG(server) kprintf(KR_OSTREAM, "Connecting using TCP.\n");

reconnect:
  result = capros_IP_TCPConnect(KR_IP, HAIIpAddr, HAIPort,
			        KR_Socket);
  switch (result) {
  default:
    kdprintf(KR_OSTREAM, "%s:%d: result is %#x!\n",
             __FILE__, __LINE__, result);
  case RC_OK:
    break;
  case RC_capros_IPDefs_Aborted:
    // This can happen if the network connection to the HAI
    // is temporarily broken.
    kprintf(KR_OSTREAM, "HAI: Connection aborted.\n");
    SleepForNS(64UL << 30);	// Sleep about 1 minute
  case RC_capros_key_Restart:
    goto reconnect;
  case RC_capros_IPDefs_Refused:
    kdprintf(KR_OSTREAM, "HAI: Connection refused.\n");
    break;
  }

  DEBUG(server) kprintf(KR_OSTREAM, "Connected.\n");
#endif

  // Save it where the other threads can get it:
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_Socket, KR_Socket,
             KR_VOID);
  assert(result == RC_OK);
}

// Inline because only called from one place.
static inline bool
SetUpSession2(void)
{
  memcpy(sessionKey, privateKey, 16);
  int i;
  for (i = 0; i < 5; i++) {
    sessionKey[11+i] ^= sessionID[i];
  }

  // Set up crypto:
  nrounds = rijndaelSetupEncrypt(rkEncrypt, sessionKey, keybits);
  nrounds = rijndaelSetupDecrypt(rkDecrypt, sessionKey, keybits);

  DEBUG(server) kprintf(KR_OSTREAM, "Requesting secure session.\n");
  // This seems to have no purpose other than to verify the connection.
  sendBuf[2] = ol2mtyp_reqSecConn;
  memcpy(sendMessage, sessionID, 5);
  if (! EncryptedSend(5))
    return false;
  CMTETimer_Define(tmr2, &SessionTimerFunction, 0);
  // Set a timeout for the response.
  CMTETimer_setDuration(&tmr2, 3000000000ULL);	// 3 seconds
  if (! GetSequencedHeader()) {
    CMTETimer_delete(&tmr2);
    return false;
  }
  if (recvrBuf[2] != ol2mtyp_ackSecConn) {
    DEBUG(server)
      kprintf(KR_OSTREAM, "Expecting ackSecConn got %d!\n", recvrBuf[2]);
    capros_key_destroy(KR_Socket);
    return false;
  }
  bool b = EnsureRecvData(ol2HeaderSize + cryptoRoundUp(5));
  CMTETimer_delete(&tmr2);
  if (!b)
    return false;
  decryptedBytes = 0;
  DecryptData();
  if (memcmp(recvrMessage, sessionID, 5)) {
    // Returned sessionID doesn't match.
    DEBUG(server)
      kdprintf(KR_OSTREAM, "Expecting sessID at %#x got at %#x!\n",
               sessionID, recvrMessage);
    capros_key_destroy(KR_Socket);
    return false;
  }
#if (PROTOCOL > 1) 
  // Enable notifications.
  // (Firmware v. 2.16a has bugs in notifications.)
  sendMessage[2] = 0x15;
  sendMessage[3] = 1;
  if (! OL2Send(2))
    return false;
  // Set a timeout for the response.
  CMTETimer_setDuration(&tmr2, 3000000000ULL);	// 3 seconds
  int recvLen = OL2Receive();
  CMTETimer_delete(&tmr2);
  if (recvLen < 1 || recvrMessage[2] != mtype_Ack) {
    DEBUG(errors) kprintf(KR_OSTREAM,
      "HAI respose to enable notif was msg type %#x.\n",
      recvrMessage[2] );
    return false;	// some problem
  }
#endif
  return true;
}

void
WriteNotification(void * notif, unsigned int sz, uint32_t * pTrailer)
{
  result_t result;

  // Fill in header and trailer:
  capros_Logfile_recordHeader * rh = (capros_Logfile_recordHeader *)notif;
  rh->length = sz;
  *pTrailer = sz;
  rh->rtc = getRTC();
  
  result = capros_Sleep_getPersistentMonotonicTime(KR_SLEEP, &rh->id);
  assert(result == RC_OK);

  result = capros_Logfile_appendRecord(KR_LOGFILE, sz, (uint8_t *)notif);
  assert(result == RC_OK);
}

unsigned int ReceiverThreadNum;
void *
ReceiverThread(void * arg)
{
  result_t result;

  // We hold the sendLock initially.
  while (1) {
    // There is no session.
    if (sendState == mstate_NeedResponse) {
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr: responding no session\n");
      responseMessageLength = -1;
      sendState = mstate_Available;	// no longer needing a response
      CMTESemaphore_up(&messageSem);
    } else {
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr: no session, sendState=%d\n",
                          sendState);
    }
    // Create a session.
    while (1) {	// loop until successful
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr: Requesting session.\n");
      IncSeqNo();
      SetSeqNo(sequenceNumber);
      sendBuf[2] = ol2mtyp_requestNewSess;
      if (NetSend(ol2HeaderSize)) {
        CMTETimer_Define(tmr2, &SessionTimerFunction, 0);

        // Set a timeout for the response.
        CMTETimer_setDuration(&tmr2, 3000000000ULL);	// 3 seconds
        if (EnsureRecvData(ol2HeaderSize)) {
          if (GetSeqNo() == sequenceNumber
              && recvrBuf[2] == ol2mtyp_ackNewSess) {
            bool b = EnsureRecvData(ol2HeaderSize + 7);
            CMTETimer_delete(&tmr2);
            if (b) {
              // Check protocol version:
              unsigned short protVer = BEToShort(&recvrBuf[ol2HeaderSize]);
              if (protVer == 1) {
                memcpy(sessionID, &recvrBuf[ol2HeaderSize + 2], 5);
                ConsumeMessage();
                if (SetUpSession2())
                  break;
              } else {
                DEBUG(errors)
                  kprintf(KR_OSTREAM, "HAI: got protocol version %d!\n",
                          protVer);
                ConsumeMessage();
              }
            } else {	// packet header is bad
              DEBUG(errors)
                kprintf(KR_OSTREAM, "HAI: expecting new sess, got %d %d!\n",
                        GetSeqNo(), recvrBuf[2]);
              capros_key_destroy(KR_Socket);
            }
          } else {
            CMTETimer_delete(&tmr2);
          }
        } else {	// lost the connection
          CMTETimer_delete(&tmr2);
        }
      }
      // The port is gone, or we are creating a new session due to an error.
      CreatePort();
    }
    // We now have a session.
    DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr: got session.\n");
    // Push it to the main thread.
    result = capros_Node_getSlotExtended(KR_KEYSTORE,
               LKSN_THREAD_PROCESS_KEYS+0, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_Process_swapKeyReg(KR_TEMP0, KR_Socket, KR_Socket, KR_VOID);
    assert(result == RC_OK);
    // Wake him up if waiting:
    if (sendState == mstate_NeedSession) {
      sendState = mstate_Available;	// no longer needing a session
      CMTESemaphore_up(&messageSem);
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr woke sender for session.\n");
    }

    while (1) {		// loop reading from the session
      CMTEMutex_unlock(&sendLock);
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr receiving.\n");
      int recvLen = OL2Receive();
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr received.\n");
      CMTEMutex_lock(&sendLock);
      if (recvLen < 0)
        break;		// session is gone
      if (GetSeqNo() == 0) {
        // It is a notification.
        assert(recvLen >= 3);	// else HAI error FIXME
        unsigned int msgType = recvrMessage[2];
        unsigned int objType = recvrMessage[3];
#if 0
        unsigned int objNum = (recvrMessage[5] << 8) + recvrMessage[6];
        kprintf(KR_OSTREAM, "++++HAI notif %#x %#x %#x %d\n",
                msgType, objType, recvrMessage[4], objNum);
#endif
        // Parse the notification.
        switch (msgType) {
        default: ;
          kdprintf(KR_OSTREAM, "HAI notif type %#x invalid.\n", msgType);
          break;

        case 0x3b: ;	// Object extended status
#define checkObjectStatusLength(n) \
           if (recvrMessage[4] != n) \
             kdprintf(KR_OSTREAM, "HAI notif length invalid.\n");
          uint8_t * objData = &recvrMessage[5];
          switch (objType) {
          default: ;
            kdprintf(KR_OSTREAM, "HAI notif status type %#x invalid.\n", objType);
            break;

          case 0x01:	// zone status
          {
            checkObjectStatusLength(4);
            // loop over zones reported.
            for (; recvLen >= 6; objData += 4, recvLen -= 4) {
              capros_HAI_ZoneNotification notif = {
                .type = capros_HAI_NotificationType_Zone,
                .objectNumber = (objData[0] << 8) + objData[1],
                .status = objData[2],
                .loopReading = objData[3],
                .padding1 = 0,
                .padding2 = 0,
                .padding3 = 0
              };
              WriteNotification(&notif, sizeof(notif), &notif.trailer);
            }
            break;
          }

          case 0x02:	// unit status
          {
            checkObjectStatusLength(5);
            // loop over units reported.
            for (; recvLen >= 7; objData += 5, recvLen -= 5) {
              capros_HAI_UnitNotification notif = {
                .type = capros_HAI_NotificationType_Unit,
                .objectNumber = (objData[0] << 8) + objData[1],
                .status = objData[2],
                .time = (objData[3] << 8) + objData[4],
                .padding2 = 0,
                .padding3 = 0
              };
              WriteNotification(&notif, sizeof(notif), &notif.trailer);
            }
            break;
          }

          case 0x06:	// thermostat status
          {
            checkObjectStatusLength(14);
            // loop over thermostats reported.
            for (; recvLen >= 16; objData += 14, recvLen -= 14) {
              capros_HAI_ThermostatNotification notif = {
                .type = capros_HAI_NotificationType_Thermostat,
                .objectNumber = (objData[0] << 8) + objData[1],
                .status = objData[2],
                .temperature = objData[3],
                .heatSetpoint = objData[4],
                .coolSetpoint = objData[5],
                .systemMode = objData[6],
                .fanMode = objData[7],
                .holdStatus = objData[8],
                .humidity = objData[9],
                .humidifySetpoint = objData[10],
                .dehumidifySetpoint = objData[11],
                .outdoorTemperature = objData[12],
                .HCHDStatus = objData[13],
                .padding = 0,
                .padding2 = 0
              };
              WriteNotification(&notif, sizeof(notif), &notif.trailer);
            }
            break;
          }

          case 0x08:	// aux sensor status
          {
            checkObjectStatusLength(6);
            // loop over aux sensors reported.
            for (; recvLen >= 8; objData += 6, recvLen -= 6) {
              capros_HAI_AuxSensorNotification notif = {
                .type = capros_HAI_NotificationType_AuxSensor,
                .objectNumber = (objData[0] << 8) + objData[1],
                .outputStatus = objData[2],
                .value = objData[3],
                .lowSetpoint = objData[4],
                .highSetpoint = objData[5],
                .padding1 = 0,
                .padding3 = 0
              };
              WriteNotification(&notif, sizeof(notif), &notif.trailer);
            }
            break;
          }

          // Unsupported object notifications:
          case 0x05:	// area
          case 0x07:	// message
          case 0x0a:	// audio zone
          case 0x0e:	// access control reader
          case 0x0f:	// access control reader lock
            break;
          }
          break;

        case 0x37:
        {
          uint8_t * objData = &recvrMessage[3];
          for (; recvLen >= 3; objData += 2, recvLen -= 2) {
            capros_HAI_OtherNotification notif = {
              .type = capros_HAI_NotificationType_Other,
              .event = (objData[0] << 8) + objData[1]
            };
            WriteNotification(&notif, sizeof(notif), &notif.trailer);
          }
        }
        }
      } else {
        // It is a response.
        if (sendState == mstate_NeedResponse) {
          responseMessageLength = recvLen;
          // +4 below to include start char, length bytes, and CRC
          memcpy(responseMessage, recvrMessage, recvLen+4);
          sendState = mstate_Available;	// no longer needing a response
          CMTESemaphore_up(&messageSem);
          DEBUG(rcvr) kprintf(KR_OSTREAM,
                              "HAI rcvr woke sender for response.\n");
        }
      }
    }
  }
}

/************** procedures for main thread and keep-alive timer *************/

static void
GetLocks(void)
{
  // Note: locks must be gotten in this order.
  CMTEMutex_lock(&messageLock);
  CMTEMutex_lock(&sendLock);
  assert(sendState == mstate_Available);
}

static void
ReleaseLocks(void)
{
  assert(sendState == mstate_Available);
  CMTEMutex_unlock(&sendLock);
  CMTEMutex_unlock(&messageLock);
}

void
TimerFunction(unsigned long data)
{
  result_t result;

  DEBUG(errors) kprintf(KR_OSTREAM, "HAI receive timed out.\n");

  /* Destroy the port/socket. This will abort any connection and
  wake up the receiver with an exception.
  Seeing sendState == mstate_NeedResponse, the receiver will wake up the
  sending thread. */
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_Socket, KR_TEMP0);
  assert(result == RC_OK);
  capros_key_destroy(KR_TEMP0);
}

/* This procedure is called under the messageLock and sendLock.
 * (Note: get messageLock before sendLock.)
 * It releases the sendLock internally.
 *
 * If the session was terminated, this procedure returns -1.
 * Otherwise, if a Negative Acknowledge is received,
 *   this procedure returns -2.
 * Otherwise, if neither the expected replyType nor NAck was received,
 *   or replyType was received but the message length is less than
 *   minReplyLen,
 *   this procedure returns -3 (HAI error).
 * Otherwise it returns the length of the data plus 1 for the message type
 *   (the length excludes the start character, length byte, and CRC).
 */
int
OL2GetResponse(unsigned int replyType, unsigned int minReplyLen)
{
  // Wait for a response.
  sendState = mstate_NeedResponse;
  CMTEMutex_unlock(&sendLock);
  // Set a timeout for the response.
  CMTETimer_Define(tmr, &TimerFunction, 0);
  CMTETimer_setDuration(&tmr, 3000000000ULL);	// set timer for 3 seconds
  CMTESemaphore_down(&messageSem);
  CMTETimer_delete(&tmr);
  CMTEMutex_lock(&sendLock);
  if (responseMessageLength < 0) {
    return -1;
  }
  // Check the expected reply:
  if (responseMessage[2] == replyType) {
    if (responseMessageLength >= minReplyLen)
      return responseMessageLength;
    else {
      DEBUG(errors) kprintf(KR_OSTREAM, "HAI: response %d got length %d!\n",
                            replyType, responseMessageLength);
    }
  } else if (responseMessage[2] == mtype_NAck) {
    DEBUG(errors) kprintf(KR_OSTREAM, "HAI got NACK\n");
    return -2;
  } else {
    DEBUG(errors) kdprintf(KR_OSTREAM, "HAI: response expected %d got %d!\n",
                          replyType, responseMessage[2]);
  }
  return -3;
}

/**************************** keep-alive timer *************************/

/* The HAI controller will stop responding if it does not receive a
 * message for 5 minutes.
 * So, if we haven't sent a message for 4.5 minutes, the sendTmr expires,
 * and we will send a message - namely, an EnableNotifications command
 * (simply because it responds with a simple Ack).
 */

void
KeepAliveTimerFunction(unsigned long data)
{
  result_t result;

  GetLocks();

  // Get the current socket capability.
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_Socket, KR_Socket);
  assert(result == RC_OK);

#if (PROTOCOL != 1)
  // Send "enable notifications" message.
  // We send this particular message simply because
  // it responds with a simple Ack.
  sendMessage[2] = 0x15;
  sendMessage[3] = 1;
  if (OL2Send(2)) {
    OL2GetResponse(mtype_Ack, 1);
    // We don't care if OL2GetResponse had a problem.
    // We just succeeded in sending a message.
  }
  // else if OL2Send failed, we have worse problems than keeping alive;
  // some other thread will take care of it.
#endif
  ReleaseLocks();
}

/************************* procedures for main thread ************************/

// Called holding the sendLock; temporarily releases the sendLock.
static inline void
WaitForSession(void)
{
  sendState = mstate_NeedSession;
  CMTEMutex_unlock(&sendLock);
  CMTESemaphore_down(&messageSem);
  CMTEMutex_lock(&sendLock);
}

/* This procedure is called under messageLock and sendLock.
 * It temporarily releases the sendLock internally.
 *
 * If the session was terminated, this procedure
 *   waits for a new session if possible,
 *   returns -1,
 *   and does not retry the operation.
 *   The send may or may not have occurred.
 * Otherwise, if a Negative Acknowledge is received,
 *   this procedure returns -2.
 * Otherwise it returns the length of the data plus 1 for the message type
 *   (the length excludes the start character, length byte, and CRC).
 */
int
OL2SendAndGetReply(unsigned int sendLen, unsigned int replyType,
                   unsigned int minReplyLen)
{
  if (! OL2Send(sendLen)) {
    // The port is gone.
    WaitForSession();
    return -1;
  }
  int i = OL2GetResponse(replyType, minReplyLen);
  if (i == -3) {
    // Got an invalid response from the HAI. Reset the connection:
    kdprintf(KR_OSTREAM, "HAI: Closing connection due to error.\n");
    capros_key_destroy(KR_Socket);
    WaitForSession();
    return -1;
  }
  /* If i == -1, the port was gone;
     however it may have been recreated before we
     reacquired sendLock. Therefore we mustn't call WaitForSession here. */
  return i;
}

static void
SimpleRequest(uint8_t typeCode,
              unsigned int replyType, unsigned int minReplyLen)
{
  while(1) {	// loop until successful
    GetLocks();
    sendMessage[2] = typeCode;
    int recvLen = OL2SendAndGetReply(1, replyType, minReplyLen);
    ReleaseLocks();
    if (recvLen >= 0) {
      return;
    }
    assert(recvLen == -1);	// NACK not handled yet
  }
}

/**************************** main server loop *************************/

int
cmte_main(void)
{
  result_t result;

  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .snd_code = 0,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,

    .rcv_key0 = KR_ARG(0),
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_data = NULL,
    .rcv_limit = 0
  };

  result = CMTETimer_setup();
  assert(result == RC_OK);	// FIXME
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          LKSN_Socket, LKSN_Socket);
  assert(result == RC_OK);	// FIXME

  // Create Logfile:
  result = capros_Constructor_request(KR_LOGFILEC, KR_BANK, KR_SCHED, KR_VOID,
             KR_LOGFILE);
  assert(result == RC_OK);	// FIXME
  // Save data only 2 days, because it will soon be copied into other logs.
  result = capros_Logfile_setDeletionPolicyByID(KR_LOGFILE,
             2*24*60*60*1000000000ULL);
  assert(result == RC_OK);

  // Get configuration data:
  uint32_t pk0, pk1, pk2, pk3;	// bytes of the private key
  result = capros_Number_get(KR_CONFIG1, &HAIIpAddr, &HAIPort, &pk0);
  assert(result == RC_OK);
  result = capros_Number_get(KR_CONFIG2, &pk1, &pk2, &pk3);
  assert(result == RC_OK);
  UnpackPK(pk0, &privateKey[0]);
  UnpackPK(pk1, &privateKey[4]);
  UnpackPK(pk2, &privateKey[8]);
  UnpackPK(pk3, &privateKey[12]);

  result = CMTEThread_create(4096, &ReceiverThread, 0, &ReceiverThreadNum);
  assert(result == RC_OK);	// FIXME

  for (;;) {
    RETURN(&Msg);

    DEBUG(server) kprintf(KR_OSTREAM, "hai was called, OC=%#x\n",
                          Msg.rcv_code);

    // Defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;
    Msg.snd_key0 = KR_VOID;

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_HAI;
      break;

    case OC_capros_HAI_getNotificationsLog:
      // This cap is read/write, so the recipient can delete records
      // once they are no longer needed.
      Msg.snd_key0 = KR_LOGFILE;
      break;

    case OC_capros_HAI_getSystemStatus:
    {
#if (PROTOCOL == 1)
      SimpleRequest(0x13, 0x14, 15);
#else
      SimpleRequest(0x18, 0x19, 15);
#endif
      Msg.snd_data = &responseMessage[3];
      Msg.snd_w1 = getRTC();
      Msg.snd_len = sizeof(capros_HAI_SystemStatus);
      break;
    }

    case OC_capros_HAI_getUnitStatus:
    {
      unsigned int unit = Msg.rcv_w1;
      GetLocks();
      while (1) {	// loop until successful
#if (PROTOCOL == 1)
        sendMessage[2] = 0x17;
        ShortToBE(unit, &sendMessage[3]);
        ShortToBE(unit, &sendMessage[5]);
        int recvLen = OL2SendAndGetReply(5, 0x18, 4);
        if (recvLen >= 0) {
          Msg.snd_w2 = responseMessage[3];
          Msg.snd_w3 = BEToShort(&responseMessage[4]);
          break;
        }
#else
        sendMessage[2] = 0x22;
        sendMessage[3] = ot_unit;
        ShortToBE(unit, &sendMessage[4]);
        ShortToBE(unit, &sendMessage[6]);
        int recvLen = OL2SendAndGetReply(6, 0x23, 7);
        if (recvLen >= 0
            && responseMessage[3] == ot_unit
            && BEToShort(&responseMessage[4]) == unit ) {
          Msg.snd_w2 = responseMessage[6];
          Msg.snd_w3 = BEToShort(&responseMessage[7]);
          break;
        }
#endif
        assert(recvLen == -1);	// NACK not handled yet
      }
      ReleaseLocks();
      Msg.snd_w1 = getRTC();
      break;
    }

    case OC_capros_HAI_getZoneStatus:
    {
      unsigned int zone = Msg.rcv_w1;
      GetLocks();
      while (1) {	// loop until successful
        // Using Protocol 3.0:
        sendMessage[2] = 0x3a;	// request extended object status
        sendMessage[3] = ot_zone;
        ShortToBE(zone, &sendMessage[4]);
        ShortToBE(zone, &sendMessage[6]);
        int recvLen = OL2SendAndGetReply(6, 0x3b, 7);
        if (recvLen >= 0
            && responseMessage[3] == ot_zone
            && responseMessage[4] >= 4	// length
            && BEToShort(&responseMessage[5]) == zone
            && (responseMessage[6] & capros_HAI_Status_Condition_Mask) != 3 ) {
          Msg.snd_w2 = responseMessage[6];
          Msg.snd_w3 = responseMessage[7];
          break;
        }
        assert(recvLen == -1);	// NACK not handled yet
      }
      ReleaseLocks();
      Msg.snd_w1 = getRTC();
      break;
    }

    case OC_capros_HAI_setUnitStatus:
    {
      unsigned int unit = Msg.rcv_w1;
      unsigned int cmd = Msg.rcv_w2;
      unsigned int param1 = Msg.rcv_w3;
      if (param1 > 255) {	// max value for any cmd
    invalid:
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      uint8_t haiCmd;
      uint8_t haiParam1;
      switch (cmd) {
      default:
        goto invalid;

      case capros_HAI_Command_UnitOff:
        haiCmd = 0;
        goto indefinitely;

      case capros_HAI_Command_UnitOn:
        haiCmd = 1;
      indefinitely:
        if (param1)
          goto invalid;
        haiParam1 = 0;
        break;

      case capros_HAI_Command_UnitOffForSeconds:
        haiCmd = 0;
        goto forSeconds;

      case capros_HAI_Command_UnitOnForSeconds:
        haiCmd = 1;
      forSeconds:
        if (param1 == 0 || param1 > 99)
          goto invalid;
        haiParam1 = param1;
        break;

      case capros_HAI_Command_UnitOffForMinutes:
        haiCmd = 0;
        goto forMinutes;

      case capros_HAI_Command_UnitOnForMinutes:
        haiCmd = 1;
      forMinutes:
        if (param1 == 0 || param1 > 99)
          goto invalid;
        haiParam1 = 100 + param1;
        break;

      case capros_HAI_Command_UnitOffForHours:
        haiCmd = 0;
        goto forHours;

      case capros_HAI_Command_UnitOnForHours:
        haiCmd = 1;
      forHours:
        if (param1 == 0 || param1 > 18)
          goto invalid;
        haiParam1 = 200 + param1;
        break;

      case capros_HAI_Command_UnitDecrement:
        haiCmd = 10;
        goto incrDecr;

      case capros_HAI_Command_UnitIncrement:
        haiCmd = 11;
      incrDecr:
        if (param1)
          goto invalid;
        goto useParam1;

      case capros_HAI_Command_UnitSet:
        haiCmd = 12;
      useParam1:
        haiParam1 = param1;
        break;
      }
      while (1) {	// loop until successful
        GetLocks();
#if (PROTOCOL == 1)
        sendMessage[2] = 0x0f;
#else
        sendMessage[2] = 0x14;
#endif
        sendMessage[3] = haiCmd;
        sendMessage[4] = haiParam1;
        ShortToBE(unit, &sendMessage[5]);
        int recvLen = OL2SendAndGetReply(5, mtype_Ack, 1);
        ReleaseLocks();
        if (recvLen >= 0) {
          break;
        }
        if (recvLen == -2) {	// NACK
          Msg.snd_code = RC_capros_HAI_NACK;
          break;
        }
      }
      break;
    }	// end of case OC_capros_HAI_setUnitStatus
    }	// end of switch(Msg.rcv_code)
  }
}
