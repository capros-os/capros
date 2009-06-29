/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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
#define KR_LOGFILERO KR_CMTE(7)
//#define RESPONSE_TEST
#ifdef RESPONSE_TEST
#define KR_SysTrace KR_CMTE(8)	// also initialize this in hai.map
#endif

#define LKSN_Socket LKSN_CMTE

typedef capros_RTC_time_t RTC_time;		// real time, seconds

uint32_t HAIIpAddr, HAIPort;	// IP address and port of HAI

RTC_time
getRTC(void)
{
  RTC_time t;
  result_t result = capros_RTC_getTime(KR_RTC, &t);
  assert(result == RC_OK);
  return t;
}

/************************ crypto stuff ******************************/

#define keybits 128
unsigned long rkEncrypt[RKLENGTH(keybits)];
unsigned long rkDecrypt[RKLENGTH(keybits)];
int nrounds;
uint8_t privateKey[16];
uint8_t sessionKey[16];
uint8_t sessionID[5];

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
#define recvBufSize (ol2HeaderSize + 16 + maxRecvMsgLen)
// Send and receive buffers, including the Omni-Link II application header.
uint8_t sendBuf[sendBufSize];
	// sendBuf[3] always remains zero
// Where to put the unencrypted message to send:
#define sendMessage (sendBuf + ol2HeaderSize + 16)

uint8_t recvrBuf[recvBufSize];
// Where to get the unencrypted received message:
#define recvrMessage (recvrBuf + ol2HeaderSize + 16)

/************************ synchronization stuff ******************************/

CMTEMutex_DECLARE_Locked(lock);	// initially locked by Receiver thread

CMTESemaphore_DECLARE(mainWait, 0);

enum {
  mstate_Available,
  mstate_NeedSession,
  mstate_NeedResponse
} mainState = mstate_Available;

// When we are awakened from mstate_NeedResponse, we get back:
int responseMessageLength;
uint8_t responseMessage[maxRecvMsgLen];

// Called by main thread.
// Called holding the lock; releases the lock; gets the lock before returning.
void
WaitForSession(void)
{
  mainState = mstate_NeedSession;
  CMTEMutex_unlock(&lock);
  CMTESemaphore_down(&mainWait);
  CMTEMutex_lock(&lock);
}

void
TimerFunction(unsigned long data)
{
  result_t result;

  DEBUG(errors) kprintf(KR_OSTREAM, "HAI receive timed out.\n");
  /* Destroy the port/socket. This will abort any connection and
  wake up the receiver with an exception. */
  result = capros_Node_getSlotExtended(KR_KEYSTORE, LKSN_Socket, KR_TEMP0);
  assert(result == RC_OK);
  capros_key_destroy(KR_TEMP0);
}

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

unsigned int sequenceNumber = 0;	// incremented to 1 before first use

static void
SetSeqNo(uint16_t seqNo)
{
  sendBuf[0] = seqNo >> 8;
  sendBuf[1] = seqNo & 0xff;
}

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

// Send data that already has an Omni-Link II application header
// and already has any necessary encryption.
// The data is in sendBuf. len includes the Omni-Link II application header.
// Returns false iff the port capability is void.
bool
NetSend(unsigned int len)
{
  result_t result;
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

  case RC_capros_TCPSocket_Already:	// we closed it?
  case RC_capros_key_Void:	// very closed, or a system restart
    return false;

  case RC_OK:
    return true;
  };
}

/* sendMessage has a packet of length totalLen.
 * sendBuf[2] has the message type of the packet.
 * Send the packet. */
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

static unsigned int
GetSeqNo(void)
{
  return (recvrBuf[0] << 8) | recvrBuf[1];
}

/* Receive a packet into recvrBuf.
 * If the port capability is void, returns -1,
 * otherwise returns the number of bytes received. */
int
NetReceive(void)
{
  result_t result;
  uint32_t lenRecvd;

#if (PROTOCOL == 1)
  uint32_t sourceIPAddr;
  uint16_t sourceIPPort;
  result = capros_UDPPort_receive(KR_Socket, sizeof(recvrBuf),
			&sourceIPAddr, &sourceIPPort,
                        &lenRecvd, recvrBuf);
  if (result != RC_OK)
    return -1;
  DEBUG(server) kprintf(KR_OSTREAM, "Received %d bytes from %#x:%d:\n",
          lenRecvd, sourceIPAddr, sourceIPPort);
#else // TCP
  uint8_t flagsRecvd;
  result = capros_TCPSocket_receive(KR_Socket, sizeof(recvrBuf),
                                    &lenRecvd, &flagsRecvd, recvrBuf);
  switch (result) {
  default:
    assert(false);
  case RC_capros_TCPSocket_RemoteClosed:
    capros_key_destroy(KR_Socket);
  case RC_capros_key_Void:
    return -1;
  case RC_OK:
    break;
  }
  DEBUG(server) kprintf(KR_OSTREAM, "Received %d bytes\n", lenRecvd);
#endif

  DEBUG(data) {
    int i;
    for (i = 0; i < lenRecvd; i++) {
      kprintf(KR_OSTREAM, " %#x", recvrBuf[i]);
    }
    kprintf(KR_OSTREAM, "\n");
  }
  return lenRecvd;
}

/* Receive and decrypt a packet.
 * If the port capability is void, returns -1,
 * otherwise returns the number of bytes received excluding the header. */
static int
EncryptedReceive(void)
{
retry: ;
  int len = NetReceive();
  if (len < 0)
    return len;
  assert(len >= ol2HeaderSize);
  len -= ol2HeaderSize;		// length of payload
  unsigned int seqNo = GetSeqNo();
  if (seqNo != sequenceNumber && seqNo != 0) {
    DEBUG(server) kprintf(KR_OSTREAM, "Expecting seq no %d got %d, discarding\n",
            sequenceNumber, seqNo);
    goto retry;	// restructure this
  }
  assert(!(len & 0xf));		// should be a multiple of 16

  // Decrypt all blocks, moving them up to recvrMessage:
  uint8_t * c = recvrBuf + ol2HeaderSize + len;
  uint8_t * p;
  unsigned int blocks;
  for (blocks = len / 16; blocks > 0; blocks--) {
    p = c;
    c -= 16;
    rijndaelDecrypt(rkDecrypt, nrounds, c, p);
    *p ^= seqNo >> 8;
    *(p+1) ^= seqNo & 0xff; 
  }
  DEBUG(data) {
    kprintf(KR_OSTREAM, "Decrypted payload");
    int i;
    for (i = 0; i < len; i++) {
      kprintf(KR_OSTREAM, " %#x", recvrMessage[i]);
    }
    kprintf(KR_OSTREAM, "\n");
  }
  return len;
}

/* If the session was terminated, this procedure returns -1.
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
  int receiveLen = EncryptedReceive();
  if (receiveLen < 0) {
    return -1;
  }
  receiveLen -= 4;	// length without the start char, length byte, and CRC
  assert(receiveLen >= 1);	// must have at least a type byte
  switch (recvrBuf[2]) {		// message type in packet header
  default: ;
    assert(false);

  case ol2mtyp_contrSessTerm:	// the session is gone
    DEBUG(rcvr) kdprintf(KR_OSTREAM, "HAI terminated session.\n");
    return -1;

  case ol2mtyp_dataMsg:
    assert(recvrMessage[0] == startCharacter);
    uint16_t CRC = CalcCRC(&recvrMessage[1], receiveLen+1);
    assert(recvrMessage[2 + receiveLen] == (CRC & 0xff));
    assert(recvrMessage[2 + receiveLen + 1] == (CRC >> 8));
    return receiveLen;
  }
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
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result);
  case RC_OK:
    break;
  case RC_capros_key_Restart:
    goto reconnect;
  case RC_capros_IPDefs_Aborted:
    kdprintf(KR_OSTREAM, "Connection aborted.\n");
    break;
  case RC_capros_IPDefs_Refused:
    kdprintf(KR_OSTREAM, "Connection refused.\n");
    break;
  }

  DEBUG(server) kprintf(KR_OSTREAM, "Connected.\n");
#endif

  // Save it where the other threads can get it:
  result = capros_Node_swapSlotExtended(KR_KEYSTORE, LKSN_Socket, KR_Socket,
             KR_VOID);
  assert(result == RC_OK);
}

unsigned int ReceiverThreadNum;
void *
ReceiverThread(void * arg)
{
  result_t result;
  int recvLen;

  // We hold the lock initially.
  while (1) {
    // There is no session.
    if (mainState == mstate_NeedResponse) {
      responseMessageLength = -1;
      mainState = mstate_Available;	// no longer needing a response
      CMTESemaphore_up(&mainWait);
    }
    // Create a session.
    while (1) {	// loop until successful
      DEBUG(rcvr) kprintf(KR_OSTREAM, "Requesting session.\n");
      IncSeqNo();
      SetSeqNo(sequenceNumber);
      sendBuf[2] = ol2mtyp_requestNewSess;
      if (NetSend(ol2HeaderSize)) {
        CMTETimer_Define(tmr2, &TimerFunction, 0);

        // Set a timeout for the response.
        CMTETimer_setDuration(&tmr2, 3000000000ULL);	// 3 seconds
        CMTETimer_delete(&tmr2);
        int lenRecvd = NetReceive();
        if (lenRecvd >= 0) {
          assert(lenRecvd == ol2HeaderSize + 7);
          assert(GetSeqNo() == sequenceNumber);
          assert(recvrBuf[2] == ol2mtyp_ackNewSess);
          assert(BEToShort(&recvrBuf[ol2HeaderSize]) == 1); // protocol version 1
          memcpy(sessionID, &recvrBuf[ol2HeaderSize + 2], 5);
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
          if (EncryptedSend(5)) {
            // Set a timeout for the response.
            CMTETimer_setDuration(&tmr2, 3000000000ULL);	// 3 seconds
            lenRecvd = EncryptedReceive();
            CMTETimer_delete(&tmr2);
            if (lenRecvd >= 0) {
              assert(lenRecvd >= 5);
              assert(recvrBuf[2] == ol2mtyp_ackSecConn);
              assert(! memcmp(recvrMessage, sessionID, 5));
////#if (PROTOCOL == 1)
#if 1	//// Firmware v. 2.16a has bugs in notifications.
// We could read the firmware version and check. But really,
// the rest of the system is depending on notifications. 
// All we could do is complain.
              break;
#else
              // Enable notifications.
#if 0//// delay
              RTC_time rt = getRTC() + 3;	// delay 3 seconds
              while (getRTC() < rt) ;
#endif
              sendMessage[2] = 0x15;
              sendMessage[3] = 1;
              if (OL2Send(2)) {
                // Set a timeout for the response.
                CMTETimer_setDuration(&tmr2, 3000000000ULL);	// 3 seconds
                recvLen = OL2Receive();
                CMTETimer_delete(&tmr2);
                if (recvLen >= 1 && recvrMessage[2] == mtype_Ack)
                  break;
                else
                  DEBUG(errors) kprintf(KR_OSTREAM,
                    "HAI respose to enable notif was msg type %#x.\n",
                    recvrMessage[2] );
              }
#endif
            }
          }
        }
      }
      // The port is gone, or we are creating a new session due to an error.
      CreatePort();
    }
    // We now have a session.
    DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI got session.\n");
    // Push it to the main thread.
    result = capros_Node_getSlotExtended(KR_KEYSTORE,
               LKSN_THREAD_PROCESS_KEYS+0, KR_TEMP0);
    assert(result == RC_OK);
    result = capros_Process_swapKeyReg(KR_TEMP0, KR_Socket, KR_Socket, KR_VOID);
    assert(result == RC_OK);
    // Wake him up if waiting:
    if (mainState == mstate_NeedSession) {
      mainState = mstate_Available;	// no longer needing a session
      CMTESemaphore_up(&mainWait);
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr woke main for session.\n");
    }

    while (1) {		// loop reading from the session
      CMTEMutex_unlock(&lock);
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr receiving.\n");
      recvLen = OL2Receive();
      DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr received.\n");
      CMTEMutex_lock(&lock);
      if (recvLen < 0)
        break;		// session is gone
      if (GetSeqNo() == 0) {
        unsigned int msgType = recvrMessage[2];
        // It is a notification.
        //if (msgType == 0x23 || msgType == 0x37)
        kprintf(KR_OSTREAM, "++++HAI notif %#x\n", msgType);////
////...
      } else {
        // It is a response.
        if (mainState == mstate_NeedResponse) {
          responseMessageLength = recvLen;
          memcpy(responseMessage, recvrMessage, recvLen);
          mainState = mstate_Available;	// no longer needing a response
          CMTESemaphore_up(&mainWait);
          DEBUG(rcvr) kprintf(KR_OSTREAM, "HAI rcvr woke main for response.\n");
        }
      }
    }
  }
}

/************************* procedures for main thread ************************/

/* If the session was terminated, this procedure waits for a new session
 *   and returns -1, but does not retry the operation.
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
  // Wait for the response.
  mainState = mstate_NeedResponse;
  CMTEMutex_unlock(&lock);
  // Set a timeout for the response.
  CMTETimer_Define(tmr, &TimerFunction, 0);
  CMTETimer_setDuration(&tmr, 3000000000ULL);	// set timer for 3 seconds
  CMTESemaphore_down(&mainWait);
  CMTETimer_delete(&tmr);
  CMTEMutex_lock(&lock);
  if (responseMessageLength < 0) {
    // The port is gone.
    WaitForSession();
    return -1;
  }
  // Check the expected reply:
  if (responseMessage[2] == replyType) {
    assert(responseMessageLength >= minReplyLen);	// FIXME depends on HAI
    return responseMessageLength;
  }
  assert(responseMessage[2] == mtype_NAck);	// FIXME depends on HAI
  DEBUG(errors) kprintf(KR_OSTREAM, "HAI got NACK\n");
  return -2;
}

static void
SimpleRequest(uint8_t typeCode,
              unsigned int replyType, unsigned int minReplyLen)
{
  while(1) {	// loop until successful
    sendMessage[2] = typeCode;
    int recvLen = OL2SendAndGetReply(1, replyType, minReplyLen);
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
  // Save data 32 days:
  result = capros_Logfile_setDeletionPolicyByID(KR_LOGFILE,
             32*24*60*60*1000000000ULL);
  assert(result == RC_OK);
  // Create read-only cap once now, since there is no shortage of cap regs.
  result = capros_Logfile_getReadOnlyCap(KR_LOGFILE, KR_LOGFILERO);
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

    CMTEMutex_lock(&lock);
    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_HAI;
      break;

    case OC_capros_HAI_getSystemStatus:
    {
#if (PROTOCOL == 1)
      SimpleRequest(0x13, 0x14, 15+1);
#else
      SimpleRequest(0x18, 0x19, 15+1);
#endif
      Msg.snd_w1 = getRTC();
      Msg.snd_data = &responseMessage[3];
      Msg.snd_len = sizeof(capros_HAI_SystemStatus);
      break;
    }

    case OC_capros_HAI_getUnitStatus:
    {
      unsigned int unit = Msg.rcv_w1;
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
#if (PROTOCOL == 1)
        sendMessage[2] = 0x0f;
#else
        sendMessage[2] = 0x14;
#endif
        sendMessage[3] = haiCmd;
        sendMessage[4] = haiParam1;
        ShortToBE(unit, &sendMessage[5]);
        int recvLen = OL2SendAndGetReply(5, mtype_Ack, 1);
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
    assert(mainState == mstate_Available);
    CMTEMutex_unlock(&lock);
  }
}
