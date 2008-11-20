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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/DevPrivs.h>
#include <idl/capros/NPIP.h>
#include <idl/capros/NPLinkee.h>

#include <linux/usb/ch9.h>

#include <domain/Runtime.h>

#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_IP   KR_APP(0)
#define KR_OSTREAM KR_APP(1)
#define KR_SLEEP   KR_APP(2)
#define KR_DEVPRIVS KR_APP(3)
#define KR_TCPSocket KR_APP(4)
#define KR_TCPListenSocket KR_APP(5)
#define KR_UDPPort KR_APP(6)

#define fourByteVal(a,b,c,d) (((((uint32_t)a)*256+(b))*256+(c))*256+(d))


const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
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
  assert(theType == IKT_capros_NPIP);

  // Create a socket:

  kprintf(KR_OSTREAM, "Connecting.\n");

  // Connect to Linux echo port
#define testIPAddr fourByteVal(192,168,0,32)
#define testIPPort 7	// echo port
  result = capros_NPIP_connect(KR_IP, testIPAddr, testIPPort,
				KR_TCPSocket);
  switch (result) {
  default:
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result);
    break;
  case RC_capros_IPDefs_Refused:
    kdprintf(KR_OSTREAM, "Connection refused.\n");
    break;
  case RC_OK:
    break;
  }

  result = capros_key_getType(KR_TCPSocket, &theType);
  ckOK
  assert(theType == IKT_capros_TCPSocket);

  kprintf(KR_OSTREAM, "Connected.\n");

  uint8_t m[] = "Test message";
  result = capros_TCPSocket_send(KR_TCPSocket, sizeof(m),
                                 capros_TCPSocket_flagPush, &m[0]);
  ckOK

  uint8_t r[sizeof(m)];
  uint32_t lenRecvd;
  uint8_t flagsRecvd;
  result = capros_TCPSocket_receive(KR_TCPSocket, sizeof(r),
                                    &lenRecvd, &flagsRecvd, &r[0]);
  ckOK
  kprintf(KR_OSTREAM, "Received %d bytes: %#x %#x %#x %#x\n", lenRecvd,
          r[0], r[1], r[2], r[3]);

  kprintf(KR_OSTREAM, "Closing.\n");

  result = capros_TCPSocket_close(KR_TCPSocket);
  ckOK

  // Test UDP.

  kprintf(KR_OSTREAM, "Creating UDP port.\n");
  result = capros_NPIP_createUDPPort(KR_IP, capros_NPIP_LocalPortAny,
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

  kprintf(KR_OSTREAM, "UDP send.\n");
  result = capros_UDPPort_send(KR_UDPPort, testIPAddr, testIPPort,
			sizeof(m), &m[0]);
  ckOK

  kprintf(KR_OSTREAM, "UDP receive.\n");
  uint32_t sourceIPAddr;
  uint16_t sourceIPPort;
  result = capros_UDPPort_receive(KR_UDPPort, sizeof(r),
			&sourceIPAddr, &sourceIPPort,
                        &lenRecvd, &r[0]);
  ckOK
  kprintf(KR_OSTREAM, "Received %d bytes from %#x:%d: %#x %#x %#x %#x\n",
          lenRecvd, sourceIPAddr, sourceIPPort,
          r[0], r[1], r[2], r[3]);

  kprintf(KR_OSTREAM, "Destroying UDP port.\n");
  result = capros_key_destroy(KR_UDPPort);
  ckOK

  // Test Listen.
  kprintf(KR_OSTREAM, "Starting Listen test on port 7.\n");

  result = capros_NPIP_listen(KR_IP, 7,
				KR_TCPListenSocket);
  ckOK

  kprintf(KR_OSTREAM, "Waiting for connection on port 7.\n");

  result = capros_TCPListenSocket_accept(KR_TCPListenSocket, KR_TCPSocket);
  ckOK

  kprintf(KR_OSTREAM, "Closing.\n");

  result = capros_TCPSocket_close(KR_TCPSocket);
  ckOK

  kprintf(KR_OSTREAM, "\nDone.\n");

  return 0;
}

