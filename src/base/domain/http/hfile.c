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

#endif // SELF_TEST

/**
 * destroyFile - Destroy the file created for writing.
 */
static void
destroyFile(void)
{
#ifdef SELF_TEST
  // FIXME
#else
  // FIXME
  //  capros_IndexedKeyStore_delete(KR_DIRECTORY, strlen(name), (uint8_t*)name);
  capros_key_destroy(KR_FILE);
#endif
}

/**
 * closeFile - Close the file. Only one file at a time can be open.
 *
 * @return is 1 if the file is closed, 0 if there was an error.
 */
static int
closeFile(void) {
#ifdef SELF_TEST
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Close theFile\n");
  return close(theFile);
#else
  return 1;
#endif
}

/**
 * getFileLen - The length of the file opened for reading.
 *
 * @return is the length of the file, or 0 if the data source doesn't
 *         give a total length (e.g. for dynamicly generated data). This
 *         choice for living with an unsigned result means that zero length
 *         files will be transfered with "chunked" transfer-encoding.
 */
static uint64_t
getFileLen(void) {
#ifdef SELF_TEST
  struct stat sb; 
  
  fstat(theFile, &sb);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Get file size %d\n", (int)sb.st_size);
  return 0;
  //  return sb.st_size;
#else
  result_t rc;
  rc = capros_File_getSize(KR_FILE, &theFileSize);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Read file size rc=%d size=%llu\n",
		       rc, theFileSize);
  if (rc != RC_OK)
    assert(false);	//TODO handle this
  return theFileSize;
#endif
}

/**
 * readFile - Read the open file.
 *
 * @param[in] buf is a pointer to the read buffer
 * @param[in] len is the length to read;
 *
 * @return is -1 if an error occured, 0 at EOF, otherwise length read.
 */
static int
readFile(void *buf, int len) {
#ifdef SELF_TEST
  int rc = read(theFile, buf, len);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Read file rc=%d\n", rc);
  return rc;
#else
  result_t rc;
  uint32_t size = 0;

  if (theFileCursor >= theFileSize) return 0;
  rc = capros_File_read(KR_FILE, theFileCursor, len, buf, &size);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Read file rc=%d numRead=%d\n",
		       rc, size);
  theFileCursor += size;
  if (RC_OK == rc) return size;
  else return -1;
#endif
}

/**
 * writeFile - Write the open file.
 *
 * @param[in] buf is a pointer to the write buffer
 * @param[in] len is the length to read;
 *
 * @return is -1 if an error occured, otherwise the number of bytes written.
 */
static int
writeFile(void *buf, int len) {
#ifdef SELF_TEST
  int rc = write(theFile, buf, len);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Write file rc=%d\n", rc);
  return rc;
#else
  result_t rc;
  uint32_t size = 0;

  rc = capros_File_write(KR_FILE, theFileCursor, len, buf, &size);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Write file rc=%d numWritten=%d\n",
		       rc, size);
  theFileCursor += size;
  if (RC_OK == rc) return size;
  else return -1;
#endif
}

// Returns 1 if OK, 0 if error.
int
handleFile(ReaderState * rs,
  ReadPtrs * rp,
  int methodIndex,
  unsigned long long contentLength,
  int expect100
  )
{
#ifdef SELF_TEST
  // lookUpSwissNumber has opened the file and left the handle in theFile.
#else
  /* The HTTPResource has indicated the object is a File
     and we should handle it. It has left the key to the file in KR_FILE. */
#endif
  
  /* Now do the method */
  switch (methodIndex) {
  case Method_GET:          /* Handle a GET method */
  case Method_HEAD:         /* Handle a HEAD method */
    {
      unsigned long int len; /* The file length */
      char cl[128];
      int isUsingChunked = 0;
      
      len = getFileLen();
      writeStatusLine(rs, 200);
      if (len == 0) {      /* Length N/A, use chunked transfer encoding */
        isUsingChunked = 1;
        writeString(rs, "Transfer-Encoding: chunked\r\n");
      } else {            /* Have length - use Content-length */
        sprintf(cl, "Content-Length: %ld\r\n", len);
        writeString(rs, cl);
      }
      // TODO Consider what Cache-Control directives to issue, if any
      // Since we don't know the content type, let the client decide.
      // writeString(rs, "Content-Type: text/html\r\n");
      // writeString(rs, "Content-Type: application/octet-stream\r\n");
      writeString(rs, "\r\n");	// end of headers
      if (Method_GET == methodIndex) { /* GET, not HEAD - send message-body */
        /*  Actually send the file */
        if (isUsingChunked) {
          // Send chunked data, no trailers, and terminating CRLF
          if (! sendChunked(rs, &readFile)
              || ! writeString(rs, "\r\n") ) {        // write error
            return 0; /* Kill the connection */
          }
        } else {
          if (! sendUnchunked(rs, &readFile)) {        	// write error
            return 0; /* Kill the connection */
          }
        }
        closeFile();
      }
      DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: FILE GET/HEAD done.\n");
    }
    break;

  case Method_PUT:             /* Handle the PUT method */
    {
      int len;
      
      if (expect100) {
        writeStatusLine(rs, 100);
        writeSSL(rs, "\r\n", 2);
      }
      /*  Actually receive the file */
      
      while ( contentLength > 0) {
        if (!readExtend(rs, rp)) {
          /* Network I/O error */
          destroyFile();
          return 0; /* Kill the connection */
        }
        len = rp->last - rp->first;
        if (contentLength < len) len = contentLength;
        len = writeFile(rp->first, len);
        if (len < 0) {      /* File write error */
          writeStatusLine(rs, 500);
          writeMessage(rs, "File I/O error on write.", 0);
          destroyFile();
          return 0;         /* Need to get back in sync with client */
        }
        contentLength -= len;
        readConsume(rs, rp->first+len);
      }
      closeFile();
      writeStatusLine(rs, 200);
      writeString(rs, "Content-Length: 0\r\n");
      writeSSL(rs, "\r\n", 2);
      //      writeMessage(rs, "File Received", 0);
    }
    break;
  default: {
    char msg[128];
    
    sprintf(msg, "Method %s not handled", methodList[methodIndex]);
    DBGPRINT(DBGTARGET, "%s\n", msg);
    writeStatusLine(rs, 500);    /* Internal server error */
    writeMessage(rs, msg, methodIndex==1);
    
  }
  }
  return 1;
}
