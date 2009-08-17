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

/* Define sizes of stuff */
#define HTTP_BUFSIZE 4096

#ifdef SELF_TEST

#include <stdio.h>

#define DBGPRINT fprintf
#define DBGTARGET stdout

#else // SELF_TEST

#include <eros/Invoke.h>
#include <domain/CMME.h>

#include <domain/domdbg.h>
#include <domain/assert.h>
#include <idl/capros/File.h>

#define DBGPRINT kprintf
#define DBGTARGET KR_OSTREAM

/* Key registers */
#define KR_SOCKET     KR_CMME(0) /* The socket for the connection */
#define KR_RTC        KR_CMME(1) /* The RealTime Clock key */
#define KR_DIRECTORY  KR_CMME(2) /* The IndexedKeyStore aka file directory */
#define KR_FILESERVER KR_CMME(3) /* The file creator object */
#define KR_FILE       KR_CMME(4) /* The "file" key */

#endif // SELF_TEST

struct bio_st;	// from <openssl/ssh.h>
struct ssl_st;	// from <openssl/err.h>

/* DEBUG stuff */
#define dbg_init	0x01   /* debug initialization logic */
#define dbg_sslinit     0x02   /* debug SSL/TLS session set up */
#define dbg_netio       0x04   /* debug network I/O operations */
#define dbg_http        0x08   /* debug HTTP transactions */
#define dbg_file        0x10   /* debug "file" I/O */
#define dbg_errors      0x20
#define dbg_resource	0x40	// HTTPResource interactions
/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors)

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))

#define Method_OPTIONS 0
#define Method_GET 1
#define Method_HEAD 2
#define Method_POST 3
#define Method_PUT 4
#define Method_DELETE 5
#define Method_TRACE 6
#define Method_CONNECT 7

/* Internal object interfaces */
typedef struct {
  struct bio_st * network; // The BIO to move data between ssl and the network
  struct ssl_st * plain;   // The SSL object to pass plain text to/from
  int current;         /* Index of the current character in the buffer */
  int last;            /* Index of the last character in the buffer+1 */
  char buf[HTTP_BUFSIZE]; /* Buffered characters (if any) */
} ReaderState;

typedef struct {
  char *first;         /* First available char from SSL */
  char *last;          /* Last+1 available character from SSL */
} ReadPtrs;

// Red-black tree stuff.

#define TREEKEY char*
typedef struct Treenode {
  struct Treenode *left;
  struct Treenode *right;
  struct Treenode *parent;
  int color;
  TREEKEY key;
  char *value;
} Treenode;
#define TREENODE Treenode
#define RB_TREE
#include <rbtree/tree.h>

extern TREENODE * rqHdrTree;

extern const char * methodList[];

#ifdef SELF_TEST

extern int theFile;

#else

// File is in KR_FILE.
extern capros_File_fileLocation theFileCursor;
extern capros_File_fileLocation theFileSize;

#endif

int readExtend(ReaderState *rs, ReadPtrs *rp);
void readConsume(ReaderState *rs, char *first);
int memcmpci(const char *a, const char *b, int len);
int writeStatusLine(ReaderState *rs, int statusCode);
int writeString(ReaderState *rs, char *str);
int writeSSL(ReaderState *rs, void *data, int len);
int writeMessage(ReaderState *rs, char *msg, int isHEAD);
int sendUnchunked(ReaderState * rs, int (*readProc)(void *, int));
bool sendChunked(ReaderState * rs, int (*readProc)(void *, int));

int handleFile(ReaderState * rs,
  ReadPtrs * rp,
  int methodIndex,
  unsigned long long contentLength,
  int expect100
  );

int handleHTTPRequestHandler(ReaderState * rs,
  ReadPtrs * rp,
  int methodIndex,
  int headersLength,
  unsigned long long contentLength,
  unsigned long theSendLimit,
  int expect100);
