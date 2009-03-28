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

/* LogHTTP: Handle a request from HTTP for a Logfile. */

//#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/Logfile.h>
#include <idl/capros/HTTPRequestHandler.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpaceDS.h>

#include "constituents.h"

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

#define KR_OSTREAM	KR_APP(0)
#define KR_Logfile      KR_ARG(0)	// from construction

#define dbg_init    0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)

void
Sepuku(result_t retCode)
{
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_TEMP0);

  /* Invoke the protospace to destroy us and return. */
  protospace_destroy_small(KR_TEMP0, retCode);
  // Does not return here.
}

#define bufSize 4096
uint32_t buf[bufSize / sizeof(uint32_t)];

int
main(void)
{
  Message Msg;
  Message * msg = &Msg;
  result_t result;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  static char responseHeader[] = 
    "\015Cache-Control" "\0\010no-cache"
    "\014Content-Type"  "\0\030application/octet-stream"
    "\0";
  char * responseHeaderCursor = responseHeader;
  int responseHeaderLength = sizeof(responseHeader);
  capros_Logfile_RecordID lastID = capros_Logfile_nullRecordID;

  DEBUG(init) kdprintf(KR_OSTREAM, "LogHTTP: initialized\n");

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
    msg->snd_w1 = 0;
    msg->snd_w2 = 0;
    msg->snd_w3 = 0;
    // msg->snd_code has no default.

    switch (msg->rcv_code) {

    default:
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;

    case OC_capros_key_getType:
      msg->snd_w1 = IKT_capros_HTTPRequestHandler;
      break;

    case OC_capros_key_destroy:
    {
      Sepuku(RC_OK);
      /* NOTREACHED */
    }

    case 0:	// OC_capros_HTTPRequestHandler_headers
      if (msg->rcv_sent > msg->rcv_limit) {	// sent too much data
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      // Ignore the headers.
      msg->snd_w1 = bufSize;
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
      msg->snd_w1 = 200;	// OK response code
      // The length is not known in advance, so use chunked:
      msg->snd_w2 = capros_HTTPRequestHandler_TransferEncoding_chunked;
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
      uint32_t lenGotten;
      uint32_t dataLimit = msg->rcv_w1;
      if (bufSize < dataLimit)	// take min
        dataLimit = bufSize;
      result = capros_Logfile_getNextRecords(KR_Logfile,
                 lastID, dataLimit, (uint8_t *)buf, &lenGotten);
      switch (result) {
      default:
      case RC_capros_Logfile_NoRecord:
        lenGotten = 0;
        break;
      case RC_OK:
        assert(lenGotten);	// must have at least one record
        // Get length of last record.
        uint32_t recLen = *(buf + (lenGotten / sizeof(uint32_t)) - 1);
        // Get header of last record.
        capros_Logfile_recordHeader * rh = (capros_Logfile_recordHeader *)
          (buf + ((lenGotten - recLen) / sizeof(uint32_t)));
        // Get highest ID gotten.
        lastID = rh->id;
        break;
      }
      msg->snd_len = lenGotten;
      msg->snd_data = buf;
      break;
    }
    }
  }
}
