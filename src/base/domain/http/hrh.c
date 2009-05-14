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

#include "http.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef SELF_TEST

#include <eros/target.h>
#include <idl/capros/key.h>
#include <idl/capros/HTTPRequestHandler.h>

#endif // SELF_TEST

#ifndef SELF_TEST

/* Tuning parameters */

/* Minimum size of buffer for headers from a HTTPRequestHandler */
#define HEADER_BUF_SIZE 2048

char *headerBuffer = NULL;
int headerBufferLength = 0;

void
freeBuffer(void)
{
  if (headerBuffer) {
    free(headerBuffer);
    headerBuffer = NULL;
    headerBufferLength = 0;
  }
}

/** transferHeaders - Transfer headers from the HTTPRequestHandler to the 
 *                    connection.
 */
static int
transferHeaders(ReaderState *rs, result_t (*getProc)(cap_t , uint32_t,
						     uint32_t *, uint8_t *)) {
/* Get additional headers from the handler */
/* We have to stand on our heads and pat our tummies since we can't
   just stream the data to the connection. There is not relation between
   the headers and the chunks of data we get from the handler. Since I 
   (wsf) can't really test this code, the way most likely to work is to
   get all the data, and then format it and send it to the connection */
  
  char *b = headerBuffer;
  int bl = headerBufferLength;
  int ln, lv;
  result_t rc;
  uint32_t headerLength;
 
  while (1) {
    // bl has buffer space left.
    while (bl > 0) {
      rc = getProc(KR_FILE, bl, &headerLength, (unsigned char *)b);
      DEBUG(http) DBGPRINT(DBGTARGET, "HTTP: got headers rc=%#x hl=%d\n",
                           rc, headerLength);
      if (RC_capros_key_UnknownRequest == rc)	// this means no headers
        goto gotHeaders;
      if (RC_OK != rc) {
        DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: getProc rc=%#x\n", rc);
	/* Handler had problems, just die */
	return 0;
      }
      if (0 == headerLength) goto gotHeaders;
      b += headerLength;
      bl -= headerLength;
    }
    bl = headerBufferLength - bl;    /* Amount in the buffer */
    if (! headerBufferLength)
      headerBufferLength = HEADER_BUF_SIZE;
    else
      headerBufferLength *= 2;      /* Size of new buffer, twice old size */
    b = malloc(headerBufferLength);
    memcpy(b, headerBuffer, bl);  /* Copy what we have to new buffer */
    free(headerBuffer);           /* Free old */
    headerBuffer = (char *)b;     /* Point at start of all the headers */
    b += bl;                      /* Place for next byte of header data */
    bl = headerBufferLength - bl; /* Len left is (new len - what we got) */
  }

gotHeaders:
  /* Now put them out as headers */
  bl = b - headerBuffer;
  b = headerBuffer;
#define getByte(p) (*(unsigned char *)(p))
  while (bl) {
    if (bl < 3) {		// too small even for the length bytes
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bl=%d b=%#x\n", bl, b);
      return 0;
    }
    ln = getByte(b);
    lv = (getByte(b+1) << 8) + getByte(b+2);
    bl -= ln +lv + 3;
    if (bl < 0) {
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bl=%d b=%#x ln=%d lv=%d\n",
                             bl, b, ln, lv);
      return 0;
    }
    
    b += 3;
    writeSSL(rs, b, ln);	/* Write it to connection */
    b += ln;
    writeSSL(rs, ": ", 2);                /* Write colon and space */
    writeSSL(rs, b, lv);	/* Write the value */
    b += lv;
    writeSSL(rs, "\r\n", 2);              /* Finish with a CRLF */
  }
  return 1;
}

// @return is -1 if an error occured, 0 at EOF, otherwise length read.
static int
readResponseBody(void * buf, int buflen)
{
  result_t rc;
  uint32_t lengthOfBodyData;
  rc = capros_HTTPRequestHandler_getResponseBody(KR_FILE, 
		 buflen, &lengthOfBodyData,
		 (unsigned char *)buf);
  DEBUG(resource) DBGPRINT(DBGTARGET, "HTTP: got body rc=%#x len=%d\n",
                           rc, lengthOfBodyData);
  if (RC_OK != rc)
    return -1;
  return lengthOfBodyData;
}
#endif

// Returns 1 if OK, 0 if error.
int
handleHTTPRequestHandler(ReaderState * rs,
  ReadPtrs * rp,
  int methodIndex,
  int headersLength,
  unsigned long long contentLength,
  unsigned long theSendLimit,
  int expect100
  )
{
#ifdef SELF_TEST
  writeStatusLine(rs, 500);
  writeMessage(rs, "lookUpSwissNumber gave HTTPRequestHandler response",
		   methodIndex==1);
  return 1;
#else
  result_t rc;
  TREENODE *node;
  /* headersLength is the total length of all headers + space for the
     lengths as per the HTTPRequestHandler protocol. Making the buffer
     this big means we don't have to check for buffer overrun. Since
     we use the buffer for output headers as well, we apply a minimum
     length as well. */
  headerBufferLength = (headersLength+1 < HEADER_BUF_SIZE
			    ? HEADER_BUF_SIZE
			    : headersLength+1);
  headerBuffer = malloc(headerBufferLength);
  unsigned char * bp = (unsigned char *)headerBuffer;
  unsigned char * bufend;
  int kl, vl;
  uint16_t statusCode;
  capros_HTTPRequestHandler_TransferEncoding bodyTransferEncoding;      
  char cl[128];       /* For generating a content length header */

  for (node = tree_min(rqHdrTree); 
       node != TREE_NIL; 
       node = tree_succ(node)) {
    kl = strlen(node->key);
    if ( (kl==17 && memcmpci(node->key, "Transfer Encoding", kl)==0)) {
      continue;  /* Skip the transfer encoding header */
    }
    /* We checked the header name length was < 256 when building the tree */
    *bp++ = kl & 0xff;
    vl = strlen(node->value);
    *bp++ = (vl >> 8) &0xff;
    *bp++ = vl & 0xff;
    memcpy(bp, node->key, kl);
    bp += kl;
    memcpy(bp, node->value, vl);
    bp += kl;
  }
  bufend = bp;

  /* Now send the headers to the handler */
  DEBUG(resource) DBGPRINT(DBGTARGET, "HTTP: sending headers\n");
  bp = (unsigned char *)headerBuffer;
  while (bufend - bp) {
    uint32_t len = (bufend-bp < theSendLimit ? bufend-bp : theSendLimit);
    rc = capros_HTTPRequestHandler_headers(KR_FILE, len, bp, &theSendLimit);
    if (RC_OK != rc) {
      DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: RH_headers got rc=%#x\n", rc);
      writeStatusLine(rs, 500);
      writeMessage(rs, "Bad HTTPRequestHandler.headers response.",
                   methodIndex==1);
      goto errorExit;
    }
    bp += len;
  }

  /* If there is a 100-continue expectation, get result from handler */
  if (expect100) {
    DEBUG(resource) DBGPRINT(DBGTARGET, "HTTP: expect 100\n");
    rc = capros_HTTPRequestHandler_getContinueStatus(KR_FILE, &statusCode);
    if (RC_capros_key_UnknownRequest == rc) {
      statusCode = 100;
    } else if (RC_OK != rc) {
      writeStatusLine(rs, 500);
      writeMessage(rs,
    	   "Bad HTTPRequestHandler.getContinueStatus response.",
    	   methodIndex==1);
      goto errorExit;
    }
    writeStatusLine(rs, statusCode); 
    writeSSL(rs, "\r\n", 2);
    if (statusCode != 100) {
      writeMessage(rs, "HTTPRequestHandler response for 100-continue.",
    	   methodIndex==1);
      goto errorExit;
    }
  }

  /* Now transfer the body of the request */
  DEBUG(resource)
    DBGPRINT(DBGTARGET, "HTTP: sending body %lld\n", contentLength);
  //TODO handle chunked transfers.
  while ( contentLength > 0) {
	int len;

	if (!readExtend(rs, rp)) {
	  /* Network I/O error */
          DEBUG(errors)
            DBGPRINT(DBGTARGET, "HTTP:%d: readExtend failed\n", __LINE__);
	  goto errorExit; /* Kill the connection */
	}
	len = rp->last - rp->first;
	if (contentLength < len) len = contentLength;
	if (len > theSendLimit) len = theSendLimit;
	rc = capros_HTTPRequestHandler_body(KR_FILE, len, 
					    (unsigned char *)rp->first,
					    &theSendLimit);
    DEBUG(http) DBGPRINT(DBGTARGET,
                         "HTTP: Sent body to Resource rc=%#x\n", rc);
	if (RC_OK != rc) {  /* handler error */
	  writeStatusLine(rs, 500);
	  writeMessage(rs, "HTTPRequestHandler error on body.", 0);
	  goto errorExit;     /* Need to get back in sync with client */
	}
	contentLength -= len;
	readConsume(rs, rp->first+len);
  }

  /* Get the response status */
  DEBUG(resource) DBGPRINT(DBGTARGET, "HTTP: getting response status\n");
  rc =  capros_HTTPRequestHandler_getResponseStatus(KR_FILE, &statusCode,
							&bodyTransferEncoding,
							&contentLength);
  assert(RC_OK == rc);	// FIXME
  if (statusCode == 204		// no content
      || statusCode == 304	// not modified
      || statusCode < 200 ) {	// informational
    // These MUST NOT have a body.
    if (bodyTransferEncoding
        != capros_HTTPRequestHandler_TransferEncoding_none) {
      DEBUG(errors) DBGPRINT(DBGTARGET,
                      "HTTP: Response status %d but xfer encoding=%d\n",
                      statusCode, bodyTransferEncoding);
      bodyTransferEncoding = capros_HTTPRequestHandler_TransferEncoding_none;
    }
  } else {
    // These MUST have a body (not sent if HEAD)
    if (bodyTransferEncoding 
        == capros_HTTPRequestHandler_TransferEncoding_none) {
      DEBUG(errors) DBGPRINT(DBGTARGET,
                      "HTTP: Response xfer encoding is none but status=%d\n",
                      statusCode);
      bodyTransferEncoding = capros_HTTPRequestHandler_TransferEncoding_identity;
      contentLength = 0;
    }
  }
  writeStatusLine(rs, statusCode);

  // Generate response headers.
  switch (bodyTransferEncoding) {
  default:
    DEBUG(errors) DBGPRINT(DBGTARGET,
                    "HTTP: got xfer encoding %u\n", bodyTransferEncoding);
  case capros_HTTPRequestHandler_TransferEncoding_identity:
    sprintf(cl, "Content-Length: %ld\r\n", contentLength);
    writeString(rs, cl);
    break;
  case capros_HTTPRequestHandler_TransferEncoding_chunked:
    writeString(rs, "Transfer-Encoding: chunked\r\n");
  case capros_HTTPRequestHandler_TransferEncoding_none:
    break;
  }
  //TODO generate a date header

  /* Get additional headers from the handler */
  if (0 == transferHeaders(rs,
		     capros_HTTPRequestHandler_getResponseHeaderData))
    /* Error. We've already reported a status, so we can't give 500. 
	   just zap the circuit */
    goto errorExit;
  writeSSL(rs, "\r\n", 2);              /* Finish headers with blank line */

  // Transfer the response body.
  DEBUG(resource) DBGPRINT(DBGTARGET, "HTTP: getting body, bte=%d\n",
                           bodyTransferEncoding);
  /* Receive the data from the handler and send it to the connection */
  if (Method_HEAD != methodIndex) {
    switch (bodyTransferEncoding) {
    case capros_HTTPRequestHandler_TransferEncoding_identity:
      if (! sendUnchunked(rs, &readResponseBody))	// write error
        goto errorExit; /* Kill the connection */
      break;
    case capros_HTTPRequestHandler_TransferEncoding_chunked:
      // Send chunked data.
      if (! sendChunked(rs, &readResponseBody))	// write error
        goto errorExit; /* Kill the connection */
      // Send trailers.
      DEBUG(resource)
        DBGPRINT(DBGTARGET, "HTTP: getting trailer headers\n");
      /* Send out any trailer headers */
      if (0 == transferHeaders(rs,
		     capros_HTTPRequestHandler_getResponseTrailer))
        /* Error. We've already reported a status, so we can't give 500. 
	   just zap the circuit */
        goto errorExit;
      // Send CRLF after chunked body and any trailer headers.
      if (! writeString(rs, "\r\n") )	// write error
        goto errorExit;
    case capros_HTTPRequestHandler_TransferEncoding_none:
      break;
    }	// end of switch
  }	// end of if not HEAD
  DEBUG(resource) DBGPRINT(DBGTARGET, "HTTP: resource done\n");
  freeBuffer();
  return 1;

errorExit:
  freeBuffer();
  return 0;
#endif
}
