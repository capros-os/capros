/*
 * Copyright (C) 2010, Strawberry Development Group.
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

/* Sample program to run in the confined environment of the https tutorial. */

#include <string.h>
#include <eros/Invoke.h>

#include <idl/capros/Node.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/TCPPortNum.h>
#include <idl/capros/Sleep.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>

/* We are started with a VCS as our entire address space, so we don't
 * need the runtime initialization of the address space: */
uint32_t __rt_runtime_hook = 0;

// Our constituents:
#define KC_Discrim 0
#define KC_Sleep 1
#define KC_RTC 2
#define KC_SNodeC 3
#define KC_FileSrvC 4
#define KC_IKSC 5
#define KC_OSTREAM 6
#define KC_TCPPort 7

#define KR_OSTREAM         KR_APP(0)
#define KR_TCPPort         KR_APP(1)
#define KR_Sleep           KR_APP(2)
#define KR_TCPListenSocket KR_APP(3)
#define KR_TCPSocket       KR_APP(4)

int
main(void)
{
  result_t result;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_TCPPort, KR_TCPPort);
  capros_Node_getSlot(KR_CONSTIT, KC_Sleep, KR_Sleep);

  // Verify the TCPPort cap:
  capros_key_type keyType;
  result = capros_key_getType(KR_TCPPort, &keyType);
  assert(result == RC_OK);
  assert(keyType == IKT_capros_TCPPortNum);

  // Create a TCPListenSocket:
  result = capros_TCPPortNum_listen(KR_TCPPort, KR_TCPListenSocket);
  if (result == RC_capros_IPDefs_Already) {
    kprintf(KR_OSTREAM, "confined: Another process has opened the port.\n");
    goto mainExit;
  }
  assert(result == RC_OK);

  // Wait for a connection:
  kprintf(KR_OSTREAM, "confined: about to listen\n");
  result = capros_TCPListenSocket_accept(KR_TCPListenSocket, KR_TCPSocket);
  assert(result == RC_OK);

  kprintf(KR_OSTREAM, "confined: received connection ");
  capros_IPDefs_ipv4Address ipaddr;
  capros_IPDefs_portNumber port;
  result = capros_TCPSocket_getRemoteAddr(KR_TCPSocket,
             &ipaddr, &port);
  assert(result == RC_OK);
  kprintf(KR_OSTREAM, "from %d.%d.%d.%d:%d\n",
          ipaddr>>24, (ipaddr>>16)&0xff, (ipaddr>>8)&0xff, ipaddr&0xff,
          port);

  // Output data to the connection:
  result = capros_TCPSocket_sendLong(KR_TCPSocket,
               18, capros_TCPSocket_flagPush, (uint8_t *)"Hello from CapROS\n" );
  switch (result) {
  default:
    kdprintf(KR_OSTREAM, "TCPSocket_sendLong returned %#x\n", result);
  case RC_capros_key_Restart:
    kdprintf(KR_OSTREAM, "TCPSocket_sendLong Restart\n");
  case RC_OK:
    break;
  }

  kprintf(KR_OSTREAM, "Sent hello.\n");

  // Echo data from the connection:
  while (1) {
    uint8_t data[512];
    uint32_t numReceived;
    uint8_t flags;
    bool stop = false;

    result = capros_TCPSocket_receiveLong(KR_TCPSocket, sizeof(data),
               &numReceived, &flags, &data[0] );
    switch (result) {
    default:
      kdprintf(KR_OSTREAM, "TCPSocket_receiveLong returned %#x\n", result);
    case RC_capros_TCPSocket_RemoteClosed:
      result = capros_TCPSocket_abort(KR_TCPSocket);
    case RC_capros_key_Void:
      kprintf(KR_OSTREAM, "TCPSocket_receiveLong returned Void\n");
    case RC_capros_key_Restart:
      stop = true;
    case RC_OK:
      break;
    }

    if (stop)
      break;

    kprintf(KR_OSTREAM, "Echoing %d char(s).\n", numReceived);

    result = capros_TCPSocket_sendLong(KR_TCPSocket,
               numReceived, capros_TCPSocket_flagPush, &data[0] );
    switch (result) {
    default:
      kdprintf(KR_OSTREAM, "TCPSocket_sendLong returned %#x\n", result);
    case RC_capros_key_Void:
      kprintf(KR_OSTREAM, "TCPSocket_sendLong returned Void\n");
    case RC_capros_key_Restart:
      stop = true;
    case RC_OK:
      break;
    }

    if (stop)
      break;
  }

  capros_key_destroy(KR_TCPListenSocket);

mainExit:
  kprintf(KR_OSTREAM, "confined: exiting.\n");
  return 0;
}
