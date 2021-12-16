/*
 * Copyright (C) 2009-2011, Strawberry Development Group.
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

/* Test program for demonstrating a web server. */

#include <stdint.h>
#include <string.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Constructor.h>
#include <idl/capros/Node.h>
#include <idl/capros/FileServer.h>
#include <idl/capros/File.h>
#include <idl/capros/IndexedKeyStore.h>
#include <idl/capros/HTTP.h>
#include <idl/capros/HTTPResourceGetConstructor.h>
#include <idl/capros/HTTPResourceGetConstructorExtended.h>
#include <idl/capros/NetListener.h>

#include <domain/CMME.h>
#include <domain/CMMEMaps.h>
#include <domain/domdbg.h>
#include <domain/assert.h>

#include "test.h"

#define KR_HomeFile      KR_CMME(0)
#define KR_HTTPDirectory KR_CMME(1)
#define KR_NetListener   KR_CMME(2)
#define KR_FileServer    KR_CMME(3)
#define KR_HTTPBuilder   KR_CMME(4)
#define KR_HTTPConstits  KR_CMME(5)

#define ckOK \
  if (result != RC_OK) \
    kdprintf(KR_OSTREAM, "%s:%d: result is %#x!\n", __FILE__, __LINE__, result);


/***********************  Stuff for home html file  ***********************/

capros_File_fileLocation homeFileCursor = 0;

void
AppendToHomeFile(const void * data, unsigned int length)
{
  result_t result;
  uint32_t len;

  result = capros_File_writeLong(KR_HomeFile, homeFileCursor, length,
             (uint8_t *)data, &len);
  ckOK
  assert(len == length);
  homeFileCursor += length;
}

void
InsertHTTPConstit(unsigned int slot)
{
  result_t result;

  result = capros_Node_getSlot(KR_HTTPConstits, slot, KR_TEMP0);
  ckOK
  result = capros_Constructor_insertConstituent(KR_HTTPBuilder, slot, KR_TEMP0);
  ckOK
}

result_t
cmme_main(void)
{
  result_t result;

  kprintf(KR_OSTREAM, "Test starting.\n");

  result = maps_init();
  assert(result == RC_OK);

  // Create FileServer.
  result = capros_Node_getSlot(KR_CONSTIT, KC_FileServerC, KR_TEMP0);
  ckOK
  result = capros_Constructor_request(KR_TEMP0, KR_BANK, KR_SCHED, KR_VOID,
                                      KR_FileServer);
  ckOK

  // Create home page file.
  result = capros_FileServer_createFile(KR_FileServer, KR_HomeFile);
  ckOK
  capros_Node_getSlotExtended(KR_CONSTIT, KC_HomePageC, KR_TEMP0);
  result = capros_Constructor_insertConstituent(KR_TEMP0, 4, KR_HomeFile);
  ckOK
  // The homepage object won't look at the file until we've started http,
  // which is after we've finished writing the file.

  // Create HTTP directory.
  result = capros_Node_getSlot(KR_CONSTIT, KC_IKSC, KR_TEMP0);
  ckOK
  result = capros_Constructor_request(KR_TEMP0, KR_BANK, KR_SCHED, KR_VOID,
                                      KR_HTTPDirectory);
  ckOK

#if 1	// uploader
  // Map the cacert.pem data.
  long mapReservation = maps_reserve_locked(1);
  result = capros_Node_getSlot(KR_CONSTIT, KC_HTTPConstit, KR_TEMP0);
  ckOK
  result = capros_Node_getSlot(KR_TEMP0, capros_HTTP_KC_Certificate, KR_TEMP1);
  ckOK
  result = maps_mapPage_locked(mapReservation, KR_TEMP1);
  ckOK

  static const char * certRef = CertSwissNum;

  // Create cacert.pem file.
  result = capros_FileServer_createFile(KR_FileServer, KR_TEMP2);
  ckOK
  // Store the data in it.
  uint8_t * data = (uint8_t *)maps_pgOffsetToAddr(mapReservation);
  uint32_t length = strlen((char *)data);
  uint32_t len;
  result = capros_File_writeLong(KR_TEMP2, 0, length, data, &len);
  ckOK
  assert(len == length);
  // Store the file in the http directory.
  result = capros_IndexedKeyStore_put(KR_HTTPDirectory, KR_TEMP2,
             strlen(certRef), (uint8_t *)certRef);
  ckOK

  maps_liberate_locked(mapReservation, 1);

  static const char * confinedRef = ConfinedSwissNum;

  // Create a link to the demo program resource.
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_DemoResource, KR_TEMP1);
  ckOK
  // Store the resource in the http directory.
  result = capros_IndexedKeyStore_put(KR_HTTPDirectory, KR_TEMP1,
             strlen(confinedRef), (uint8_t *)confinedRef);
  ckOK

  // Append a link to the home html file.
  static const char homeFileDemo1[] =
        "<h3>Confinement demo:</h3>"
	"<p>Command to upload <tt>myConfinedProgram</tt>: </p>\n"
	"<tt>curl --cacert cacert.pem -T myConfinedProgram https://demo.capros.org:"
        HTTPSPortNumString "/?s=";
  static const char homeFileDemo2[] = "</tt>\r"
	"<p> where <tt>cacert.pem</tt> can be downloaded "
	"<a href=\"cacert.pem?s=";
  static const char homeFileDemo3[] = 
	"\">here</a>.</p>"
        "<p><tt>myConfinedProgram</tt> can listen on port "
        ConfinedPortNumString ".</p>"
	;
  AppendToHomeFile(homeFileDemo1, sizeof(homeFileDemo1)-1);
  AppendToHomeFile(confinedRef, strlen(confinedRef));
  AppendToHomeFile(homeFileDemo2, sizeof(homeFileDemo2)-1);
  AppendToHomeFile(certRef, strlen(certRef));
  AppendToHomeFile(homeFileDemo3, sizeof(homeFileDemo3)-1);
#endif	// demo upload

  static char homeFileTrailer[] = "</BODY></HTML>";
  AppendToHomeFile(homeFileTrailer, sizeof(homeFileTrailer)-1);

  capros_Node_getSlotExtended(KR_CONSTIT, KC_HomePageLimit, KR_TEMP0);
  uint32_t sendLimit;
  result = capros_Number_get32(KR_TEMP0, &sendLimit);
  ckOK

  // Make a HTTPResource for the home page object.
  result = capros_Node_getSlot(KR_CONSTIT, KC_HTTPRGetC, KR_TEMP0);
  ckOK
  result = capros_HTTPResourceGetConstructor_construct(KR_TEMP0,
             KR_BANK, KR_SCHED,
             KR_VOID, KR_VOID, KR_VOID,
             KR_TEMP1);
  ckOK
  capros_Node_getSlotExtended(KR_CONSTIT, KC_HomePageC, KR_TEMP0);
  const cap_t homePageCap = KR_TEMP2;
  result = capros_HTTPResourceGetConstructorExtended_init(KR_TEMP1,
             KR_TEMP0, sendLimit, homePageCap);
  ckOK

  // Put home page object in directory.
  result = capros_IndexedKeyStore_put(KR_HTTPDirectory, homePageCap,
             11, (uint8_t *)HomePageSwissNum);
  ckOK

  // Create the HTTP constructor.
  result = capros_Node_getSlot(KR_CONSTIT, KC_MetaCon, KR_TEMP0);
  ckOK
  result = capros_Constructor_request(KR_TEMP0, KR_BANK, KR_SCHED, KR_VOID,
             KR_HTTPBuilder);
  ckOK
  result = capros_Node_getSlot(KR_CONSTIT, KC_HTTPConstit, KR_HTTPConstits);
  ckOK

  result = capros_Node_getSlot(KR_HTTPConstits, capros_HTTP_KC_ProtoSpace,
                               KR_TEMP0);
  ckOK
  result = capros_Constructor_insertAddrSpace32(KR_HTTPBuilder, KR_TEMP0,
                  0 /* well-known start addr for interpreter */ );
  ckOK

  result = capros_Node_getSlot(KR_HTTPConstits, 9, KR_TEMP0);
  ckOK
  result = capros_Constructor_insertSymtab(KR_HTTPBuilder, KR_TEMP0);
  ckOK

  InsertHTTPConstit(capros_HTTP_KC_ProgramText);
  InsertHTTPConstit(capros_HTTP_KC_ProgramDataVCS);
  InsertHTTPConstit(capros_HTTP_KC_ProtoSpace);
  InsertHTTPConstit(capros_HTTP_KC_ProgramPC);
  InsertHTTPConstit(capros_HTTP_KC_OStream);
  InsertHTTPConstit(capros_HTTP_KC_RSAKey);
  InsertHTTPConstit(capros_HTTP_KC_Certificate);
  InsertHTTPConstit(capros_HTTP_KC_RTC);
  // done with KR_HTTPConstits

  result = capros_Constructor_insertConstituent(KR_HTTPBuilder,
             capros_HTTP_KC_FileServer, KR_FileServer);
  ckOK

  result = capros_Constructor_insertConstituent(KR_HTTPBuilder,
             capros_HTTP_KC_Directory, KR_HTTPDirectory);
  ckOK

  result = capros_Constructor_seal(KR_HTTPBuilder, KR_TEMP2);
  ckOK

  // Create a NetListener
  result = capros_Node_getSlot(KR_CONSTIT, KC_NetListenerC, KR_TEMP0);
  ckOK
  result = capros_Constructor_request(KR_TEMP0, KR_BANK, KR_SCHED,
                                      KR_VOID, KR_NetListener);
  ckOK
  result = capros_Node_getSlot(KR_CONSTIT, KC_HTTPSPort, KR_TEMP1);
  ckOK
  result = capros_NetListener_listen(KR_NetListener, KR_TEMP1, KR_TEMP2);
  ckOK

  // We are now live on the net!

  kprintf(KR_OSTREAM, "Test finished setup.\n");

#if 0////
  for (;;) {
    result = capros_Node_getSlot(KR_CONSTIT, KC_SLEEP, KR_TEMP0);
    ckOK
    result = capros_Sleep_sleep(KR_TEMP0, 10000);	// sleep 10 seconds
    assert(result == RC_OK || result == RC_capros_key_Restart);
  }
#endif
  return 0;
}

