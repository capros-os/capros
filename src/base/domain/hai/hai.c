/*
 * Copyright (C) 2008, Strawberry Development Group.
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
#include <idl/capros/HAI.h>
#include <idl/capros/IP.h>
#include <idl/capros/RTC.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>
#include <crypto/rijndael.h>

#define dbg_server 0x1
#define dbg_data   0x2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/* Protocol 1 is Omni-Link as described in "HAI Network Communication
     Protocol Description" 20P09-1 Rev A May 2003
     and "Omni-Link Serial Protocol Description" 10P17 Rev H September 2004.
   Protocol 2 is Omni-Link II as described in document 20P00 Rev 2.16
     June 2008. */
#define PROTOCOL 1

#define KR_OSTREAM KR_APP(0)
#define KR_RTC     KR_APP(1)
#define KR_CONFIG1 KR_APP(2)
#define KR_CONFIG2 KR_APP(3)
#define KR_IP      KR_APP(4)

#if (PROTOCOL == 1)
#define KR_UDPPort KR_APP(5)
#else
#define KR_TCPSocket KR_APP(4)
#endif

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

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
#else
#define startCharacter 0x21
#endif

#define ol2HeaderSize 4	// size of the Omni-Link II application header
#define maxSendMsgLen 128 //// verify this
#define maxRecvMsgLen 128 //// verify this
/* The + 16 below is because we encrypt block 1 into block 0, etc. */
#define sendBufSize (ol2HeaderSize + 16 + maxSendMsgLen)
#define recvBufSize (ol2HeaderSize + 16 + maxRecvMsgLen)
// Send and receive buffers, including the Omni-Link II application header.
uint8_t sendBuf[sendBufSize];
	// sendBuf[3] always remains zero
uint8_t recvBuf[recvBufSize];

// Where to put the unencrypted message to send:
#define sendMessage (sendBuf + ol2HeaderSize + 16)
// Where to get the unencrypted received message:
#define recvMessage (recvBuf + ol2HeaderSize + 16)

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

static unsigned int
GetSeqNo(void)
{
  return (recvBuf[0] << 8) | recvBuf[1];
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

// Create a port.
void
CreatePort(void)
{
  result_t result;

#if (PROTOCOL == 1)
  DEBUG(server) kprintf(KR_OSTREAM, "Creating UDP port.\n");
  result = capros_IP_createUDPPort(KR_IP, KR_UDPPort);
  assert(result == RC_OK);

  uint32_t maxReceiveSize, maxSendSize;
  result = capros_UDPPort_getMaxSizes(KR_UDPPort, HAIIpAddr,
			&maxReceiveSize, &maxSendSize);
  assert(result == RC_OK);
  DEBUG(server) kprintf(KR_OSTREAM, "Max size rcv %d snd %d\n", maxReceiveSize, maxSendSize);
  // We had better be able to send a message in a single packet:
  assert(maxSendSize >= maxSendMsgLen + ol2HeaderSize);
#else	// TCP
  DEBUG(server) kprintf(KR_OSTREAM, "Connecting using TCP.\n");

  result = capros_IP_connect(KR_IP, HAIIpAddr, HAIPort,
			     KR_TCPSocket);
  switch (result) {
  default:
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result);
    break;
  case RC_capros_IPDefs_Aborted:
    kdprintf(KR_OSTREAM, "Connection aborted.\n");
    break;
  case RC_capros_IPDefs_Refused:
    kdprintf(KR_OSTREAM, "Connection refused.\n");
    break;
  case RC_OK:
    break;
  }

  DEBUG(server) kprintf(KR_OSTREAM, "Connected.\n");
#endif
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
  result = capros_UDPPort_send(KR_UDPPort, HAIIpAddr, HAIPort,
             len, sendBuf);
#else // TCP
  result = capros_TCPSocket_send(KR_TCPSocket, len,
                                 capros_TCPSocket_flagPush, sendBuf);
#endif
  switch (result) {
  default: ;
    assert(false);

  case RC_capros_TCPSocket_Already:	// we closed it?
  case RC_capros_key_Void:	// very closed, or a system restart
    return false;

  case RC_OK:
    return true;
  };
}

/* Receive a UDP packet into recvBuf.
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
  // FIXME: need a timeout for this
  result = capros_UDPPort_receive(KR_UDPPort, sizeof(recvBuf),
			&sourceIPAddr, &sourceIPPort,
                        &lenRecvd, recvBuf);
  assert(result == RC_OK);
  DEBUG(server) kprintf(KR_OSTREAM, "Received %d bytes from %#x:%d:\n",
          lenRecvd, sourceIPAddr, sourceIPPort);
#else // TCP
  uint8_t flagsRecvd;
  // FIXME: need a timeout for this
  result = capros_TCPSocket_receive(KR_TCPSocket, sizeof(recvBuf),
                                    &lenRecvd, &flagsRecvd, recvBuf);
  assert(result == RC_OK);
  DEBUG(server) kprintf(KR_OSTREAM, "Received %d bytes\n", lenRecvd);
#endif

  DEBUG(data) {
    int i;
    for (i = 0; i < lenRecvd; i++) {
      kprintf(KR_OSTREAM, " %#x", recvBuf[i]);
    }
    kprintf(KR_OSTREAM, "\n");
  }
  return lenRecvd;
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

  DEBUG(data) {
    kprintf(KR_OSTREAM, "Encrypted");
    int i;
    for (i = 0; i < ol2HeaderSize+totalLen; i++) {
      kprintf(KR_OSTREAM, " %#x", sendBuf[i]);
    }
    kprintf(KR_OSTREAM, "\n");
  }
  return NetSend(ol2HeaderSize + totalLen);
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
  if (seqNo != sequenceNumber) {
    DEBUG(server) kprintf(KR_OSTREAM, "Expecting seq no %d got %d, discarding\n",
            sequenceNumber, seqNo);
    goto retry;	// restructure this
  }
  assert(!(len & 0xf));		// should be a multiple of 16

  // Decrypt all blocks, moving them up to recvMessage:
  uint8_t * c = recvBuf + ol2HeaderSize + len;
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
      kprintf(KR_OSTREAM, " %#x", recvMessage[i]);
    }
    kprintf(KR_OSTREAM, "\n");
  }
  return len;
}

// Create a session.
void
CreateSession(void)
{
  while (1) {	// loop until successful
    DEBUG(server) kprintf(KR_OSTREAM, "Requesting session.\n");
    IncSeqNo();
    SetSeqNo(sequenceNumber);
    sendBuf[2] = ol2mtyp_requestNewSess;
    if (NetSend(ol2HeaderSize)) {
      int lenRecvd = NetReceive();
      if (lenRecvd >= 0) {
        assert(lenRecvd == ol2HeaderSize + 7);
        assert(GetSeqNo() == sequenceNumber);
        assert(recvBuf[2] == ol2mtyp_ackNewSess);
        assert(BEToShort(&recvBuf[ol2HeaderSize]) == 1); // protocol version 1
        memcpy(sessionID, &recvBuf[ol2HeaderSize + 2], 5);
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
          lenRecvd = EncryptedReceive();
          if (lenRecvd >= 0) {
            assert(lenRecvd >= 5);
            assert(recvBuf[2] == ol2mtyp_ackSecConn);
            assert(! memcmp(recvMessage, sessionID, 5));
            return;
          }
        }
      }
    }
    // The port is gone.
    CreatePort();
  }
}

/* Send an Omni-Link II application data message. 
 * The message is in sendMessage ff., except for the first two
 * bytes and the CRC, which this procedure fills in.
 * len is the value for the second byte of the message. */
/* If the port capability is void, this procedure creates a new session
 * and returns false, but does not retry the send.
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

  if (EncryptedSend(len + 4))
    return true;

  // The port is gone.
  CreateSession();
  return false;
}

/* If the session was terminated, this procedure creates a new session
 * and returns -1, but does not retry the operation.
 * The send may or may not have occurred.
 * Otherwise it returns the length of the data plus 1 for the message type
 * (the length excludes the start character, length byte, and CRC). */
int
OL2SendAndGetReply(unsigned int sendLen, unsigned int replyType,
                   unsigned int minReplyLen)
{
  if (! OL2Send(sendLen))
    return -1;
  int receiveLen = EncryptedReceive();
  if (receiveLen < 0) {
    // The port is gone.
    CreateSession();
    return receiveLen;
  }
  receiveLen -= 4;
  assert(receiveLen >= 4);
  switch (recvBuf[2]) {		// message type
  default: ;
    assert(false);

  case ol2mtyp_contrSessTerm:	// the session is gone
    CreateSession();
    return -1;

  case ol2mtyp_dataMsg:
    assert(recvMessage[0] == startCharacter);
    uint16_t CRC = CalcCRC(&recvMessage[1], receiveLen+1);
    assert(recvMessage[2 + receiveLen] == (CRC & 0xff));
    assert(recvMessage[2 + receiveLen + 1] == (CRC >> 8));

    // Check the expected reply:
    assert(recvMessage[2] == replyType);
    assert(receiveLen >= minReplyLen);
    
    return receiveLen;
  }
}

void
SimpleRequest(uint8_t typeCode,
              unsigned int replyType, unsigned int minReplyLen)
{
  while(1) {	// loop until successful
    sendMessage[2] = typeCode;
    int recvLen = OL2SendAndGetReply(1, replyType, minReplyLen);
    if (recvLen >= 0) {
      return;
    }
  }
}

/**************************** main server loop *************************/

int
main(void)
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

  // Needed to ensure crypto is set up:
  CreateSession();

  for(;;) {
    RETURN(&Msg);

    DEBUG(server) kprintf(KR_OSTREAM, "hai was called, OC=%#x\n",
                          Msg.rcv_code);

    // Defaults for reply:
    Msg.snd_invKey = KR_RETURN;
    Msg.snd_code = RC_OK;
    Msg.snd_w1 = 0;
    Msg.snd_w2 = 0;
    Msg.snd_w3 = 0;

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_HAI;
      break;

    case OC_capros_HAI_getSystemStatus:
    {
      SimpleRequest(0x13 /* 0x18 */, 0x14, 15+1);
      Msg.snd_w1 = getRTC();
      Msg.snd_data = &recvMessage[3];
      Msg.snd_len = sizeof(capros_HAI_SystemStatus);
      break;
    }

    case OC_capros_HAI_getUnitStatus:
    {
      unsigned int unit = Msg.rcv_w1;
      while (1) {	// loop until successful
        sendMessage[2] = 0x17;
        ShortToBE(unit, &sendMessage[3]);
        ShortToBE(unit, &sendMessage[5]);
        int recvLen = OL2SendAndGetReply(5, 0x18, 4);
        if (recvLen >= 0) {
          break;
        }
      }
      Msg.snd_w1 = getRTC();
      Msg.snd_w2 = recvMessage[3];
      Msg.snd_w3 = BEToShort(&recvMessage[4]);
      break;
    }

    case OC_capros_HAI_setUnitStatus:
    {
      unsigned int unit = Msg.rcv_w1;
      unsigned int cmd = Msg.rcv_w2;
      unsigned int param1 = Msg.rcv_w3;
      if (param1 > 99) {	// max value for any cmd
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
        if (param1 == 0)
          goto invalid;
        haiParam1 = param1;
        break;

      case capros_HAI_Command_UnitOffForMinutes:
        haiCmd = 0;
        goto forMinutes;

      case capros_HAI_Command_UnitOnForMinutes:
        haiCmd = 1;
      forMinutes:
        if (param1 == 0)
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
        sendMessage[2] = 0x0f;
        sendMessage[3] = haiCmd;
        sendMessage[4] = haiParam1;
        ShortToBE(unit, &sendMessage[5]);
        int recvLen = OL2SendAndGetReply(5, mtype_Ack, 1);
        if (recvLen >= 0) {
          break;
        }
      }
      break;
    }
    }
  }
}
