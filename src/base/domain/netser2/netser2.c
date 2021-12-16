/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
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

/* netser - driver for a network-to-serial adapter, specifically the
 * Netburner PK70EX-232. */

#include <stdlib.h>
#include <string.h>
#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <domain/CMTEThread.h>
#include <domain/CMTESemaphore.h>
#include <domain/CMTETimer.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Process.h>
#include <idl/capros/Number.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/SerialPort.h>
#include <idl/capros/IP.h>
#include <idl/capros/NPLink.h>
#include <idl/capros/TCPSocket.h>
#include <disk/NPODescr.h>
#include <domain/assert.h>

#define dbg_read  0x1
#define dbg_write 0x2
#define dbg_conn  0x4

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0x0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)


#define KC_VOLSIZE 0

#define KR_IPAddrPortNum KR_CMTE(0)
#define KR_IP            KR_CMTE(1)
#define KR_TCPSocket     KR_CMTE(2)
#define KR_ReadThreadStart KR_CMTE(3)
#define KR_WriteThreadStart KR_CMTE(4)


CMTEMutex_DECLARE_Unlocked(lock);

uint32_t TCPPortNum;
uint32_t netserPortNum;	// 0 through 3
uint32_t IPAddress;

unsigned int read_threadNum;
unsigned int write_threadNum;

bool isOpen = false;

/* readState has two bits.
 * RS_haveReader means slot readerCap in KR_KEYSTORE has a resume key to
 *   the reader and readMaxPairs has the amount of data he can accept.
 * RS_threadAvail means the readThread is, or soon will be, Available
 *   to be called to be notified that a reader exists.
 * RS_haveReader implies not RS_threadAvail.
 */
#define RS_haveReader 0x1
#define RS_threadAvail 0x2
unsigned int readState = 0;
unsigned long readMaxPairs;

// Slots in KR_KEYSTORE:
enum {
  readerCap = LKSN_APP,
  writerCap,
  socketCap,
  lastLksnCap = socketCap
};

/***************************  Socket creation stuff  ***********************/

CMTEMutex_DECLARE_Unlocked(connLock);

// Connection state (valid only under connLock):
#define cs_ReadThreadHas  0x1
#define cs_WriteThreadHas 0x2
// Initially, both threads have the best socket we have
// (they will immediately discover it's no good):
unsigned int connState = cs_ReadThreadHas | cs_WriteThreadHas;

// If connState is nonzero, we have a socket cap in socketCap.

// Get or create a socket cap.
// mask is cs_ReadThreadHas or cs_WriteThreadHas.
// Returns false if serial port not open, true if got socket cap.
bool
GetSocket(unsigned int mask)
{
  result_t result;

  if (! isOpen)
    return false;

  CMTEMutex_lock(&connLock);
  if (connState & mask) {
    // This thread is complaining about the cap we gave it,
    // so assume it is no longer good and create a new socket.
    for (;;) {	// loop trying to create socket
      DEBUG(conn) kprintf(KR_OSTREAM, "Netser %d creating socket.\n", netserPortNum);
      result = capros_IP_TCPConnect(KR_IP, IPAddress, TCPPortNum, KR_TCPSocket);
      switch (result) {
      default:	// could be RC_capros_IPDefs_NoMem
        assert(false);

      case RC_capros_key_Restart:
        kprintf(KR_OSTREAM, "Netser %d connection suffered a system restart.\n", netserPortNum);
        break;

      case RC_capros_IPDefs_Refused:
        kprintf(KR_OSTREAM, "Netser %d connection refused.\n", netserPortNum);
        break;

      case RC_capros_IPDefs_Aborted:
        kprintf(KR_OSTREAM, "Netser %d connection aborted.\n", netserPortNum);
        break;

      case RC_OK:
        // Save socket cap.
        result = capros_Node_swapSlotExtended(KR_KEYSTORE,
                   socketCap, KR_TCPSocket, KR_VOID);
        assert(result == RC_OK);
        DEBUG(conn) kprintf(KR_OSTREAM, "Netser %d created socket.\n", netserPortNum);

        // We created a new socket and gave it to this thread.
        // The other thread doesn't have it.
        connState = mask;

        CMTEMutex_unlock(&connLock);
        return true;
      }

      // We failed to create the socket.
      // Wait a bit and try again.
      DEBUG(conn) kprintf(KR_OSTREAM, "Netser %d sleeping to create socket.\n", netserPortNum);
      result = capros_Sleep_sleep(KR_SLEEP, 5000);	// wait 5 seconds
      assert(result == RC_OK || result == RC_capros_key_Restart);
    }
  } else {
    // This thread is complaining about its cap,
    // but it doesn't have the latest socket. Give it the socket.
    result = capros_Node_getSlotExtended(KR_KEYSTORE, socketCap, KR_TCPSocket);
    assert(result == RC_OK);

    // The following is equivalent to connState |= mask,
    // since the other bit will always be set already.
    connState = cs_ReadThreadHas | cs_WriteThreadHas;

    CMTEMutex_unlock(&connLock);
    return true;
  }
}

/*****************************  Reading stuff  *************************/

// The most characters we will read from the net at once:
#define maxReadChars 250
uint8_t netReadBuf[maxReadChars];
uint8_t serReadBuf[maxReadChars*2];	// pairs

uint32_t netChars;	// number of data characters in netReadBuf
unsigned int firstChar;	// index of first data character in netReadBuf
uint8_t flags;

// Call under lock.
// The data is in serReadBuf.
void
WakeUpReader(unsigned long charsToDeliver, uint32_t rc)
{
  result_t result;

  DEBUG(read) kprintf(KR_OSTREAM, "Netser read delivering %d chars\n", charsToDeliver);

  result = capros_Node_getSlotExtended(KR_KEYSTORE, readerCap, KR_TEMP0);
  assert(result == RC_OK);

  Message Msg = {
    .snd_invKey = KR_TEMP0,
    .snd_code = rc,
    .snd_w1 = charsToDeliver,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_data = &serReadBuf[0],
    .snd_len = charsToDeliver*2,
  };
  PSEND(&Msg);
  assert(readState == RS_haveReader);
  readState = 0;
}

void *
readThread(void * data)
{
  result_t result;

  CMTEMutex_lock(&lock);
  while (1) {
    if (readState == RS_haveReader) {
      if (netChars == 0) {
        CMTEMutex_unlock(&lock);
        DEBUG(read) kprintf(KR_OSTREAM, "Netser reading net.\n");
        for (;;) {
          result = capros_TCPSocket_receive(KR_TCPSocket, maxReadChars,
                     &netChars, &flags, &netReadBuf[0]);
          switch (result) {
          default:
            assert(false);
          case RC_capros_TCPSocket_RemoteClosed:
            capros_key_destroy(KR_TCPSocket);
          case RC_capros_key_Void:
            if (GetSocket(cs_ReadThreadHas))
              continue;		// try reading again
            // Serial port is closed.
            netChars = 0;	// return to the reader with no data
          case RC_OK:
            break;
          }
          break;
        }
        DEBUG(read) kprintf(KR_OSTREAM, "Netser read got %d chars: %.2x %.2x\n",
                           netChars, netReadBuf[0], netReadBuf[1]);
        // assert(netChars > 0 || ! isOpen); // disabled due to timing window
        firstChar = 0;
        CMTEMutex_lock(&lock);
        // Now we have data for the reader,
        // but the reader may have disappeared (due to timeout).
      } else {
        // Deliver characters to the reader.
        assert(readMaxPairs > 0);
        unsigned int charsToDeliver = 0;
        while (netChars && charsToDeliver < readMaxPairs) {
          netChars--;
          uint8_t c = netReadBuf[firstChar++];
          serReadBuf[charsToDeliver *2] = 0;	// flag
          serReadBuf[charsToDeliver *2 + 1] = c;
          charsToDeliver++;
        }
        WakeUpReader(charsToDeliver, RC_OK);
      }
    } else {
      // Wait for a reader.
      readState = RS_threadAvail;
      CMTEMutex_unlock(&lock);
      DEBUG(read) kprintf(KR_OSTREAM, "Netser read waiting for reader.\n");
      Message Msg = {
        .snd_invKey = KR_VOID,
        .snd_code = RC_OK,
        .snd_key0 = KR_VOID,
        .snd_key1 = KR_VOID,
        .snd_key2 = KR_VOID,
        .snd_rsmkey = KR_VOID,
        .snd_len = 0,
        .rcv_key0 = KR_VOID,
        .rcv_key1 = KR_VOID,
        .rcv_key2 = KR_VOID,
        .rcv_rsmkey = KR_VOID,
        .rcv_limit = 0
      };
      RETURN(&Msg);
      DEBUG(read) kprintf(KR_OSTREAM, "Netser read got reader.\n");
      CMTEMutex_lock(&lock);
    }
  }
}

/*****************************  Timer stuff  ****************************/

void
timerFunction(unsigned long data)
{
  CMTEMutex_lock(&lock);
  if (readState == RS_haveReader) {
    DEBUG(read) kprintf(KR_OSTREAM, "Netser %d timed out, waking.\n", netserPortNum);
    WakeUpReader(0, RC_capros_SerialPort_TimedOut);
  } else {
    DEBUG(read) kprintf(KR_OSTREAM, "Netser %d timed out, no wake.\n", netserPortNum);
  }
  CMTEMutex_unlock(&lock);
}

CMTETimer tmr;

/*****************************  Write stuff  ****************************/

// The number of characters we can receive at once:
#define maxRcvChars capros_SerialPort_maxWriteBytes

/* Buffers for receiving strings passed to the SerialPort key. */
uint8_t rcvBuf1[maxRcvChars];
uint8_t rcvBuf2[maxRcvChars];
uint8_t * currentRcvBuf = &rcvBuf1[0];

bool haveWriter = false;
unsigned int writeLen;

void *
writeThread(void * data)
{
  result_t result;

  Message Msg = {
    .snd_code = RC_OK,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_VOID,
    .rcv_limit = 0
  };
  Msg.snd_invKey = KR_VOID;

  while (1) {
    // Possibly return to the writer, and wait for a new writer.
    RETURN(&Msg);

    assert(haveWriter);
    // Write data is in the non-current buffer:
    uint8_t * writeBuf = (uint8_t *)((uint32_t)currentRcvBuf ^ ((uint32_t)&rcvBuf1[0] ^ (uint32_t)&rcvBuf2[0]));
    DEBUG(write) {
      int i;
      kprintf(KR_OSTREAM, "Netser writing %d from %#x:", writeLen, writeBuf);
      for (i = 0; i < writeLen; i++) {
        kprintf(KR_OSTREAM, " %.2x", writeBuf[i]);
      }
      kprintf(KR_OSTREAM, "\n");
    }

    // Send raw data.
    for (;;) {
      result = capros_TCPSocket_send(KR_TCPSocket, writeLen,
                 capros_TCPSocket_flagPush, writeBuf);
      switch (result) {
      default:
        assert(false);

      case RC_capros_TCPSocket_Already:	// we closed the connection
      case RC_capros_key_Void:
        if (GetSocket(cs_WriteThreadHas))
          continue;		// try reading again
        // The serial port is closed. Just discard the data.
      case RC_OK:
        break;
      }
      break;
    }

    // Return to the writer.
    result = capros_Node_getSlotExtended(KR_KEYSTORE, writerCap, KR_TEMP0);
    assert(result == RC_OK);

    CMTEMutex_lock(&lock);

    haveWriter = false;

    CMTEMutex_unlock(&lock);

    Msg.snd_invKey = KR_TEMP0;
  }
}

/************************* SerialPort cap server *************************/

// Must be under lock.
bool
ProcessRead(Message * msg)
{
  result_t result;

  switch (readState) {
  default: ;
    assert(false);
  case RS_haveReader:
    msg->snd_code = RC_capros_SerialPort_Already;
    return false;

  case RS_threadAvail:
  case 0:
    readMaxPairs = msg->rcv_w1;	// max number of pairs to read
    // Save the reader capability.
    result = capros_Node_swapSlotExtended(KR_KEYSTORE, readerCap,
               KR_RETURN, KR_VOID);
    assert(result == RC_OK);
    if (readState == RS_threadAvail) {
      // Wake up the read thread.
      msg->snd_invKey = KR_ReadThreadStart;
    } else {
      msg->snd_invKey = KR_VOID;
    }
    readState = RS_haveReader;
    return true;
  }
}

result_t
cmte_main(void)
{
  result_t result;

  result = CMTETimer_setup();
  assert(result == RC_OK);

  CMTETimer_init(&tmr, timerFunction, 0);

  assert(maxReadChars <= capros_TCPSocket_maxReceiveLength);

  result = capros_Number_get(KR_IPAddrPortNum, &netserPortNum, &TCPPortNum, &IPAddress);
  assert(result == RC_OK);

  // Allocate slots for resume keys to waiters:
  result = capros_SuperNode_allocateRange(KR_KEYSTORE,
                                          LKSN_APP, lastLksnCap);
  assert(result == RC_OK);

  Message Msg = {
    .snd_invKey = KR_VOID,
    .snd_code = RC_OK,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
    .rcv_key0 = KR_VOID,
    .rcv_key1 = KR_VOID,
    .rcv_key2 = KR_VOID,
    .rcv_rsmkey = KR_RETURN,
    .rcv_limit = maxRcvChars
  };

  kprintf(KR_OSTREAM, "Netser started, serPort=%d.\n", netserPortNum);

  // Create read thread.
  result = CMTEThread_create(4096, &readThread, NULL, &read_threadNum);
  assert(result == RC_OK);

  // Create write thread.
  result = CMTEThread_create(4096, &writeThread, NULL, &write_threadNum);
  assert(result == RC_OK);

  // Create start capabilities to threads.
  result = capros_Node_getSlotExtended(KR_KEYSTORE,
             LKSN_THREAD_PROCESS_KEYS + read_threadNum, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Process_makeStartKey(KR_TEMP0, 0, KR_ReadThreadStart);
  assert(result == RC_OK);

  result = capros_Node_getSlotExtended(KR_KEYSTORE,
             LKSN_THREAD_PROCESS_KEYS + write_threadNum, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Process_makeStartKey(KR_TEMP0, 0, KR_WriteThreadStart);
  assert(result == RC_OK);

  // A start key to this process acts like a SerialPort key.
  // Give our serial cap to nplink.
  DEBUG(conn) kprintf(KR_OSTREAM, "Netser %d calling nplink.\n", netserPortNum);
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_VOLSIZE, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_getSlot(KR_TEMP0, volsize_pvolsize, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Node_getSlot(KR_TEMP0, volsize_nplinkCap, KR_TEMP0);
  assert(result == RC_OK);
  // NPLink's serial port # is our port # (0-3) plus 4.
  result = capros_NPLink_RegisterNPCap(KR_TEMP0, KR_TEMP1,
             IKT_capros_SerialPort, netserPortNum + 4);
  assert(result == RC_OK);
  DEBUG(conn) kprintf(KR_OSTREAM, "Netser %d called nplink.\n", netserPortNum);

  for (;;) {
    Msg.rcv_key0 = KR_VOID;
    Msg.rcv_key1 = KR_VOID;
    Msg.rcv_key2 = KR_VOID;
    Msg.rcv_rsmkey = KR_RETURN;
    Msg.rcv_data = currentRcvBuf;

    /* Msg.snd_invKey is one of the following:
         KR_RETURN
         KR_VOID
         KR_ReadThreadStart
         KR_WriteThreadStart
       For KR_RETURN we must do a prompt invocation: */
    if (Msg.snd_invKey != KR_RETURN)
      NPRETURN(&Msg);	// start key needs a nonprompt return
    else
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

    switch (Msg.rcv_code) {
    default:
      Msg.snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      Msg.snd_w1 = IKT_capros_SerialPort;
      break;

    case OC_capros_SerialPort_open:
    {
      if (isOpen) {	// already open - can only open once
        Msg.snd_code = RC_capros_SerialPort_Already;
        break;
      }

      /* It might be nice to create the connection here,
      and return any error as RC_capros_key_NoAccess. */
      isOpen = true;

      // Threads will create the connection.
      break;
    }

    case OC_capros_SerialPort_close:
    {
      if (! isOpen) {	// already closed
        Msg.snd_code = RC_capros_SerialPort_Already;
        break;
      }
      if (haveWriter) {
        Msg.snd_code = RC_capros_SerialPort_WriteInProgress;
        break;
      }

      isOpen = false;

      result = capros_Node_getSlotExtended(KR_KEYSTORE, socketCap, KR_TCPSocket);
      assert(result == RC_OK);
      result = capros_TCPSocket_close(KR_TCPSocket);
      switch (result) {
      default:
        assert(false);	// FIXME
      case RC_OK:
        break;
      }
      break;
    }

    case 0:	// capros_SerialPort_read (not yet working in IDL)
    {
      if (! isOpen		// port isn't open
          || Msg.rcv_w1 == 0) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      /* A large readCount is not a problem. We will not send more than
         maxReadChars characters. */

      CMTEMutex_lock(&lock);
      if (ProcessRead(&Msg)) {
        // We accepted the read.
        CMTETimer_delete(&tmr);	// no timeout
      }
      CMTEMutex_unlock(&lock);
      break;
    }

    case 2:	// capros_SerialPort_readTimeout (not yet working in IDL)
    {
      if (! isOpen		// port isn't open
          || Msg.rcv_w1 == 0) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      /* A large readCount is not a problem. We will not send more than
         maxReadChars characters. */

      uint32_t timeout = Msg.rcv_w2;	// timeout in microseconds
      DEBUG(read) kprintf(KR_OSTREAM, "Netser %d got readTimeout.\n", netserPortNum);
      CMTEMutex_lock(&lock);
      if (ProcessRead(&Msg)) {
        // We accepted the read.
        DEBUG(read) kprintf(KR_OSTREAM, "Netser %d setting timeout to %u usec.\n", netserPortNum, timeout);
        CMTETimer_setDuration(&tmr, timeout*1000ULL);
      }
      CMTEMutex_unlock(&lock);
      break;
    }

    case 1: ;	// OC_capros_SerialPort_write (not yet working in IDL)
    {
      uint32_t count = Msg.rcv_sent;
      if (! isOpen		// port isn't open
          || count >= capros_SerialPort_maxWriteBytes) {
        Msg.snd_code = RC_capros_key_RequestError;
        break;
      }
      CMTEMutex_lock(&lock);
      if (haveWriter)
        Msg.snd_code = RC_capros_SerialPort_Already;
      else {
        writeLen = count;
        // Save the writer capability:
        result = capros_Node_swapSlotExtended(KR_KEYSTORE, writerCap,
                   KR_RETURN, KR_VOID);
        assert(result == RC_OK);

        // Switch receive buffers:
        currentRcvBuf = (uint8_t *)((uint32_t)currentRcvBuf ^ ((uint32_t)&rcvBuf1[0] ^ (uint32_t)&rcvBuf2[0]));

        // Wake up the write thread:
        Msg.snd_invKey = KR_WriteThreadStart;

        haveWriter = true;
      }
      CMTEMutex_unlock(&lock);
      break;
    }

    case OC_capros_SerialPort_setTermios2:
      /* Serial port parameters are configured statically in the PK70EX-232.
      Ignore any attempt to change them here. */
      break;

    case OC_capros_SerialPort_waitForBreak:
      /* No ability to handle break on the PK70EX-232. */
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_waitForICountChange:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_getICounts:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_discardBufferedOutput:
      /* We send data to the port as soon as we get it, so we have no
         buffered data to discard. */
      break;

    case OC_capros_SerialPort_writeHighPriorityChar:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_stopTransmission:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_startTransmission:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_beginBreak:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_endBreak:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_waitUntilSent:
      /* FIXME: this should wait until we are not writing to the TCPSocket. */
      break;

    case OC_capros_SerialPort_getModemStatus:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_setModemStatus:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_suspend:
      Msg.snd_code = RC_capros_key_RequestError;
      break;

    case OC_capros_SerialPort_resume:
      Msg.snd_code = RC_capros_key_RequestError;
      break;
    }
  }
}
