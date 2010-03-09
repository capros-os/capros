/*
 * Copyright (C) 2009-2010, Strawberry Development Group.
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

/* HomeHTTPRH: HTTPRequestHandler for the home page. */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <eros/Invoke.h>

#include <idl/capros/File.h>
#include <idl/capros/HTTPRequestHandler.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/RTC.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpaceDS.h>

#include "constituents.h"

#define KR_OSTREAM	KR_APP(0)

#define dbg_init    0x1
#define dbg_file    0x2
#define dbg_errors  0x4

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0 | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

/****************************  main server loop  ***************************/

void
Sepuku(result_t retCode)
{
  capros_Node_getSlotExtended(KR_CONSTIT, KC_PROTOSPC, KR_TEMP0);

  /* Invoke the protospace to destroy us and return. */
  protospace_destroy_small(KR_TEMP0, retCode);
  // Does not return here.
}

#define bufSize 4096
uint32_t buf[bufSize / sizeof(uint32_t)];

#define pageBufSize 4096	// must be big enough for our data
char pageBuf[pageBufSize];
char * pageBufIn = pageBuf;	// loc for next char
char * pageBufOut = pageBuf;	// loc for next char

char homeFileHeader[] =
  "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\""
   "\"http://www.w3.org/TR/html4/loose.dtd\">\n"
  "<HTML><HEAD><TITLE>CapROS Demonstration System</TITLE>\n"
  "<meta http-equiv=content-type content=\"text/html; charset=utf-8\">"
  "</HEAD>\n"
  "<BODY><H1>Welcome to the CapROS Demonstration System"
  "<img src=\"http://www.capros.org/img/PolabearSmall.gif\""
  " alt=\"POLA bear\">"
  "</H1>\n"
  ;

static void
GatherBankLimits(cap_t kr, const char * label)
{
  result_t result;

  capros_SpaceBank_limits sblim;
  DEBUG(file) kprintf(KR_OSTREAM, "homepage: getting bank limits\n");
  result = capros_SpaceBank_getLimits(kr, &sblim);
  assert(result == RC_OK);
  pageBufIn += sprintf(pageBufIn, "<p>%s space available: %d frames.</p>",
                       label, sblim.effAllocLimit);
}

/* Gather current data and format it in pageBuf. */
static void
GatherData(void)
{
  result_t result;

  // -1 below because we don't want the terminating NUL.
  memcpy(pageBufIn, homeFileHeader, sizeof(homeFileHeader)-1);
  pageBufIn += sizeof(homeFileHeader)-1;

#if 0
  capros_Node_getSlotExtended(KR_CONSTIT, KC_RTC, KR_TEMP0);
  result = capros_RTC_getTime(KR_TEMP0, &now);
  assert(result == RC_OK);
  time_t CTime = now;	// copy to variable of standard type
  struct tm * gmt = gmtime(&CTime);
  pageBufIn += sprintf(pageBufIn,
        "<p>Data as of %.4d/%.2d/%.2d %.2d:%.2d UT</p>\n",
        gmt->tm_year+1900, gmt->tm_mon+1, gmt->tm_mday,
        gmt->tm_hour, gmt->tm_min);
#endif

  // Report space bank statistics
  pageBufIn += sprintf(pageBufIn, "<h3>Space bank statistics:</h3>");
  capros_Node_getSlotExtended(KR_CONSTIT, KC_NPSB, KR_TEMP0);
  GatherBankLimits(KR_TEMP0, "Non-persistent");
  GatherBankLimits(KR_BANK, "Persistent");

  // DataFile has the remainder of the home page.
  capros_Node_getSlotExtended(KR_CONSTIT, KC_DataFile, KR_TEMP0);
  capros_File_fileLocation fileSize;
  result = capros_File_getSize(KR_TEMP0, &fileSize);
  assert(result == RC_OK);
  uint32_t lengthRead;
  result = capros_File_readLong(KR_TEMP0, 0, fileSize, (uint8_t *)pageBufIn,
             &lengthRead);
  assert(result == RC_OK);
  assert(lengthRead == fileSize);
  pageBufIn += lengthRead;
  DEBUG(file) kprintf(KR_OSTREAM, "home: total size %d\n", pageBufIn - pageBuf);
}

int
main(void)
{
  Message Msg;
  Message * msg = &Msg;
  result_t result;

  capros_Node_getSlotExtended(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  static char responseHeader[] = 
    "\014\0\030Content-Type" "text/html; charset=utf-8"
    "\015\0\010Cache-Control" "no-cache"	// because the data may change
    ;
  char * responseHeaderCursor = responseHeader;
  // Do not include the terminating NUL in the length:
  int responseHeaderLength = sizeof(responseHeader) - 1;

  DEBUG(init) kprintf(KR_OSTREAM, "HomeHTTPRH: initialized\n");

  // Return a start key to self.
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP0);
  assert(result == RC_OK);
  
  msg->snd_invKey = KR_RETURN;
  msg->snd_key0 = KR_TEMP0;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_VOID;
  msg->snd_len = 0;
  msg->snd_code = 0;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;

  msg->rcv_key0 = KR_VOID;
  msg->rcv_key1 = KR_VOID;
  msg->rcv_key2 = KR_VOID;
  msg->rcv_rsmkey = KR_RETURN;
  msg->rcv_data = buf;
  msg->rcv_limit = bufSize;

  for(;;) {
    RETURN(msg);

    // Set default return values:
    msg->snd_invKey = KR_RETURN;
    msg->snd_key0 = KR_VOID;
    msg->snd_key1 = KR_VOID;
    msg->snd_key2 = KR_VOID;
    msg->snd_rsmkey = KR_VOID;
    msg->snd_len = 0;
    msg->snd_code = RC_OK;
    msg->snd_w1 = 0;
    msg->snd_w2 = 0;
    msg->snd_w3 = 0;

    switch (msg->rcv_code) {

    default:
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_destroy:
      Sepuku(RC_OK);

    case OC_capros_key_getType:
      msg->snd_w1 = IKT_capros_HTTPRequestHandler;
      break;

    case 0:	// OC_capros_HTTPRequestHandler_headers
      if (msg->rcv_sent > msg->rcv_limit) {	// sent too much data
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      // Ignore the headers.
      msg->snd_w1 = bufSize;
      break;

    case 1:	// OC_capros_HTTPRequestHandler_body
      // We only support GET and HEAD, so we should not get this.
      msg->snd_code = RC_capros_key_RequestError;
      break;

    case 2:	// OC_capros_HTTPRequestHandler_trailer
      if (msg->rcv_sent > msg->rcv_limit) {	// sent too much data
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      // Ignore the trailers.
      msg->snd_w1 = bufSize;
      break;

    case OC_capros_HTTPRequestHandler_getResponseStatus:
      msg->snd_w1 = 200;	// OK
      msg->snd_w2 = capros_HTTPRequestHandler_TransferEncoding_chunked;
      // Gather data now, in preparation for sending the response body.
      GatherData();
      break;

    case 3:	// OC_capros_HTTPRequestHandler_getResponseHeaderData
    {
      uint32_t dataLimit = msg->rcv_w1;
      if (responseHeaderLength < dataLimit)	// take min
        dataLimit = responseHeaderLength;
      msg->snd_data = responseHeaderCursor;
      msg->snd_len = dataLimit;
      responseHeaderCursor += dataLimit;
      responseHeaderLength -= dataLimit;
      break;
    }

    case 4:	// OC_capros_HTTPRequestHandler_getResponseBody
    {
      uint32_t dataLimit = msg->rcv_w1;
      uint32_t lenAvail = pageBufIn - pageBufOut;
      if (lenAvail < dataLimit)	// take min
        dataLimit = lenAvail;
      msg->snd_data = pageBufOut;
      msg->snd_len = dataLimit;
      pageBufOut += dataLimit;
      break;
    }
    }
  }
}
