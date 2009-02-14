/*
 * Copyright (C) 2009, Strawberry Development Group.
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

/* HTTP test.
*/

#include <string.h>
#include <time.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/Node.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/IndexedKeyStore.h>
#include <idl/capros/FileServer.h>
#include <idl/capros/File.h>
#include <idl/capros/NetListener.h>
#include <idl/capros/HTTP.h>

#include <domain/Runtime.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#define KR_OSTREAM    KR_APP(0)
#define KR_HTTPConstits KR_APP(1)
#define KR_NETLISTENERC KR_APP(2)
#define KR_TCPPortNum KR_APP(3)
#define KR_METACON    KR_APP(4)
#define KR_IKSC       KR_APP(5)
#define KR_NFILEC     KR_APP(6)
#define KR_HTTPSym    KR_APP(7)

#define KR_NETLISTENER KR_APP(9)
#define KR_FileServer KR_APP(10)
#define KR_ProtoSpace KR_APP(11)
#define KR_Directory  KR_APP(12)
#define KR_HTTPB      KR_APP(13)	// unsealed constructor
#define KR_HTTPC      KR_APP(14)	// sealed constructor
#define KR_File       KR_APP(15)

const uint32_t __rt_stack_pointer = 0x20000;
const uint32_t __rt_unkept = 1;

#define ckOK \
  if (result != RC_OK) { \
    kdprintf(KR_OSTREAM, "Line %d result is 0x%08x!\n", __LINE__, result); \
  }

unsigned char fileContents[] = "hello, world";

void
InsertConstit(unsigned int slot)
{
  result_t result;

  result = capros_Node_getSlot(KR_HTTPConstits, slot, KR_TEMP0);
  ckOK
  result = capros_Constructor_insertConstituent(KR_HTTPB, slot, KR_TEMP0);
  ckOK
}

int
main(void)
{
  result_t result;

  kprintf(KR_OSTREAM, "Starting.\n");

  // Create an HTTP constructor.
  result = capros_Constructor_request(KR_METACON, KR_BANK, KR_SCHED, KR_VOID,
             KR_HTTPB);
  ckOK

  kprintf(KR_OSTREAM, "Inserting constituents.\n");

  result = capros_Node_getSlot(KR_HTTPConstits, capros_HTTP_KC_ProtoSpace,
                               KR_TEMP0);
  ckOK
  result = capros_Constructor_insertAddrSpace32(KR_HTTPB, KR_TEMP0,
                  0 /* well-known start addr for interpreter */ );
  ckOK
  result = capros_Constructor_insertSymtab(KR_HTTPB, KR_HTTPSym);
  ckOK

  InsertConstit(capros_HTTP_KC_ProgramVCS);
  InsertConstit(capros_HTTP_KC_ProgramPC);
  InsertConstit(capros_HTTP_KC_ProtoSpace);
  InsertConstit(capros_HTTP_KC_OStream);
  InsertConstit(capros_HTTP_KC_RSAKey);
  InsertConstit(capros_HTTP_KC_Certificate);
  InsertConstit(capros_HTTP_KC_RTC);

  // Create file server.
  result = capros_Constructor_request(KR_NFILEC, KR_BANK, KR_SCHED, KR_VOID,
             KR_FileServer);
  ckOK
  result = capros_Constructor_insertConstituent(KR_HTTPB,
             capros_HTTP_KC_FileServer, KR_FileServer);
  ckOK

  // Create file
  result = capros_FileServer_createFile(KR_FileServer, KR_BANK, KR_SCHED,
             KR_File);
  ckOK
  unsigned long lengthWritten;
  result = capros_File_write(KR_File, 0, sizeof(fileContents), fileContents,
             &lengthWritten);
  ckOK
  assert(lengthWritten == sizeof(fileContents));
#if 0
  // Check the data
  unsigned char readbuf[80];
  result = capros_File_read(KR_File, 0, sizeof(readbuf), readbuf,
             &lengthWritten);
  ckOK
  assert(lengthWritten == sizeof(fileContents));
  kprintf(KR_OSTREAM, "File contains %d %d %d\n", readbuf[0], readbuf[1], readbuf[2]);
#endif
  

  // Create directory.
  result = capros_Constructor_request(KR_IKSC, KR_BANK, KR_SCHED, KR_VOID,
             KR_Directory);
  ckOK
  result = capros_IndexedKeyStore_put(KR_Directory, KR_File,
             3, (unsigned char *)"bar");
  ckOK
  result = capros_Constructor_insertConstituent(KR_HTTPB,
             capros_HTTP_KC_Directory, KR_Directory);
  ckOK

  kprintf(KR_OSTREAM, "Sealing constructor.\n");

  result = capros_Constructor_seal(KR_HTTPB, KR_HTTPC);
  ckOK

  kprintf(KR_OSTREAM, "Constructing NetListener.\n");

  result = capros_Constructor_request(KR_NETLISTENERC, KR_BANK, KR_SCHED,
                                      KR_VOID, KR_NETLISTENER);
  ckOK

  kprintf(KR_OSTREAM, "NetListener listen.\n");

  result = capros_NetListener_listen(KR_NETLISTENER, KR_TCPPortNum, KR_HTTPC);
  ckOK

  kprintf(KR_OSTREAM, "\nDone.\n");

  return 0;
}

