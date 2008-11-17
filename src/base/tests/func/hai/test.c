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

/* USB test.
*/

#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/DevPrivs.h>
#include <idl/capros/IP.h>
#include <idl/capros/NPLinkee.h>

#include <linux/usb/ch9.h>

#include <domain/Runtime.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <crypto/rijndael.h>

//#define UDP	// not TCP

#define KR_IP   KR_APP(0)
#define KR_OSTREAM KR_APP(1)
#define KR_SLEEP   KR_APP(2)
#define KR_DEVPRIVS KR_APP(3)
#ifdef UDP
#define KR_UDPPort KR_APP(4)
#else
#define KR_TCPSocket KR_APP(4)
#endif

#define dmaVirtAddr 0x1d000

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

#define fourByteVal(a,b,c,d) (((((uint32_t)a)*256+(b))*256+(c))*256+(d))

#define keybits 128
#define testIPAddr fourByteVal(192,168,0,101)	// HAI Omni Pro
#define testIPPort 4369

unsigned long rkEncrypt[RKLENGTH(keybits)];
unsigned long rkDecrypt[RKLENGTH(keybits)];
int nrounds;

#if 1
const uint8_t privateKey[16] = {
  0x1c, 0x30, 0x90, 0x0c, 0x50, 0xdc, 0x1e, 0x7a,
  0x9d, 0xd1, 0x24, 0x81, 0xe9, 0x2d, 0x70, 0xb1
};
#else	// see if reversed works////
const uint8_t privateKey[16] = {
  0xb1, 0x70, 0x2d, 0xe9, 0x81, 0x24, 0xd1, 0x9d,
  0x7a, 0x1e, 0xdc, 0x50, 0x0c, 0x90, 0x30, 0x1c
};
#endif

const uint8_t protocolVersion[] = { 0x00, 0x01 };
uint8_t sessionKey[16];
uint8_t sessionID[5];

enum {
  ol2mtyp_requestNewSess = 1,
  ol2mtyp_ackNewSess,
  ol2mtyp_reqSecConn,
  ol2mtyp_ackSecConn,
  ol2mtyp_clientSessTerm,
  ol2mtyp_contrSessTerm,
  ol2mtyp_rejectNewSess,
  ol2mtyp_dataMsg = 32 //// 16 // not 32 as stated in the doc!
};

#define ol2HeaderSize 4	// size of the Omni-Link II application header
#define maxSendMsgLen 128 ////
#define maxRecvMsgLen 128 ////
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

// Send data that already has an Omni-Link II application header
// and already has any necessary encryption.
// The data is in sendBuf. len includes the Omni-Link II application header.
void
NetSend(unsigned int len)
{
  result_t result;
#ifdef UDP
  result = capros_UDPPort_send(KR_UDPPort, testIPAddr, testIPPort,
             len, sendBuf);
#else // TCP not UDP
  result = capros_TCPSocket_send(KR_TCPSocket, len,
                                 capros_TCPSocket_flagPush, sendBuf);
#endif
  ckOK
}

/* Receive a UDP packet into recvBuf.
 * Returns the number of bytes received. */
unsigned int
NetReceive(void)
{
  result_t result;
  uint32_t lenRecvd;
#ifdef UDP
  uint32_t sourceIPAddr;
  uint16_t sourceIPPort;
  // FIXME: need a timeout for this
  result = capros_UDPPort_receive(KR_UDPPort, sizeof(recvBuf),
			&sourceIPAddr, &sourceIPPort,
                        &lenRecvd, recvBuf);
  ckOK
  kprintf(KR_OSTREAM, "Received %d bytes from %#x:%d:\n",
          lenRecvd, sourceIPAddr, sourceIPPort);
#else // TCP not UDP
  uint8_t flagsRecvd;
  // FIXME: need a timeout for this
  result = capros_TCPSocket_receive(KR_TCPSocket, sizeof(recvBuf),
                                    &lenRecvd, &flagsRecvd, recvBuf);
  ckOK
  kprintf(KR_OSTREAM, "Received %d bytes ", lenRecvd);
#endif

  int i;
  for (i = 0; i < lenRecvd; i++) {
    kprintf(KR_OSTREAM, " %#x", recvBuf[i]);
  }
  kprintf(KR_OSTREAM, "\n");
  return lenRecvd;
}

/* sendMessage has a packet of length totalLen.
 * sendBuf[2] has the message type of the packet.
 * Send the packet. */
static void
EncryptedSend(unsigned int totalLen)
{
  // Set up sequence number:
  IncSeqNo();
  unsigned int seqNo = sequenceNumber;	// local copy
  SetSeqNo(seqNo);

#if 1
  kprintf(KR_OSTREAM, "EncryptedSend len=%d seq %d type %d",
          totalLen, seqNo, sendBuf[2]);
  int i;
  for (i = 0; i < totalLen; i++) {
    kprintf(KR_OSTREAM, " %#x", sendMessage[i]);
  }
  kprintf(KR_OSTREAM, "\n");
#endif

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

#if 1
  kprintf(KR_OSTREAM, "Encrypted");
  for (i = 0; i < ol2HeaderSize+totalLen; i++) {
    kprintf(KR_OSTREAM, " %#x", sendBuf[i]);
  }
  kprintf(KR_OSTREAM, "\n");
#endif
  NetSend(ol2HeaderSize + totalLen);
}

/* Receive and decrypt a packet.
 * Returns the length excluding the header. */
static unsigned int
EncryptedReceive(void)
{
#ifndef REPLAY
retry: ;
  unsigned int len = NetReceive();
  assert(len >= ol2HeaderSize);
  len -= ol2HeaderSize;		// length of payload
  unsigned int seqNo = GetSeqNo();
  if (seqNo != sequenceNumber) {
    kprintf(KR_OSTREAM, "Expecting seq no %d got %d, discarding\n",
            sequenceNumber, seqNo);
    goto retry;	// restructure this
  }
  assert(!(len & 0xf));		// should be a multiple of 16
#else
static const uint8_t d[] = {
//0xed,0xf2,0xc0,0xb9,0x81,0x7f,0x25,0x5c,
//0x91,0x05,0xf7,0x8c,0x47,0x46,0xd7,0xc1
//0x33,0xd1,0xaf,0x3a,0xab,0x8e,0x5c,0xe5,
//0x23,0x58,0xd1,0x6d,0x8a,0x08,0x05,0x8c
0xa6,0x01,0x5c,0xe8,0x36,0x6f,0x23,0xf0,
0xb4,0xd1,0xd7,0x38,0xee,0x97,0x64,0x7e
};
  unsigned int len = 16;
  unsigned int seqNo = 0xdc6e;
  memcpy(recvBuf+ol2HeaderSize, d, 16);
#endif

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
#if 1
  kprintf(KR_OSTREAM, "Decrypted payload");
  int i;
  for (i = 0; i < len; i++) {
    kprintf(KR_OSTREAM, " %#x", recvMessage[i]);
  }
  kprintf(KR_OSTREAM, "\n");
#endif
  return len;
}

/* Send an Omni-Link II application data message. 
 * The message is in sendMessage ff., except for the first two
 * bytes and the CRC, which this procedure fills in.
 * len is the value for the second byte of the message. */
void
OL2Send(unsigned int len)
{
  sendMessage[0] = 0x21;
  sendMessage[1] = len;

  uint16_t CRC = CalcCRC(&sendMessage[1], len+1);
  sendMessage[2 + len] = CRC & 0xff;
  sendMessage[2 + len + 1] = CRC >> 8;

  sendBuf[2] = ol2mtyp_dataMsg;

  EncryptedSend(len + 4);
}

/* Returns length of the data plus 1 for the message type. */
unsigned int
OL2SendAndGetReply(unsigned int sendLen)
{
  OL2Send(sendLen);
  unsigned int receiveLen = EncryptedReceive();
  receiveLen -= 4;
  assert(recvBuf[2] == ol2mtyp_dataMsg);
  assert(recvMessage[0] == 0x21);
  uint16_t CRC = CalcCRC(&recvMessage[1], receiveLen+1);
  assert(recvMessage[2 + receiveLen] == (CRC & 0xff));
  assert(recvMessage[2 + receiveLen + 1] == (CRC >> 8));
  
  return receiveLen;
}

int
main(void)
{
  result_t result;
  capros_key_type theType;
  //unsigned long err;

  kprintf(KR_OSTREAM, "Starting.\n");

  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_code = RC_OK,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .rcv_key0 = KR_IP,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_limit = 0,
  };

  RETURN(&Msg);
  assert(Msg.rcv_code == OC_capros_NPLinkee_registerNPCap);
  // Reply to NPLink:
  Msg.snd_invKey = KR_RETURN;
  SEND(&Msg);

  result = capros_key_getType(KR_IP, &theType);
  ckOK
  assert(theType == IKT_capros_IP);

#if 1 ////
#ifdef UDP
  kprintf(KR_OSTREAM, "Creating UDP port.\n");
  result = capros_IP_createUDPPort(KR_IP, capros_IP_LocalPortAny,
                                   KR_UDPPort);
  ckOK

  result = capros_key_getType(KR_UDPPort, &theType);
  ckOK
  assert(theType == IKT_capros_UDPPort);

  uint32_t maxReceiveSize, maxSendSize;
  result = capros_UDPPort_getMaxSizes(KR_UDPPort, testIPAddr,
			&maxReceiveSize, &maxSendSize);
  ckOK
  kprintf(KR_OSTREAM, "Max size rcv %d snd %d\n", maxReceiveSize, maxSendSize);
#else	// TCP not UDP
  kprintf(KR_OSTREAM, "Connecting using TCP.\n");

  result = capros_IP_connect(KR_IP, testIPAddr, testIPPort,
			KR_TCPSocket);
  switch (result) {
  default:
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result);
    break;
  case RC_capros_IP_Aborted:
    kdprintf(KR_OSTREAM, "Connection aborted.\n");
    break;
  case RC_capros_IP_Refused:
    kdprintf(KR_OSTREAM, "Connection refused.\n");
    break;
  case RC_OK:
    break;
  }

  result = capros_key_getType(KR_TCPSocket, &theType);
  ckOK
  assert(theType == IKT_capros_TCPSocket);

  kprintf(KR_OSTREAM, "Connected.\n");
#endif

#ifndef REPLAY
  kprintf(KR_OSTREAM, "Requesting session.\n");
  IncSeqNo();
  SetSeqNo(sequenceNumber);
  sendBuf[2] = ol2mtyp_requestNewSess;
  NetSend(ol2HeaderSize);

  uint32_t lenRecvd = NetReceive();
  assert(lenRecvd == ol2HeaderSize + 7);
  assert(GetSeqNo() == sequenceNumber);
  assert(recvBuf[2] == ol2mtyp_ackNewSess);
  assert(! memcmp(&recvBuf[ol2HeaderSize],
                  protocolVersion, sizeof(protocolVersion)));
  memcpy(sessionID, &recvBuf[ol2HeaderSize + 2], 5);
#else
uint8_t sessID[5] = {0x5f, 0xce, 0x99, 0x1b, 0x24};
memcpy(sessionID, sessID,5);
#endif
  memcpy(sessionKey, privateKey, 16);
  int i;
  for (i = 0; i < 5; i++) {
    sessionKey[11+i] ^= sessionID[i];
  }

  // Set up crypto:
  nrounds = rijndaelSetupEncrypt(rkEncrypt, sessionKey, keybits);
  nrounds = rijndaelSetupDecrypt(rkDecrypt, sessionKey, keybits);

#ifndef REPLAY
  kprintf(KR_OSTREAM, "Requesting secure session.\n");
  // This seems to have no purpose other than to verify the connection.
  sendBuf[2] = ol2mtyp_reqSecConn;
  memcpy(sendMessage, sessionID, 5);
  EncryptedSend(5);
  lenRecvd = EncryptedReceive();
  assert(lenRecvd >= 5);
  assert(recvBuf[2] == ol2mtyp_ackSecConn);
  assert(! memcmp(recvMessage, sessionID, 5));

#if 1
  // Request system info

  unsigned int recvLen;
  sendMessage[2] = 0x18;
  OL2Send(1);

  result = capros_Sleep_sleep(KR_SLEEP, 5000);

  sendMessage[2] = 0x18;
  recvLen = OL2SendAndGetReply(1);
#endif

  kprintf(KR_OSTREAM, "Closing session.\n");
  sendBuf[2] = ol2mtyp_clientSessTerm;
  EncryptedSend(0);	// "encrypted" only to get the sequence number logic
  lenRecvd = EncryptedReceive();
  assert(lenRecvd == 0);
  assert(recvBuf[2] == ol2mtyp_contrSessTerm);
#else	// REPLAY
  EncryptedReceive();
  (void) GetSeqNo();
#endif

#ifdef UDP
#else
  result = capros_TCPSocket_close(KR_TCPSocket);
  ckOK
#endif
#endif ////

  kprintf(KR_OSTREAM, "\nDone.\n");

  return 0;
}

