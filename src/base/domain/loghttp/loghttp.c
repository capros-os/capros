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

#include <string.h>
#include <time.h>
#include <eros/Invoke.h>

#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/Logfile.h>
#include <idl/capros/HTTPRequestHandler.h>
#include <idl/capros/SWCA.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <domain/Runtime.h>
#include <domain/ProtoSpaceDS.h>

#include "constituents.h"

//#define BIGTEST 18000	// just generate a large amount of data

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

static inline void
minEquals(unsigned long var, unsigned long val)
{
  if (var > val) var = val;
}

#define KR_OSTREAM	KR_APP(0)
#define KR_Logfile      KR_ARG(0)	// from construction

#define dbg_init    0x1

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0 )

#define DEBUG(x) if (dbg_##x & dbg_flags)

#define maxFormats 6
enum {
  ft_binary24,
  ft_32,
  ft_16,
  ft_u16,
  ft_16x2,
  ft_leds
};
uint32_t formatType;

// Format time as yyyy/mm/dd hh:mm:ss
static void
FormatTime(uint8_t * p, char * * forPP)
{
  time_t tim = ((capros_Logfile_recordHeader *)p)->rtc;
  struct tm * rt = gmtime(&tim);
  *forPP += sprintf(*forPP, "%.4d/%.2d/%.2d %.2d:%.2d:%.2d",
             rt->tm_year+1900, rt->tm_mon+1, rt->tm_mday,
             rt->tm_hour, rt->tm_min, rt->tm_sec);
}

void
Sepuku(result_t retCode)
{
  capros_Node_getSlot(KR_CONSTIT, KC_PROTOSPC, KR_TEMP0);

  /* Invoke the protospace to destroy us and return. */
  protospace_destroy_small(KR_TEMP0, retCode);
  // Does not return here.
}

#define rcvBufSize 4096
uint32_t rcvBuf[rcvBufSize / sizeof(uint32_t)];
#define sndBufSize 4096
uint8_t sndBuf[sndBufSize+1];	// +1 for NUL from sprintf

char responseHeader[80];
char * responseHeaderCursor = responseHeader;
void
AddHeader(const char * name, const char * value)
{
  size_t nameLen = strlen(name);
  size_t valueLen = strlen(value);
  *responseHeaderCursor++ = strlen(name);
  *responseHeaderCursor++ = valueLen >> 8;
  *responseHeaderCursor++ = valueLen & 0xff;
  memcpy(responseHeaderCursor, name, nameLen);
  responseHeaderCursor += nameLen;
  memcpy(responseHeaderCursor, value, valueLen);
  responseHeaderCursor += valueLen;
}

int
main(void)
{
  Message Msg;
  Message * msg = &Msg;
  result_t result;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  capros_Node_getSlot(KR_CONSTIT, KC_FORMATTYPE, KR_TEMP0);
  result = capros_Number_get32(KR_TEMP0, &formatType);
  assert(result == RC_OK);

  // Build the appropriate response headers.
  AddHeader("Cache-Control", "no-cache");
  AddHeader("Content-Type",
            formatType == ft_binary24 ? "application/octet-stream"
                                      : "text/plain" );
  int responseHeaderLength = responseHeaderCursor - responseHeader;
  responseHeaderCursor = responseHeader;

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
  msg->rcv_data = rcvBuf;
  msg->rcv_limit = rcvBufSize;
#ifdef BIGTEST
  int bytesSent = 0;
#endif

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
      msg->snd_w1 = rcvBufSize;
      break;

    case 2:	// OC_capros_HTTPRequestHandler_trailer
      if (msg->rcv_sent > msg->rcv_limit) {	// sent too much data
        msg->snd_code = RC_capros_key_RequestError;
        break;
      }
      // Ignore the trailers.
      msg->snd_w1 = rcvBufSize;
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
      uint32_t formattedLimit = msg->rcv_w1;
      // Take min of how much we can send and how much the caller can receive.
      minEquals(formattedLimit, sndBufSize);
      static unsigned int binarySize[maxFormats] = {24,24,24,24,24,24};
#define ts 19	// max size of formatted time
#define v32 11	// max size of formatted uint32_t
#define v16 6	// max size of formatted uint16_t
      static unsigned int formattedMaxSize[maxFormats] = {
        24,
        ts+1+v32+1,
        ts+1+v16+1,
        ts+1+v16+1,
        ts+1+v16+1+v16+1,
        ts+(1+1)*8+1
      };
#undef ts
#undef v32
#undef v16
      unsigned int formattedRecs
        = formattedLimit / formattedMaxSize[formatType];
      assert(formattedRecs);	// else this algorithm won't work
      unsigned int binSize = formattedRecs * binarySize[formatType];
      minEquals(binSize, rcvBufSize);
      uint8_t * rawP = (uint8_t *)rcvBuf;
      result = capros_Logfile_getNextRecords(KR_Logfile,
                 lastID, binSize, rawP, &lenGotten);
#ifdef BIGTEST
      int bytesWanted = BIGTEST - bytesSent;
      if (bytesWanted > 0) {
        lenGotten = binSize;
        bytesSent += binSize;
        result = RC_OK;
      } else {
        result = RC_capros_Logfile_NoRecord;
      }
#endif
      switch (result) {
      default:
      case RC_capros_Logfile_NoRecord:
        break;

      case RC_OK:
        assert(lenGotten);	// must have at least one record
        uint8_t * endRaw = rawP + lenGotten;
        // Get length of last record.
        uint32_t recLen = *((uint32_t *)endRaw - 1);
        // Get header of last record.
        capros_Logfile_recordHeader * rh = (capros_Logfile_recordHeader *)
                                           (endRaw - recLen);
        // Get highest ID gotten.
        lastID = rh->id;

        // Format the records.
        char * forP = (char *)sndBuf;
        while (rawP < endRaw) {
          capros_Logfile_LogRecord16 * rec16;
          capros_Logfile_LogRecord32 * rec32;
          switch (formatType) {
          case ft_binary24:
            memcpy(forP, rawP, 24);
            forP += 24;
            rawP += 24;
            break;

          case ft_32:
            FormatTime(rawP, &forP);
            rec32 = (capros_Logfile_LogRecord32 *)rawP;
            forP += sprintf(forP, "\t%d\n", rec32->value);
            rawP += 24;
            break;

          case ft_16:
            FormatTime(rawP, &forP);
            rec16 = (capros_Logfile_LogRecord16 *)rawP;
            forP += sprintf(forP, "\t%d\n", rec16->value);
            rawP += 24;
            break;

          case ft_u16:
            FormatTime(rawP, &forP);
            rec16 = (capros_Logfile_LogRecord16 *)rawP;
            forP += sprintf(forP, "\t%u\n", (uint16_t)rec16->value);
            rawP += 24;
            break;

          case ft_16x2:
            FormatTime(rawP, &forP);
            rec16 = (capros_Logfile_LogRecord16 *)rawP;
            forP += sprintf(forP, "\t%d\t%d\n", rec16->value, rec16->param);
            rawP += 24;
            break;

          case ft_leds:
          {
            FormatTime(rawP, &forP);
            capros_SWCA_LEDLogRecord * ledrec
              = (capros_SWCA_LEDLogRecord *)rawP;
            int i;
            unsigned int leds = ledrec->LEDsSteady;
            for (i = 0; i < 8; i++) {
              forP += sprintf(forP, "\t%d", (leds & 0x80) >> 7);
              leds <<= 1;
            }
            forP += sprintf(forP, "\n");
            rawP += 24;
            break;
          }
          }
        }
        msg->snd_len = forP - (char *)sndBuf;
        msg->snd_data = sndBuf;
        break;
      }
      break;
    }
    }
  }
}
