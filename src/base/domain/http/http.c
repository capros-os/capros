/*
 * Copyright (C) 2009, 2011, Strawberry Development Group.
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

#include <stddef.h>
#ifdef SELF_TEST
typedef int bool;
#define false 0
#define true 1

#else
#include <eros/target.h>
#include <domain/assert.h>
#include <domain/ProtoSpaceDS.h>

#include <idl/capros/key.h>
#include <idl/capros/Process.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Range.h>
#include <idl/capros/HTTP.h>
#include <idl/capros/HTTPResource.h>
#include <idl/capros/TCPSocket.h>
#include <idl/capros/Node.h>
#include <idl/capros/Discrim.h>
#include <idl/capros/RTC.h>
#include <idl/capros/IndexedKeyStore.h>
#include <idl/capros/FileServer.h>
#include <domain/CMME.h>
#include <domain/CMMEMaps.h>

/* Constituent node contents are defined in HTTP.idl. */
#endif
#include "http.h"

/* OpenSSL stuff */
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#ifdef SELF_TEST
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
//typedef unsigned long int uint32_t;
typedef uint32_t capros_RTC_time_t;
int listen_socket;
int sock;
typedef struct {
  int snd_code;
} Message;
#else

unsigned long __rt_stack_size = 7*4096;

result_t sockRcvLastError = RC_OK;

long mapReservation;
/* Map a memory space up to one page in size. */
/* Uses KR_TEMP0. */
static void *
MapSegment(cap_t seg)
{
  result_t result = maps_mapPage_locked(mapReservation, seg);
  assert(RC_OK == result);
  return maps_pgOffsetToAddr(mapReservation);
}

#endif

char *rsaKeyData = NULL;
char *certData = NULL;
char *defaultPageData = 
"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n"
"   \"http://www.w3.org/TR/html4/loose.dtd\">\n"
"<html>\n"
"<head>\n"
"<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n"
"<title></title>\n"
"</head>\n"
"<body>\n"
"<noscript>\n"
"<h1>This page requires a Javascript enabled web browser.</h1>\n"
"</noscript>\n"
"<script type=\"text/javascript\">\n"
"var urlref = /(.*)#(.*)/.exec(window.location.href);\n"
"if (urlref) {\n"
"    var fragment = /(.*)&=/.exec(urlref[2]);\n"
"    if (fragment) {\n"
"        urlref[2] = fragment[1];\n"
"    }\n"
"    var url = urlref[1] + (/\\?/.test(urlref[1]) ? '&' : '?') + urlref[2];\n"
"    var http;\n"
"    if (window.XMLHttpRequest) {\n"
"        http = new XMLHttpRequest();\n"
"    } else {\n"
"        http = new ActiveXObject('MSXML2.XMLHTTP.3.0');\n"
"    }\n"
"    http.open('GET', url, true);\n"
"    http.onreadystatechange = function () {\n"
"        if (4 !== http.readyState) { return; }\n"
"\n"
"        window.document.write(http.responseText);\n"
"        window.document.close();\n"
"    };\n"
"    http.send(null);\n"
"}\n"
"</script>\n"
"</body>\n"
"</html>\n"
;

/* Internal routine prototypes */
static uint32_t connection(void);
static int setUpContext(SSL_CTX *ctx);
static void print_SSL_error_queue(void);
static void push_ssl_data(BIO *network_bio);
void readInit(SSL *ssl, BIO *network_bio, ReaderState *rs);
int readExtend(ReaderState * rs);
static void readSkipDelim(ReaderState *rs, ReadPtrs *rp);
static int readToken(ReaderState *rs, ReadPtrs *rp, char *sepStr);
static int process_http(SSL *ssl, BIO *network_bio, ReaderState *rs);
static char *findSeparator(ReadPtrs *rp, char *sepStr);
static int compareToken(ReadPtrs *rp, const char *list[], int ci);
static int lookUpSwissNumber(char *swissNumber, int methodCode,
			     int lengthOfPath, char *pathAndQuery);
static void scanConsumeSeps(ReadPtrs *rp, char *sepStr);
static void scanToken(ReadPtrs *rp, char *sepStr);


/* RB Tree stuff */
#define ERR_FIL DBGTARGET
#define ERROR_PRINTF(x) DBGPRINT x
#define VERB_FIL DBGTARGET
#define VERB_PRINTF(x) DBGPRINT x
#ifdef SELF_TEST
#define assert(expression)  \
  ((void) ((expression) ? 0 : \
   DBGPRINT(DBGTARGET, "%s:%d: failed assertion `" #expression "'\n", \
            __FILE__, __LINE__ ) ))
#else
#include <domain/assert.h>
#endif

TREENODE * rqHdrTree = TREE_NIL; // A rbtree with the request headers and values

int tree_compare(TREENODE *a, TREENODE *b) {
  return tree_compare_key(a, b->key);
}
int tree_compare_key(TREENODE *a, TREEKEY b) {
  int la = strlen(a->key);
  int lb = strlen(b);
  int cr =  memcmpci(a->key, b, (la<lb?la:lb));
  if (cr) return cr;
  if (la == lb) return 0;
  if (la < lb) return -1;
  return 1;
}
#include <rbtree/tree_validate.c>
#include <rbtree/tree_util.c>
#include <rbtree/tree_init.c>
#include <rbtree/tree_insert.c>
#include <rbtree/tree_find.c>
#include <rbtree/tree_min.c>
//#include <rbtree/tree_remove.c>
#include <rbtree/tree_succ.c>
//#include <rbtree/tree_contains.c>
static TREENODE *
free_tree(TREENODE *tree) {
  if (TREE_NIL == tree) return TREE_NIL;
  tree->left = free_tree(tree->left);
  tree->right = free_tree(tree->right);
  free(tree->key);
  free(tree->value);
  free(tree);
  return TREE_NIL;
}



#ifndef SELF_TEST
int
cmme_main(void)
{
  result_t rc;
  Message msg;

  capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_OStream, KR_OSTREAM); // for debug

  DEBUG(init) DBGPRINT(DBGTARGET, "HTTP: Starting.\n");

  rc = maps_init();
  assert(rc == RC_OK);	// TODO handle
  mapReservation = maps_reserve_locked(1);

  capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_RTC, KR_RTC); 

  msg.snd_invKey = KR_RETURN;
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_len = 0;
  msg.snd_code = RC_OK;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  SEND(&msg);

  connection();

  DEBUG(init) DBGPRINT(DBGTARGET, "HTTP: Closing socket. Last sock err = %#x\n",
                       sockRcvLastError);

  // If sockRcvLastError == Void or Restart, the connection is already gone,
  // but calling close is harmless.
  capros_TCPSocket_close(KR_SOCKET);
  DEBUG(init) DBGPRINT(DBGTARGET, "HTTP: Closed socket\n");

  /* Now we must wait until we receive RemoteClosed. 
     Don't abort() or destroy(); that may cause data we've sent to be lost. */
  while (1) {
    switch (sockRcvLastError) {
    case RC_OK: ;
      // Receive data until the other end closes.
      uint32_t got;
      uint8_t buf[40];
      sockRcvLastError
        = capros_TCPSocket_receiveLong(KR_SOCKET, 40, &got, 0, buf);
      DEBUG(init) DBGPRINT(DBGTARGET, "HTTP: Final socket rc=%#x len=%u.\n",
                           sockRcvLastError, got);
      continue;
 
    default:
    case RC_capros_key_Void:
      DEBUG(init) DBGPRINT(DBGTARGET,
                           "HTTP: Receive did not get RemoteClosed\n");
    case RC_capros_TCPSocket_RemoteClosed:
    case RC_capros_key_Restart:
      break;
    }
    break;
  }

  maps_fini();
  return 0;
#else
int
main(void)
{
    int fd = open("privkey.pem", O_RDONLY, 0);
    struct stat sb; 
    struct sockaddr_in inad;
    int rc;
    socklen_t adrlen;

    fstat(fd, &sb);
    rsaKeyData = malloc(sb.st_size+1);
    read(fd, rsaKeyData, sb.st_size);
    close(fd);
    rsaKeyData[sb.st_size] = 0;
    printf("len=%d\n%s\n", strlen(rsaKeyData), rsaKeyData);

    fd = open("cacert.pem", O_RDONLY, 0);
    fstat(fd, &sb);
    certData = malloc(sb.st_size+1);
    read(fd, certData, sb.st_size);
    close(fd);
    certData[sb.st_size] = 0;
    printf("len=%d\n%s\n", strlen(certData), certData);
    inad.sin_family = AF_INET;
    inad.sin_port = htons(5001);
    inad.sin_addr.s_addr = INADDR_ANY;

    listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    rc = bind(listen_socket, (struct sockaddr *)&inad, sizeof(inad));
    if (0 != rc) {
      printf("Error on bind, rc=%d, errno=%d %s\n", rc, errno, strerror(errno));
      return 1;
    }
    printf("Listening on socket\n");
    rc = listen(listen_socket, 10);
    if (0 != rc) {
      printf("Error on listen, rc=%d, errno=%d %s\n", rc, errno, strerror(errno));
      return 1;
    }
    memset((char *)&inad,0,sizeof(inad));
    adrlen = sizeof(inad);
    sock = accept(listen_socket, (struct sockaddr *)&inad, &adrlen);
    if (sock < 0) {
      printf("Error on accept, rc=%d, errno=%d %s\n", sock, errno, strerror(errno));
      return 1;
    }
    printf("Connection accepted\n");

    connection();
  close(sock);
  return 0;
#endif
}

static uint32_t
connection(void)
{
  SSL_CTX *sslContext;
  SSL *ssl;
  //  SSL_METHOD *meth = SSLv23_server_method();
  BIO *internal_bio;        /* Data to/from SSL/TLS */
  BIO *network_bio;         /* Mate of internal_bio, data to/from network */
  unsigned long rc;         /* To hold the return code from SSL calls */
  ReaderState rs;

  /* Copy the TCPSocket key */
#ifndef SELF_TEST
  capros_Process_getKeyReg(KR_SELF, KR_ARG(0), KR_SOCKET);
#endif

  DEBUG(init) DBGPRINT(DBGTARGET, "HTTP: received connection\n");

  /* Initialize the SSL library */
  SSL_load_error_strings();       /* readable error messages */
  SSL_library_init();             /* initialize library */
  
  sslContext = SSL_CTX_new(SSLv23_server_method());
  if (sslContext == NULL) {
    DBGPRINT(DBGTARGET, "HTTP: Failed to SSL_CTX_new()\n");
    print_SSL_error_queue();
    return 0;
  }

  /* Initialize the context with the server private key and certificate */
  if (!setUpContext(sslContext)) {
    return 0;
  }
  ssl = SSL_new(sslContext);

  /* We now set up two BIOs for input and output between the SSL library
     and the TCPSocket key. */
  BIO_new_bio_pair(&internal_bio, 0, &network_bio, 0);

  SSL_set_bio(ssl, internal_bio, internal_bio);

  /*
   * We now start to feed data between the OpenSSL black box, the TCP
   * connection, and the code in this process which acts as an HTTP server.
   *
   * There are two pipes, the input pipe which comes from the browser at the
   * other end of the connection and the output which is sent to the
   * connection. The OpenSSL black box sits between the TCP access and the
   * HTTP server, straddling both pipes. Generally, is passes input from 
   * TCP through to HTTP, after decrypting it, although during SSL/TLS setup, 
   * it may generate output messages of its own. For output from HTTP, it
   * generally passes it to TCP after encrypting it, although renegotition
   * may require it to read input before passing the output.
   *
   * This process is driven by looping on the SSL calls, and responding
   * to their returns by filling/emptying the appropriate pipes. At the 
   * recomendation of the OpenSSL documentation, we query the BIO connections
   * about what is pending, as well as depend on the SSL call returns.
   */

  /* Everything set up - Accept the connection - (Build crypto) */
  while ((rc = SSL_accept(ssl))) {
    int err = SSL_get_error(ssl,rc);
    if (SSL_ERROR_NONE == err) break;
    else if (SSL_ERROR_WANT_WRITE == err || SSL_ERROR_WANT_READ == err) {
      DEBUG(sslinit) DBGPRINT(DBGTARGET, "SSL_accept netio needed\n");
      push_ssl_data(network_bio);
      continue;
    } else {
      print_SSL_error_queue();
      return 0;
    }
  }
  /* TODO Check rc for session set up correctly */
  DEBUG(sslinit) {
    char desc[128];
    SSL_CIPHER *ciph = SSL_get_current_cipher(ssl);
    if (NULL == ciph) {
      strcpy(desc, "None");
    } else {
      SSL_CIPHER_description(ciph, desc, sizeof(desc));
    }
    DBGPRINT(DBGTARGET, "SSL description: %s\n", desc);
  }

  readInit(ssl, network_bio, &rs);

  while (process_http(ssl, network_bio, &rs)) { /* Process html until error */
    push_ssl_data(network_bio);
  }
  push_ssl_data(network_bio);   /* Push out last messages */

  /* Termination logic */
  SSL_free(ssl);		/* implicitly frees internal_bio */
  BIO_free(network_bio);        /* We have to free the network_bio */
  
  print_SSL_error_queue();
  return 1;
}


/**
 * setUpContext - Put the server private key and certificate into the context
 *                and perform other setup operations.
 *
 * @param[in] ctx is the SSL context to use.
 *
 * @return is 1 for context set up, 0 for failure.
 */
static int
setUpContext(SSL_CTX *ctx) {
  RSA *key;
  X509 *x509;
  BIO *bio;

  DEBUG(init) DBGPRINT(DBGTARGET, "HTTP: SetUpContext\n");
  /* Read a private key from a BIO using the pass phrase "" */
#ifndef SELF_TEST
  capros_Node_getSlotExtended(KR_CONSTIT, capros_HTTP_KC_RSAKey, KR_TEMP1);
  rsaKeyData = MapSegment(KR_TEMP1);
#endif
  bio = BIO_new_mem_buf(rsaKeyData, -1); // new bio data len via strlen
  key = PEM_read_bio_RSAPrivateKey(bio, NULL, 0, NULL);
  BIO_free(bio);
  if (key == NULL) {
    DBGPRINT(DBGTARGET, "HTTP: Private key decode failed\n");
    print_SSL_error_queue();
    return 0;
  }
  
  if (!SSL_CTX_use_RSAPrivateKey(ctx, key)) {
    DBGPRINT(DBGTARGET, "HTTP: Private key load failed\n");
    print_SSL_error_queue();
    return 0;
  }
  
  /* Seed the ssl library random number generator */
  /* Particularly paranoid people may not like this --
   * so provide your own random seeding before calling this */
  while (!RAND_status()) {
    capros_RTC_time_t time;
    DEBUG(init) DBGPRINT(DBGTARGET, "HTTP: Seeding random number generator\n");
    RAND_seed(rsaKeyData, strlen(rsaKeyData));
#ifndef SELF_TEST
    capros_RTC_getTime(KR_RTC, &time);
#endif
    RAND_seed((void*)&time, sizeof(time));
  }

  /* Install the server certificate */
#ifndef SELF_TEST
  capros_Node_getSlotExtended(KR_CONSTIT, capros_HTTP_KC_Certificate, KR_TEMP1);
  certData = MapSegment(KR_TEMP1);
#endif
  bio = BIO_new_mem_buf(certData, -1); // new bio data len via strlen
  x509 = PEM_read_bio_X509(bio, NULL, 0, NULL);
  BIO_free(bio);
  SSL_CTX_use_certificate(ctx, x509);

  if (!SSL_CTX_check_private_key(ctx)) {
    DBGPRINT(DBGTARGET, "HTTP: Private key does not match certificate.\n");
    print_SSL_error_queue();
    return 0;
  }
  return 1;
}

/**
 * print_SSL_error_queue - Print the contents of the SSL error queue
 */
void
print_SSL_error_queue(void) {
  unsigned long errorNumber;
  while ((errorNumber = ERR_get_error())) {
    DBGPRINT(DBGTARGET, "%s\n", ERR_error_string(errorNumber, NULL));
  }
}


/**
 * push_ssl_data - Move data between the network_bio and the network.
 *                 Return is when one set of output has been sent, and
 *                 if there has been a read, one read has been done.
 *
 * @param[in] network_bio is the network side of the BIO pair.
 */
void push_ssl_data(BIO *network_bio) {
  size_t inBuf;                /* Amount of data buffered/needed */
  uint32_t got;
  int tryAgain = 1;
#ifndef SELF_TEST
  uint32_t rc;
#endif

  while (tryAgain) {
    tryAgain = 0;
    inBuf = BIO_ctrl_pending(network_bio);  /* Test for output */
    if (inBuf > 0) {
      DEBUG(netio) DBGPRINT(DBGTARGET, "HTTP: out len=%d\n", inBuf);

      unsigned char buf[inBuf];
      
      BIO_read(network_bio, buf, inBuf);
#ifndef SELF_TEST
      /* TODO set the push flag only if network_bio has no more to send */
      rc = capros_TCPSocket_sendLong(KR_SOCKET, inBuf, 
   				     capros_TCPSocket_flagPush, buf);
      if (RC_OK != rc) {
        DEBUG(errors) DBGPRINT(DBGTARGET,
                               "HTTP: TCPSocket_send returned %#x\n", rc);
	(void)BIO_shutdown_wr(network_bio);
	break;
      }
#else
      got = write(sock, buf, inBuf);
      if (got != inBuf) {
	printf("Error on write, sent=%d, actual=%d errno=%d %s\n",
	       inBuf, got, errno, strerror(errno));
	(void)BIO_shutdown_wr(network_bio);
	break;
      }
#endif
      tryAgain = 1;
    }
    inBuf = BIO_ctrl_get_read_request(network_bio); /* Ask how big a read */
    if (inBuf > 0) {
      DEBUG(netio) DBGPRINT(DBGTARGET, "HTTP: in req len=%d\n", inBuf);

      unsigned char buf[inBuf];
      
#ifndef SELF_TEST
      rc = capros_TCPSocket_receiveLong(KR_SOCKET, inBuf, &got, 0, buf);
      if (RC_OK != rc) {
        sockRcvLastError = rc;
        DEBUG(errors) DBGPRINT(DBGTARGET,
                               "HTTP: TCPSocket_receive returned %#x\n", rc);
	(void)BIO_shutdown_wr(network_bio);
	break;
      }
#else
      got = read(sock, buf, inBuf);
      if (got < 1) {
	printf("Error on read, rc=%d, errno=%d %s\n",
	       got, errno, strerror(errno));
	(void)BIO_shutdown_wr(network_bio);
	break;
      }      
#endif
      tryAgain = 1;
      BIO_write(network_bio, buf, got);
    }
  }
}

/* The following variables are used both by process_http and other routines */
#ifdef SELF_TEST
int theResourceType;

#define capros_HTTPResource_RHType_HTTPRequestHandler 0
#define capros_HTTPResource_RHType_MethodNotAllowed 1
#define capros_HTTPResource_RHType_File 2

#define capros_HTTPResource_maxLengthOfPathAndQuery 512

int theFile;
#else
// File is in KR_FILE.
capros_File_fileLocation theFileCursor;
capros_File_fileLocation theFileSize;
capros_HTTPResource_RHType theResourceType;
uint32_t theSendLimit;
#endif

/* The following variables are used by process_http. They are defined outside
   process_http so a common freeStorage routine may be called to free the
   dynamically allocated data assigned to them. */
char *authority = NULL;
char *pathandquery = NULL;
char *headerName = NULL;

static void
freeStorage(void) {
  if (authority) {
    free(authority);
    authority = NULL;
  }
  if (pathandquery) {
    free(pathandquery);
    pathandquery = NULL;
  }
  if (headerName) {
    free(headerName);
    headerName = NULL;
  }
  free_tree(rqHdrTree);
  rqHdrTree = TREE_NIL;
}

int
sendUnchunked(ReaderState * rs, int (*readProc)(void *, int))
{
  char buf[2048];
  int len;

  while ((len = (*readProc)(buf, sizeof(buf))) > 0) {
    if (!writeSSL(rs, buf, len))
      return false;
  }
  return len == 0;
}

bool	// returns true iff successful, false if write error
sendChunked(ReaderState * rs, int (*readProc)(void *, int))
{
  char buf[2048];
  int len;
  char cl[19];

  while (1) {
    len = (*readProc)(buf, sizeof(buf));
    if (len < 0)
      return false;
    sprintf(cl, "%x\r\n", len);
    if (! writeString(rs, cl))
      return false;
    if (0 == len)
      return true;
    if (!writeSSL(rs, buf, len))
      return false;
    if (! writeString(rs, "\r\n"))
      return false;
  }
}

#define MethodEntry(name) [Method_##name] = #name
const char * methodList[] = {
  MethodEntry(OPTIONS),
  MethodEntry(GET),
  MethodEntry(HEAD),
  MethodEntry(POST),
  MethodEntry(PUT),
  MethodEntry(DELETE),
  MethodEntry(TRACE),
  MethodEntry(CONNECT),
  NULL };

/** process_http - Process the HTTP protocol available on the ssl connection 
 *
 * @param[in] ssl Is the ssl object
 * @param[in] network_bio is the bio to feed data to/from the network
 *
 * @return is a code indicating what should happen next:
 *      0 - Shut down the ssl/tcp connection
 *      1 - Process another HTTP message
 */
static int 
process_http(SSL *ssl, BIO *network_bio, ReaderState *rs) {
  /* Parse the HTTP request. The request is terminated by a cr/lf/cr/lf
     sequence. */
  //  char *breakChars = "()<>@,;:\\\"/[]?={} \t\r\n";
  ReadPtrs rp = {rs->buf + rs->current, rs->buf + rs->current};
  int mustClose = 0;    /* We must close the connection after response */
  int methodIndex;
  int versionIndex;
  char * swissNumber;
  //  int headerIndex = -1;

#ifndef SELF_TEST
#if capros_HTTPResource_Method_OPTIONS != Method_OPTIONS
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#if capros_HTTPResource_Method_GET != Method_GET
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#if capros_HTTPResource_Method_HEAD != Method_HEAD
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#if capros_HTTPResource_Method_POST != Method_POST
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#if capros_HTTPResource_Method_PUT != Method_PUT
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#if capros_HTTPResource_Method_DELETE != Method_DELETE
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#if capros_HTTPResource_Method_TRACE != Method_TRACE
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#if capros_HTTPResource_Method_CONNECT != Method_CONNECT
#error filename line-num methodIndex does not match HTTPResource header index
#endif
#endif

  static const
    char *versionList[] = {"HTTP/1.1", NULL};
  /* static const
     char *headerList[] = {"User-Agent", "Host", "Accept", "Accept-Language",
     "Accept-Encoding", "Accept-Charset", "Keep-Alive",
     "Connection", "Cache-Control", "Expect",
     "Content-Encoding", "Content-Length", NULL};
  */
  static const
    char *connectionList[] = {"close", NULL};
  static const
    char *expectationList[] = {"100-continue", NULL};
  char c;
  char *value;
  int namelen, valuelen;
  TREENODE *existing = TREE_NIL;
  TREENODE *node;
  int pathLength;  
  int headersLength = 0;
  
  /* The following are results of parsing the HTTP headers */
#define MAX_HEADER_LENGTH 4096 /* Arbitrary maximum length for a header */
  TREENODE *last = TREE_NIL;   /* The last node used, for header continuation */
  unsigned long long contentLength = 0;
  int expect100 = 0;
  
  /* TODO skip leading blank lines */
  if (!readToken(rs, &rp, " \n")) return 0;
  if (*rp.last != ' ' && *rp.last != '\t') {
    DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bad request format\n");
    writeStatusLine(rs, 400);  /* Return Bad Request */
    writeMessage(rs, "Request not followed by space or tab", 0);
    return 0;                  /* Must get back in sync with client */ 
  }
  methodIndex = compareToken(&rp, methodList, 0);
  if (-1 == methodIndex) {
    char errMsg[120];
    int i,j;

    DEBUG(http) DBGPRINT(DBGTARGET, "HTTP: request not recognized %.*s\n",
			 rp.last - rp.first, rp.first);
    writeStatusLine(rs, 501); /* Return Not Implemented */
    strcpy(errMsg, "Method ");
    i = rp.last-rp.first;
    j = strlen(errMsg);
    if (i < 20) {
      memcpy(errMsg+j, rp.first, i);
      errMsg[j+i] = 0;
    } else {
      memcpy(errMsg+j, rp.first, 17);
      errMsg[j+17] = 0;
      strcat(errMsg, "...");
    }
    strcat(errMsg, " not implemented");    
    writeMessage(rs,errMsg, 0);
    return 0;                  /* Must get back in sync with client */ 
  }

  // TODO handle absolute URI format (no one sends it yet)
  /* Get the URI part of the request */
  size_t authorityLen;
  if (!readToken(rs, &rp, " \n")) return 0; /* Read the authority */
  c = *rp.last;
  switch (c) {                       /* See what we found */
  case ' ':                          /* Found a space - save authority */
    authorityLen = rp.last - rp.first;
    authority = malloc(authorityLen + 1);
    memcpy(authority, rp.first, authorityLen);
    authority[authorityLen] = 0;
    assert(authorityLen == strlen(authority));
    break;
  default:                                    /* Newline etc bad request */
    DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bad request (newline etc)\n");
    writeStatusLine(rs, 400);  /* Return Bad Request */
    writeMessage(rs, "HTTP version missing.", methodIndex==1);
    return 0;                  /* Must get back in sync with client */ 
  }

  /* Break the authority into the components we need, which are the path,
     the query (less question mark separator, the Swiss number portion, 
     and the Swiss number. */
  {
    char * queryEnd = authority + authorityLen;

    char *delim = strchr(authority, '?');
    if (delim) {	// there is a '?'
      pathLength = delim - authority;
      delim++;		// skip over the '?'
    } else {		// no query
      pathLength = authorityLen;
      delim = queryEnd;
    }

    pathandquery = malloc(authorityLen + 2); /* max len <= len(authority)
				+ perhaps "/" + terminating NUL */
    /* Save the path */
    if (pathLength) {
      memcpy(pathandquery, authority, pathLength);
    } else {                     /* path is empty */
      pathandquery[0] = '/';
      pathLength = 1;
    }
    pathandquery[pathLength] = 0;	// terminating NUL

    /* Extract the Swiss number from the query. The Swiss number will be
       the s=number parameter. These keyword parameters are separated by
       either the ampersand (&) character or the semicolon (;) character.
       The Swiss number will be the last s= if there are more than one
       s= in the query. */
    swissNumber = NULL;		// default, if we don't find one
    if (*delim) {		// there is a query
      /* Replace the NUL at the end of the query with '&',
         so that every keyword definition *ends* with '&'. */
      *queryEnd++ = '&';

      /* Search backwards until we find a s=value definition. */
      char * tail = queryEnd;
      char * p;
      char * where;
      do {
        p = tail;
        where = tail - 1;	// skip '&'
        assert(*where == '&' || *where == ';');
        while (1) {		// scan backwards looking for '&'
          if (where == delim)
            break;		// at the beginning
          char * prev = where - 1;
          char c = *prev;
          if (c == '&' || c == ';') {	// treat ';' like '&'
            break;
          }
          where = prev;
        }
        // Found "name=value&" between where and tail.
        if (*where == 's' && *(where+1) == '=') {
          // Found "s=value&"
          swissNumber = where+2;
          *(tail-1) = 0;	// terminate with NUL (replacing '&')
          break;
        }
        tail = where;
      } while (where != delim);
      /* Copy the query, minus the "s=value&", to pathandquery. */
      char * query = pathandquery + pathLength; // end of path, beg of query
      // Copy the part before "s=value&".
      memcpy(query, delim, where - delim);
      where = query + (where - delim);
      // Copy the part after "s=value&".
      memcpy(where, tail, queryEnd - tail);
      where += queryEnd - tail;
      if (where != pathandquery)	// if there are any "n=v&" left
        where--;		// delete the final '&' we added
      *where = 0;		// terminating NUL for pathandquery

      /* HTTP 1.1 must handle % encoding of the Swiss number, but we may
	 foil a few attacks by not handling that, and our Swiss numbers do
	 not include characters that need to be % encoded. */
    }
    // else no query, leave swissNumber NULL
  }

  /* Get the http version */
  if (!readToken(rs, &rp, " \n\r")) return 0;
  versionIndex = compareToken(&rp, versionList, 1);
  if (-1 == versionIndex) {
    DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: version not supported\n");
    writeStatusLine(rs, 505);  /* Return HTTP Version Not Supported*/
    writeMessage(rs, "This server only supports HTTP/1.1", methodIndex==1);
    freeStorage();
    return 0;                  /* Must get back in sync with client */ 
  }

  /* We should now have a CRLF as the next item */
  if (!readToken(rs, &rp, "\n")) {
    freeStorage();
    return 0;                  /* Must get back in sync with client */ 
  }
  if (rp.last-rp.first > 1) {
    freeStorage();
    writeStatusLine(rs, 400);  /* Return Bad Request */
    writeMessage(rs, "HTTP version not followed by CRLF on request line",
		 methodIndex==1);
    return 0;                  /* Must get back in sync with client */ 
  }
  readSkipDelim(rs, &rp);

  tree_init();   /* Initialize the dummy RB tree node */

  /* Process the message headers */
  while (1) {
    if (!readToken(rs, &rp, ":\n")) {
      freeStorage();
      return 0;
    }
    if ((rp.last - rp.first == 0) /* LF only, not kosher, but accepted */
	|| (rp.last - rp.first == 1 && *rp.first == '\r')) {
      /* Clear out the LF from the blank line after the headers */
      readSkipDelim(rs, &rp);
      break;    /* Reached end of the headers */
    }

    /* A header may be continued by having a line starting with SP or HT. All
       CRLF, HT, SP should be replaced with one SP before interpretation. */
    if (*rp.first == ' ' || *rp.first =='\t') { /* Continued header */
      if (TREE_NIL == last) {
	/* Error */
	freeStorage();
	writeStatusLine(rs, 400);
	writeMessage(rs, "Continuation line without header", methodIndex==1);
	return 0;         /* Get back in sync with client */
      }
      existing = last;    /* Continue the last node processed */
    }

    /* We are now pointed at either the character after the :, which may be
       whitespace or a parameter, or at the leading whitespace in a 
       continuation line. */

    /* save the header name */
    namelen = rp.last - rp.first;
    if (namelen <= 0 || namelen > 255) {
      freeStorage();
      writeStatusLine(rs, 500);
      writeMessage(rs, "Header name length longer than 255 or is zero",
		   methodIndex==1);
      return 0;         /* Get back in sync with client */
    }
    headersLength += namelen+2;   /* Acculminate total length of headers */
    headerName = malloc(namelen+1);
    memcpy(headerName, rp.first, namelen); /* Don't copy the trailing : */
    headerName[namelen] = 0;
    readSkipDelim(rs, &rp);          /* Found a ':' - skip it */

    /* save the header value */
    if (!readToken(rs, &rp, "\r\n")) {
      freeStorage();
      return 0;
    }  // TODO truncate : and leading and trailing whitespace
    while ((*rp.first == ' ' || *rp.first == '\t') && rp.first<rp.last) {
      rp.first++;
    }
    valuelen = rp.last - rp.first;
    headersLength += valuelen + 3;
    if (valuelen <= 0
	|| valuelen > MAX_HEADER_LENGTH
	|| headersLength > 0xffff) {
      writeStatusLine(rs, 413);   /* Say the entity is too large */
      writeMessage(rs, "Header too long", methodIndex==1);
      freeStorage();
      return 0;     /* Ignoring warnings in the RFC to make sure zapping
		       the connection doesn't destroy the response */
    }
    value = malloc(valuelen+1);
    memcpy(value, rp.first, valuelen);
    value[valuelen] = 0;

    if (TREE_NIL == existing) existing = tree_find(rqHdrTree, headerName);
    if (TREE_NIL == existing ) {
      /* Insert the result into the RB tree */
      node = malloc(sizeof(TREENODE));
      node->left = TREE_NIL;
      node->right = TREE_NIL;
      node->parent = TREE_NIL;
      node->color = TREE_BLACK;
      
      node->key = headerName;
      headerName = NULL;       /* Don't free active entry */
      node->value = value;
      rqHdrTree = tree_insert(rqHdrTree, node);
    } else {
      /* Append the data to the existing entry */
      char *newValue = malloc(strlen(existing->value)+strlen(value)+1);
      strcpy(newValue, existing->value);
      strcat(newValue, value);
      free(existing->value);
      existing->value = newValue;
      free(value);
      free(headerName);
    }
     /* clean out to next CRLF */
    if (!readToken(rs, &rp, "\n")) { 
      freeStorage();
      return 0;
    }
    readSkipDelim(rs, &rp);
  }     /* End of while(1) to read all the headers */
  

  /* We've collected all the headers, now parse the ones we need */

  /* The general headers: */

  /* "Cache-Control" */
  // We don't do caching, so we can ignore any Cache-Control directives.

  /* "Connection" */
  // We must handle Connection: close, and close the connection after 
  // our response.
  {
    TREENODE *node = tree_find(rqHdrTree, "Connection");
    ReadPtrs p;
    
    if (TREE_NIL != node) {
      p.first = node->value;
      p.last = p.first + strlen(p.first);
      while (1) {
	int i;
	
	scanToken(&p, ", \t\r\n");
	i = compareToken(&p, connectionList, 1);
	if (0==i) mustClose = 1;
	p.first = p.last;
	p.last += strlen(p.last);
	if (*p.first == 0) break;
      }
    }
  }

  /* "Date" */
  // Only valid for POST and PUT, and then optional. It is for cache 
  // control, and we MUST send date headers when we get a good clock.

  /* "Pragma" */
  // Kind of a fuzzily (by practice) defined HTTP 1.0 function for 
  // specifing "no-cache". May be used by a client to request an
  // uncached response. Doesn't effect servers.

  /* "Trailer" */
  // Indicates which headers may be included in the trailer portion of
  // each chunk in chunked transfers.

  /* "Transfer-Encoding" */
  // Used to indicated "chunked" transfers.

  /* "Upgrade" */
  // Designed for changing protocols on the existing connection. We don't.

  /* "Via" */
  // Gives the path of proxys the request passed over. May be useful for
  // debugging.

  /* "Warning" */
  // It looks like this header is server to client only.


  /* The request headers: */

  /* "Accept" */
  // We really should make sure our application/octet-stream and 
  // text/html are acceptable - someday
  // if we don't see application/octet-stream | text/* | */*, we 
  // should give 406

  /* "Accept-Charset" */
  // We don't know character sets from adam. RFC2616 says we should send
  // 406 if we don't have a response in an acceptable character set, 
  // although sending some unacceptable character set is also allowed.
  // So we'll send what we have.
 
  /* "Accept-Encoding" */
  /* Values are case insensitive */
  // We should check for "identity" with a q=0 saying it is unacceptable.
  // Hey, life is rough all over, so take the identity encoding, 'cus
  // that's all we know. We should be giving a 406.
  
  /* "Accept-Language" */
  // We have no idea what human language the response is in. It may be
  // binary for all we know. Our server responses are in English.
  // Hope you like it.

  /* "Authorization" */
  // We don't do HTTP style client authorization. We use web keys.
  // Since we don't issue 401 (unauthorized) we shouldn't see this
  // header.

  /* "Expect" */
  // We need to send a interum response of 100 if we get 100-continue
  {
    TREENODE *node = tree_find(rqHdrTree, "Expect");
    ReadPtrs p;
    
    if (TREE_NIL != node) {
      p.first = node->value;
      p.last = p.first + strlen(p.first);
      while (1) {
	int i;
	
	scanToken(&p, ", \t\r\n");
	i = compareToken(&p, expectationList, 1);
	if (i == 0) {
	  expect100 = 1;
	} else {
	  writeStatusLine(rs, 417);
	  writeSSL(rs, "\r\n", 2);
	  freeStorage();
	  return 0;                  /* Must get back in sync with client */ 
	}
	p.first = p.last;
	p.last += strlen(p.last);
	if (*p.first == 0) break;
      }
    }
  }

  /* "From" */
  // The user's email address -- A spammer's dream.

  /* "Host" */
  // Gives the host name from the URI requested. Used for multi-host
  // on a single IP. TODO Servers MUST reject requests which do not
  // have a host header with 400.

  /* "If-Match" */
  // This header is used with entity tags, which we do not yet support.
  // Since there are a bunch of MUSTs included with IF-Match, we TODO
  // need to support entity tags to avoid update based on stale data
  // via PUT.

  /* "If-Modified-Since" */
  // Servers SHOULD give a not modified response, which we don't.

  /* "If-None-Match" */
  // See If-Match.

  /* "If-Range" */
  // We don't support sub-range operations so we MUST ignore this header.

  /* "If-Unmodified-Since" */
  // See If-Modified_Since.

  /* "Max-Forwards" */
  // Only used with TRACE and OPTIONS methods which we don't support.

  /* "Proxy-Authorization" */
  // Issued only in response to a Proxy-Authenticate header which we
  // don't send.

  /* "Range" */
  // Used to request a sub-range of the resource. MAY be ignored by
  // the server, which we do.

  /* "Referer" */
  // Allows the server to learn what URI contained the URI being 
  // requested. WE SHOULD NOT LOG THIS HEADER, as we may be logging
  // web keys.

  /* "TE" */
  // Used to request compression of the response. We don't.

  /* "User-Agent" */
  // Information about the client software.

 
  /* The entity headers: */

  /* "Allow" */
  // The allow header may be used by a client to give a hint to usage
  // of resources uploaded with a "PUT" request. Server's don't have
  // to support those methods, so we do what we do and ignore "Allow".
  
  /* "Content-Encoding" */
  // Content-Encoding values are case insensitive
  // We only accept "identity", so don't even look for it
  // TODO we should check for other than identity on PUT requests.

  /* "Content-Language" */
  // We might see one of these on a PUT or POST. Think about it if we
  // ever grow up to understand that there are multiple natural languages.

  /* "Content-Length" */
  // We need this header for upload length
  {  
    int len;
    TREENODE *node = tree_find(rqHdrTree, "Content-length");
    
    if (TREE_NIL != node) {
      len = strlen(node->value);
      if (len > 15 || len == 0) {
	writeStatusLine(rs, 413);   /* Say the entity is too large */
	writeMessage(rs, "File too long for upload", methodIndex==1);
	freeStorage();
	return 0;     /* Ignoring warnings in the RFC to make sure zapping
			 the connection doesn't destroy the response */
      }
      contentLength = atoll(node->value);
    }
  }

  /* "Content-Location" */
  // I (wsf) am not entirely sure what this header is meant to do. It 
  // looks like Barbie-Doll standards making where someone thought it
  // would be useful and the rest of the standards committee rolled over
  // and played dead to expidite the process. Since it goes with the
  // resource, and is undefined in PUT or POST requests, we can safely
  // ignore it.

  /* "Content-MD5" */
  // This header MAY be used as a checksum of the content of a PUT or
  // POST request. We MAY check it, but we don't.

  /* "Content-Range" */
  // This header is used with sub-range requests which we don't now
  // support. We MAY send the whole resource with a 200, which we do.

  /* "Content-Type" */
  // I suppose we could get one of these with a PUT request, but we
  // ignore it.

  /* "Expires" */
  // It looks like this header is only server to client.

  /* "Last-Modified" */
  // It looks like this header is server to client only. We SHOULD be
  // sending it.


  /* extension headers encountered: */
  
  /* "Keep-Alive" */
  // This header is triggered by "Connection: keep-alive" from Firefox.


  /* Check that length of path and query is in range */
  if (capros_HTTPResource_maxLengthOfPathAndQuery < strlen(pathandquery)) {
    writeStatusLine(rs, 400);
    writeMessage(rs, "Path + Query too long.", methodIndex==1);
    freeStorage();
    return 0;
  }
  
  assert(rp.first == rp.last && rp.first == rs->buf + rs->current);
  // Done with rp.

  /* We've read all the headers and are ready to do the method. First
     we look up the Swiss number and see if we are serving a file, or 
     if some domain will handle the request */

  if (swissNumber) {
    if (!lookUpSwissNumber(swissNumber, methodIndex, 
			   pathLength, pathandquery)) {
      /* Swiss number not found or other error */
      writeStatusLine(rs, 404);
      writeMessage(rs, "File not found on server.", methodIndex==1);
      freeStorage();
      return 0;
    }
    
    switch (theResourceType) {
    case capros_HTTPResource_RHType_HTTPRequestHandler:
#ifdef SELF_TEST
      DBGPRINT(DBGTARGET, "ERROR: RHType == HTTPRequestHandler found\n");
#else
      {
        DEBUG(resource) DBGPRINT(DBGTARGET,
                                 "lookUpSwissNumber gave HTTPRequestHandler\n");
        int ok = handleHTTPRequestHandler(rs, methodIndex,
                   headersLength, contentLength, theSendLimit, expect100);
        capros_key_destroy(KR_FILE);	// done with the HTTPRequestHandler
        if (!ok) {
          DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: hrh returned 0\n");
          freeStorage();
          return 0;
        }
      }	// end of case capros_HTTPResource_RHType_HTTPRequestHandler
#endif
      break;

    case  capros_HTTPResource_RHType_MethodNotAllowed:
      writeStatusLine(rs, 405); 
      writeMessage(rs, "Method Not Allowed", methodIndex==1);
      freeStorage();
      break;
        
    case capros_HTTPResource_RHType_File:
      if (! handleFile(rs, methodIndex, contentLength, expect100)) {
        DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: hfile returned 0\n");
        freeStorage();
        return 0;
      }
      break;
    }
  } else {
    /* No Swiss number. Serve a default page. */
    switch (methodIndex) {
    case Method_GET:
    case Method_HEAD:
      {
        unsigned long int len = strlen(defaultPageData);
        char cl[32];
        
        writeStatusLine(rs, 200);
        sprintf(cl, "Content-Length: %ld\r\n", len);
        writeString(rs, cl);

        // TODO Consider what Cache-Control directives to issue, if any
        writeString(rs, "Content-Type: text/html\r\n\r\n");
        if (Method_GET == methodIndex) { /* not HEAD - send body */
          /*  Actually send the file */
          if (!writeSSL(rs, defaultPageData, len)) {
            freeStorage();
            return 0;
          }
        }
      }
      break;
    default:
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bad request (no query)\n");
      writeStatusLine(rs, 400);  /* Return Bad Request */
      writeMessage(rs, "URI query field missing.", methodIndex==1);
      return 0;                /* Must get back in sync with client */ 
    }
  }

  /* Push any queued data to the network */
  freeStorage();
  if (mustClose) return 0;
  return 1;
  
  /* Required support for http 1/1
     chunked transfer encoding
     request line methods: GET <resource> HTTP 1.1
     HEAD <resource> HTTP 1.1 {Like GET but no data}
     Host: <dnsName> {Must be present. Can ignore value of dnsName}
     Expect: 100-continue must be supported, other Expects give 417 error
     TE: Look for chunked;q=0
     Transfer-encoding: look for chunked;q=0
     
     Include response headers:
     Cache-control: no-cache, no-store
     Connection: close {If closing the connection after response}
     Content-length: <nnn> {Or used chunked transfer}
     Server: CapROS-HTTP 0.0.1
  */
}


/**
 * readToken - Read a token from the input stream
 *
 * @param[in] rs is the ReaderState for the connection
 * @param[in] rp is a ReadPtrs. It is assumed that it points to a previous
 *               token, which will be skipped over.
 * @param[in] sepStr is a string of separator characters which delimit the
 *            token. Leading separator characters will be skipped and consumed.
 *
 * @return is:
 *   0 if there was an error. *rp may not be updated.
 *   1 if no error. If return is 1, rp->first
 *         will point to the token, and rp->last will point to the next
 *         separator. The characters from rp->first to rp->last-1 inclusive
 *         are the token.
 */
static int
readToken(ReaderState *rs, ReadPtrs *rp, char *sepStr) {
  char *cp;

  readConsume(rs, rp->last);

  // Consume any leading separators:
  while (1) {
    if (readEnsure(rs) == 0)
      return 0;
    char c = rs->buf[rs->current];
    if (!strchr(sepStr, c)) {
      break;
    }
    rs->current++;	// Consume the leading separator
  }

  for (;;) {
    rp->first = rs->buf + rs->current;
    rp->last = rs->buf + rs->last;
    cp = findSeparator(rp, sepStr);
    if (cp) break;
    if (!readExtend(rs))
      return 0;
  }
  rp->last = cp;

  /* Check the token for invalid characters (assume separators are valid)*/
  for (cp=rp->first; cp<rp->last; cp++) {
    unsigned char c = *cp;
    if (!(c <= 0x7e && c>= 0x20) && (c!=0x0a && c!=0x0d)) { 
      writeStatusLine(rs, 200);
      writeMessage(rs, "Invalid character found in HTTP protocol data.", 0);
      DEBUG(http) DBGPRINT(DBGTARGET, "BadChar: %x\n", c);
      return 0;
    }
  }
  return 1;
}


/**
 * scanToken - Scan a token from a string
 *
 * @param[in] rp is a ReadPtrs. It is assumed that it points to a previous
 *               token, which will be skipped over.
 * @param[in] sepStr is a string of separator characters which delimit the
 *            token. Leading separator characters will be skipped and consumed.
 *            The NUL at end of string always stops the scan.
 *
 *         When scanToken returns, rp->first will point to the next token,
 *         and rp->next will point to the next separator. The characters 
 *         from rp->first to rp->last-1 inclusive are the token.
 */
static void
scanToken(ReadPtrs *rp, char *sepStr) {
  char *cp;

  scanConsumeSeps(rp, sepStr);
  cp = findSeparator(rp, sepStr);
  if (!cp) {
    rp->last = rp->first + strlen(rp->first);
  } else {
    rp->last = cp;
  }
}

/**
 * compareToken - Compare a token with a list of valid ones and return the
 *                tokens index in the list.
 * 
 * @param[in] rp is a pointer to a ReadPtr structure defining the token
 * @param[in] list is an array of pointers to strings, the last is NULL
 * @param[in] ci is non-zero (TRUE) to make the comparison case independent.
 * 
 * @return is the index in the list, or -1 if it is not in the list.
 */
static int
compareToken(ReadPtrs *rp, const char *list[], int ci) {
  int i;
  int len = rp->last - rp->first;
  
  for (i=0; list[i]; i++) {
    if (strlen(list[i]) == len) {
      if (ci) {
	if (0 == memcmpci(rp->first, list[i], len)) return i;
      } else {
	if (0 == memcmp(rp->first, list[i], len)) return i;
      }
    }
  }
  return -1;
}


/**
 * memcmpci - Case independent memory field comparison. This routine 
 *            assumes US-ASCII aka ANSI X3.4-1986.
 *
 * @param[in] a is a pointer to the first field to compare
 * @param[in] b is a pointer to the second field to compare
 * @param[in] len is the length to compare
 *
 * @return -1 if the first different character in a is < the corisponding one
 *            in b, +1 if the first different character in a is > the 
 *            corisponding one in b, or 0 if all characters are equal.
 */
int
memcmpci(const char *a, const char *b, int len) {
  int i;
  static const char tt[] = "abcdefghijklmnopqrstuvwxyz";
  for (i=0; i<len; i++) {
    char aa = a[i];
    char bb = b[i];
    if (aa>='A' && aa<='Z') aa = tt[aa-'A'];
    if (bb>='A' && bb<='Z') bb = tt[bb-'A'];
    if (aa<bb) return -1;
    if (aa>bb) return 1;
  }
  return 0;
}


/**
 * findSeparator - Find the next separator in the input stream. This routine
 *                 considers any ASCII ctl or characters > 0x7e to be 
 *                 separators
 *
 * @param[in] rp is the ReadPtrs for the input stream.
 * @param[in] sepStr is a string of separator characters to scan for
 *
 * @return is a pointer to the separator, or NULL if there is none.
 *
 * HTTP token separators are:
 *     token          = 1*<any CHAR except CTLs or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | SP | HT
 */ 
static char *
findSeparator(ReadPtrs *rp, char *sepStr) {
  char *c;

  for (c=rp->first; c<rp->last; c++) {
    if (strchr(sepStr, *c)) return c;
  }
  return NULL;
}


/**
 * readInit - Initialize the network reader
 *
 * @param[in] ssl is the SSL object to read from
 * @param[in] network_bio is the BIO used to move data between the network
 *            and the SSL library.
 * @param[in] is a ReaderState object for this connection.
 */
void
readInit(SSL *ssl, BIO *network_bio, ReaderState *rs) {
  rs->plain = ssl;
  rs->network = network_bio;
  rs->current = 0;
  rs->last = 0;
}

/**
 * readConsume - Indicate that data in the buffer is no longer needed.
 *
 * @param[in] rs is the ReaderState for this connection.
 * @param[in] first is the first byte in the buffer still needed.
 */
void
readConsume(ReaderState *rs, char *first) {
  rs->current = first - rs->buf;
}

/**
 * scanConsumeSeps - Skip over any leading separators.
 * 
 * @param[in] rp is a ReadPtrs.
 * @param[in] first is the first byte in the buffer still needed.
 * @param[in] sepStr is a string of separator characters to scan for
 *
 *         rp will be set to the first non-separator and the amount of 
 *         available data in the string.
 */

static void
scanConsumeSeps(ReadPtrs *rp, char *sepStr) {
  char c;
  
  while (1) {
    c = *rp->first;
    if (0 == c || !strchr(sepStr, c)) {
      rp->last = rp->first + strlen(rp->first);
      return;
    }
    rp->first++;
  }
}

/**
 * readSkipDelim - Skip one character in the input stream. First skips the
 *                 token indicated by the input rp, and then skips the
 *                 next character. These characters must be in the buffer.
 *                 readToken ensures that they are in the buffer.
 *                 On return, rp->first and rp->last will point to the
 *                 character beyond the next character, which may not be
 *                 in the buffer.
 * 
 * @param[in] rs is the ReaderState for this connection.
 * @param[in] rp is a ReadPtrs.
 *
 */

static void
readSkipDelim(ReaderState *rs, ReadPtrs *rp) {
  readConsume(rs, rp->last);        /* Consume the token */
  rs->current++;                    /* Consume the next character */
  rp->first = rp->last = rs->buf + rs->current;
}

/**
 * readExtend - Extend that amount of data in the read buffer
 *
 * @param[in] rs is the ReaderState for the connection.
 *
 * @return is the result of the operation as follows:
 *     0 - An ssl read error has been detected. bail out
 *     1 - (rs->last - rs->current) is greater on exit
 *         than it was on entry.
 */
int
readExtend(ReaderState *rs) {
  int rc;
  
  if (rs->current) {    /* Some data has been consumed, push it down */
    memcpy(rs->buf, rs->buf+rs->current, rs->last-rs->current);
    rs->last -= rs->current;
    rs->current = 0;
  }
  /* Read the HTTP request */
  while (1) {
    int err;
    
#ifdef SHORT_READ
    rc = SSL_read(rs->plain, rs->buf+rs->last, 1);
#else
    rc = SSL_read(rs->plain, rs->buf+rs->last, HTTP_BUFSIZE-rs->last);
#endif
    err = SSL_get_error(rs->plain, rc);
    switch (err) {
    default:
      /* Note, it may be normal to get an error here if the other end
         closes the connection. */
      DEBUG(errors)
        DBGPRINT(DBGTARGET, "HTTP:%d: SSL error %d rc %d\n", __LINE__, err, rc);
      print_SSL_error_queue();
      return 0;

    case SSL_ERROR_WANT_WRITE:
    case SSL_ERROR_WANT_READ:
      DEBUG(netio) DBGPRINT(DBGTARGET, "SSL_read netio needed\n");
      push_ssl_data(rs->network);
      continue;

    case SSL_ERROR_NONE:
      break;
    }
    break;
  }
  assert(rc > 0);
  int bytesToPrint = rc < 500 ? rc : 500;	// limit amount printed
  DEBUG(http) DBGPRINT(DBGTARGET, "Read: %.*s\n",
                       bytesToPrint, rs->buf+rs->last);
  rs->last += rc;
  /* rc is the amount of data read from ssl - sanity check it */
  if (rs->last > HTTP_BUFSIZE || rc < 0) {
    DBGPRINT(DBGTARGET, "Read size error, read=%d, inBuffer=%d\n",
	     rc, rs->last);
    print_SSL_error_queue();
    return 0;
  }
  return 1;
}

/**
 * readEnsure - Ensure there is some data in the read buffer
 *
 * @param[in] rs is the ReaderState for the connection.
 *
 * @return is the number of bytes of data in the buffer.
 * If zero, no data could be read due to an ssl read error.
 */
int
readEnsure(ReaderState * rs) {
  int len = rs->last - rs->current;
  if (len > 0) {
    return len;
  } else {
    if (!readExtend(rs))
      return 0;
    return rs->last;	// rs->current == 0
  }
}


/**
 * writeMessage - Write an error message as HTML to the entity of the response.
 *
 * @param[in] rs is the ReaderState for the connection.
 * @param[in] msg is the message to write.
 * @param[in] isHEAD is 0 unless the request method is HEAD.
 *
 * @return is 0 if an error occured, otherwise 1
 */
int
writeMessage(ReaderState *rs, char *msg, int isHEAD) {
  char output[256];
  char cl[60];
  
  sprintf(output,
	  "<html>\r\n<title>ERROR</title>\r\n<body>\r\n%s\r\n</body>\r\n</html>\r\n",
	  msg);
  sprintf(cl, "Content-Length: %d\r\n", strlen(output));
  if (!writeString(rs, cl)) return 0;
  if (!writeString(rs, "Content-Type: text/html\r\n\r\n")) return 0;
  if (!isHEAD) {       /* not HEAD - send message-body */
    if (!writeString(rs, output)) return 0;
  }
  return 1;
}


/**
 * writeStatusLine - Send the status line to the connection.
 *
 * @param[in] rs is the ReaderState for the connection
 * @param[in] statusCode is the status code.
 *
 * @return is 0 if there was an error, otherwise 1
 */
int
writeStatusLine(ReaderState *rs, int statusCode) {
  char str[256];
  const char *reason = "Unknown";
  int i;

  typedef struct {
    const int status;
    const char *reason;
  } ReasonTableEntry;
  static const ReasonTableEntry rt[] = {
    {100, "Continue"},
    {101, "Switching Protocols"},
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Time-out"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Request Entity Too Large"},
    {414, "Request-URI Too Large"},
    {415, "Unsupported Media Type"},
    {416, "Requested range not satisfiable"},
    {417, "Expectation Failed"},
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Time-out"},
    {505, "HTTP Version not supported"},
    {-1, "end"}};

  for (i=0; -1 != rt[i].status; i++) {
    if (statusCode == rt[i].status) {
      reason = rt[i].reason;
      break;
    }
  }
  sprintf(str, "HTTP/1.1 %d %s\r\n", statusCode, reason);
  if (!writeSSL(rs, str, strlen(str))) return 0;
  return 1;
}

/**
 * writeString - Write a string to the SSL connection
 *
 * @param[in] rs is the ReaderState for the connection
 * @param[in] str is the string to write.
 *
 * @return is 0 if an error occured, 1 otherwise
 */
int
writeString(ReaderState *rs, char *str) {
  return writeSSL(rs, str, strlen(str));
}


/**
 * writeSSL - Write data to the SSL connection.
 * 
 * @param[in] rs is the ReaderState for the connection
 * @param[in] data is a pointer to the data to write
 * @param[in] length is the length of data to write
 *
 * @return is 0 if an error occured, 1 otherwise
 */
int
writeSSL(ReaderState *rs, void *data, int len) {
  int rc;
  
  DEBUG(netio) {
    int prtlen = len;
    if (prtlen > 40)	// limit the number of characters printed
      prtlen = 40;
    DBGPRINT(DBGTARGET, "HTTP SSL write len=%d data=%.*s\n",
             len, prtlen, (char*)data);
  }
  if (0 == len) return 1;  /* SSL_write behavior with length 0 is undefined */
  /* Write to the SSL connection */
  while ((rc = SSL_write(rs->plain, data, len))) {
    int err = SSL_get_error(rs->plain, rc);
    if (SSL_ERROR_NONE == err) break;
    else if (SSL_ERROR_WANT_WRITE == err || SSL_ERROR_WANT_READ == err) {
      DEBUG(netio) DBGPRINT(DBGTARGET, "SSL_write netio needed\n");
      push_ssl_data(rs->network);
      continue;
    } else {
      print_SSL_error_queue();
      return 0;
    }
  }
  return 1;
}


/**
 * lookUpSwissNumber - Look up the Swiss number in the directory of servable
 *                     objects.
 *
 * @param[in] swissNumber is the Swiss number
 *
 * @return is 1 if the Swiss number was found, else 0.
 */
static int
lookUpSwissNumber(char *swissNumber, int methodCode, int lengthOfPath, 
		  char *pathAndQuery) {
  DEBUG(http) DBGPRINT(DBGTARGET,
		       "HTTP lookUpSwissNumber pathlen=%d path+query= %s\n",
		       lengthOfPath, pathAndQuery);
#ifdef SELF_TEST
  /* In SELF_TEST mode, we use the swiss number as a UNIX file name. To keep
     from exposing the whole Unix file system, limit the character
     set of name */
  int i;
  char fullname[strlen(swissNumber) + 256];
  int isRead = (methodCode==Method_GET || methodCode==Method_HEAD);
  
  for (i=0; i<strlen(swissNumber); i++) {
    
    if (NULL == 
	strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
	       swissNumber[i])) {
      DBGPRINT(DBGTARGET, 
	       "Bad character \"%c\" in filename \"%s\"\n", swissNumber[i], 
	       swissNumber);
      theFile = -1;
      return 0;
    }
  }
  strcpy(fullname, "/home/");
  strcat(fullname, getlogin());
  strcat(fullname, "/testdir/");
  strcat(fullname, swissNumber);
  theFile = open(fullname, (isRead ? O_RDONLY : O_WRONLY|O_CREAT),
		 S_IRUSR | S_IWUSR);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Open \"%s\" result %d\n",
		       fullname, theFile);
  if (-1 == theFile) return 0;
  theResourceType = capros_HTTPResource_RHType_File;
  return 1;
#else
  result_t rc;
  uint32_t len = strlen(swissNumber);

  if (len > capros_IndexedKeyStore_maxNameLen) {
    DBGPRINT(DBGTARGET, "Swissnumber too long, len=%d\n", len);
    return 0;
  }
  theFileCursor = 0;
  rc = capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_Directory, KR_DIRECTORY);
  assert(RC_OK == rc);

  rc = capros_IndexedKeyStore_get(KR_DIRECTORY, len, (uint8_t*)swissNumber,
				  KR_FILE);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Get resource key \"%s\" rc=%#x\n",
		       swissNumber, rc);
  if (RC_OK != rc) {
    DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: Directory_get returned %#x\n",
			   rc);
    return 0;
  }
  rc = capros_HTTPResource_request(KR_FILE, (1) * 65536 + (1), methodCode,
				   lengthOfPath, strlen(pathAndQuery),
				   (unsigned char *)pathAndQuery,
				   &theResourceType, KR_FILE, &theSendLimit);

  return 1;
#endif
}
