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

/* Tuning parameters */

/* Minimum size of buffer for headers from a HTTPRequestHandler */
#define HEADER_BUF_SIZE 2048


#include <stddef.h>
#ifndef SELF_TEST
#include <eros/target.h>
#include <eros/Invoke.h>
#include <domain/Runtime.h>
#include <domain/assert.h>
#include <domain/ProtoSpaceDS.h>

#include <idl/capros/Process.h>
#include <idl/capros/GPT.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Range.h>
#include <idl/capros/HTTP.h>
#include <idl/capros/HTTPResource.h>
#include <idl/capros/HTTPRequestHandler.h>
#include <idl/capros/TCPSocket.h>
#include <idl/capros/Node.h>
#include <idl/capros/Discrim.h>
#include <idl/capros/RTC.h>
#include <idl/capros/IndexedKeyStore.h>
#include <idl/capros/File.h>
#include <idl/capros/FileServer.h>

#include <domain/domdbg.h>

/* Constituent node contents */
#include "constituents.h"
#endif

/* OpenSSL stuff */
#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#ifdef SELF_TEST
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
//typedef unsigned long int uint32_t;
typedef uint32_t capros_RTC_time_t;
#define DBGPRINT fprintf
#define DBGTARGET stdout
int listen_socket;
int sock;
typedef struct {
  int snd_code;
} Message;
#else
#define DBGPRINT kprintf
#define DBGTARGET KR_OSTREAM

/* Key registers */
#define KR_SOCKET     KR_APP(1) /* The socket for the connection */
#define KR_RTC        KR_APP(2) /* The RealTime Clock key */
#define KR_DIRECTORY  KR_APP(3) /* The IndexedKeyStore aka file directory */
#define KR_FILESERVER KR_APP(4) /* The file creator object */
#define KR_FILE       KR_APP(5) /* The "file" key */
#define KR_AddrSpace  KR_APP(8)	/* Our address space cap */
#define KR_OSTREAM    KR_APP(9)	/* only used for debugging */

/* Define locations in our address space */
/* Our address space looks like:
0 to 0x400000: a VCS containing our text, data, bss, and stack.
0x400000 to 0x800000: area for mapping segments. */
const uint32_t __rt_stack_pointer = 0x400000;
#define mapAddress 0x400000

void *
MapSegment(cap_t seg)
{
  result_t result = capros_GPT_setSlot(KR_AddrSpace, mapAddress >> 22, seg);
  assert(RC_OK == result);
  return (void *)mapAddress;
}

#endif

char *rsaKeyData = NULL;
char *certData = NULL;
char *defaultPageData = 
"<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\r\n"
"   \"http://www.w3.org/TR/html4/loose.dtd\">\r\n"
"<html>\r\n"
"<head>\r\n"
"<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\r\n"
"<title></title>\r\n"
"</head>\r\n"
"<body>\r\n"
"<noscript>\r\n"
"<h1>This page requires a Javascript enabled web browser.</h1>\r\n"
"</noscript>\r\n"
"<script type=\"text/javascript\">\r\n"
"var urlref = /(.*)#(.*)/.exec(window.location.href);\r\n"
"if (urlref) {\r\n"
"    var fragment = /(.*)&=/.exec(urlref[2]);\r\n"
"    if (fragment) {\r\n"
"        urlref[2] = fragment[1];\r\n"
"    }\r\n"
"    var url = urlref[1] + (/\?/.test(urlref[1]) ? '&' : '?') + urlref[2];\r\n"
"    var http;\r\n"
"    if (window.XMLHttpRequest) {\r\n"
"        http = new XMLHttpRequest();\r\n"
"    } else {\r\n"
"        http = new ActiveXObject('MSXML2.XMLHTTP.3.0');\r\n"
"    }\r\n"
"    http.open('GET', url, true);\r\n"
"    http.onreadystatechange = function () {\r\n"
"        if (4 !== http.readyState) { return; }\r\n"
"\r\n"
"        window.document.write(http.responseText);\r\n"
"        window.document.close();\r\n"
"    };\r\n"
"    http.send(null);\r\n"
"}\r\n";

/* Define sizes of stuff */
#define HTTP_BUFSIZE 4096

/* DEBUG stuff */
#define dbg_init	0x01   /* debug initialization logic */
#define dbg_sslinit     0x02   /* debug SSL/TLS session set up */
#define dbg_netio       0x04   /* debug network I/O operations */
#define dbg_http        0x08   /* debug HTTP transactions */
#define dbg_file        0x10   /* debug "file" I/O */
#define dbg_errors      0x20
/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_init | dbg_sslinit | dbg_netio| dbg_http | dbg_file | dbg_errors )

#define CND_DEBUG(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if (CND_DEBUG(x))


/* Internal object interfaces */
typedef struct {
  BIO *network;        /* The BIO to move data between ssl and the network */
  SSL *plain;          /* The SSL object to pass plain text to/from */
  int current;         /* Index of the current character in the buffer */
  int last;            /* Index of the last character in the buffer+1 */
  char buf[HTTP_BUFSIZE]; /* Buffered characters (if any) */
} ReaderState;

typedef struct {
  char *first;         /* First available char from SSL */
  char *last;          /* Last+1 available character from SSL */
} ReadPtrs;

/* Internal routine prototypes */
#ifndef SELF_TEST
static int processRequest(Message *argmsg);
#endif
static uint32_t connection(void);
static int setUpContext(SSL_CTX *ctx);
static void print_SSL_error_queue(void);
static void push_ssl_data(BIO *network_bio);
void readInit(SSL *ssl, BIO *network_bio, ReaderState *rs);
static int readExtend(ReaderState *rs, ReadPtrs *rp);
static void readConsume(ReaderState *rs, char *first);
static int readConsumeSeps(ReaderState *rs, ReadPtrs *rp, char *first, 
			   char *sepStr);
static void readSkipDelim(ReaderState *rs, ReadPtrs *rp);
static int readToken(ReaderState *rs, ReadPtrs *rp, char *sepStr);
static int process_http(SSL *ssl, BIO *network_bio, ReaderState *rs);
static char *findSeparator(ReadPtrs *rp, char *sepStr);
static int compareToken(ReadPtrs *rp, const char *list[], int ci);
static int memcmpci(const char *a, const char *b, int len);
static int writeSSL(ReaderState *rs, void *data, int len);
static int writeStatusLine(ReaderState *rs, int statusCode);
static int writeString(ReaderState *rs, char *str);
static int writeMessage(ReaderState *rs, char *msg, int isHEAD);
static int lookUpSwissNumber(char *swissNumber, int methodCode,
			     int lengthOfPath, char *pathAndQuery);
static int closeFile(void);
static void destroyFile(void);
static uint64_t getFileLen(void);
static int readFile(void *buf, int len);
static int writeFile(void *buf, int len);
static void scanConsumeSeps(ReadPtrs *rp, char *sepStr);
static void scanToken(ReadPtrs *rp, char *sepStr);
#ifndef SELF_TEST
static int
transferHeaders(ReaderState *rs, result_t (*getProc)(cap_t , uint32_t,
						     uint32_t *, uint8_t *));
#endif


/* RB Tree stuff */
#define ERR_FIL DBGTARGET
#define ERROR_PRINTF(x) DBGPRINT x
#define VERB_FIL DBGTARGET
#define VERB_PRINTF(x) DBGPRINT x
#ifdef SELF_TEST
typedef int bool;
#define assert(expression)  \
  ((void) ((expression) ? 0 : \
   DBGPRINT(DBGTARGET, "%s:%d: failed assertion `" #expression "'\n", \
            __FILE__, __LINE__ ) ))
#define false 0
#define true 1
#else
#include <domain/assert.h>
#endif

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



int
main(void)
{
#ifndef SELF_TEST
  Message msg;

  char buff[256]; // Initial parameters

  capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_OStream, KR_OSTREAM); // for debug
  capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_RTC, KR_RTC); 
  capros_Process_getAddrSpace(KR_SELF, KR_AddrSpace);

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

  /* The following loop is bogus, as there is no start key to us: */

  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_limit = sizeof(buff);
  msg.rcv_data = buff;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  for(;;) {
    RETURN(&msg);

    msg.snd_invKey = KR_RETURN;
    (void) processRequest(&msg);
  }
#else
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
  return 0;
#endif
}

#ifndef SELF_TEST
static int
processRequest(Message *argmsg)
{
  uint32_t code = argmsg->rcv_code;
  // Message msg;

  argmsg->snd_len = 0;
  argmsg->snd_w1 = 0;
  argmsg->snd_w2 = 0;
  argmsg->snd_w3 = 0;
  argmsg->snd_key0 = KR_VOID;
  argmsg->snd_key1 = KR_VOID;
  argmsg->snd_key2 = KR_VOID;
  argmsg->snd_code = RC_OK;

  switch (code) {
  case OC_capros_key_getType: /* Key type */
    {
      argmsg->snd_code = RC_OK;
      argmsg->snd_w1 = IKT_capros_HTTP;
      break;
    }
  case OC_capros_key_destroy:
    {
      capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_ProtoSpace, KR_TEMP0);
      /* Invoke the protospace to destroy us and return. */
      protospace_destroy_small(KR_TEMP0, RC_OK);
      // Does not return here.
    }
  default:
    {
      argmsg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
  }
  
  return 1;
}
#endif

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
  capros_Node_getSlotExtended(KR_CONSTIT, capros_HTTP_KC_RSAKey, KR_TEMP0);
  rsaKeyData = MapSegment(KR_TEMP0);
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
  capros_Node_getSlotExtended(KR_CONSTIT, capros_HTTP_KC_Certificate, KR_TEMP0);
  certData = MapSegment(KR_TEMP0);
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
      unsigned char buf[inBuf];
      
      DEBUG(netio) DBGPRINT(DBGTARGET, "HTTP: out len=%d\n", inBuf);
      BIO_read(network_bio, buf, inBuf);
#ifndef SELF_TEST
      /* TODO set the push flag only if network_bio has no more to send */
      rc = capros_TCPSocket_send(KR_SOCKET, inBuf, 
				 capros_TCPSocket_flagPush, buf);
      if (RC_OK != rc) {
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
      unsigned char buf[inBuf];
      
      DEBUG(netio) DBGPRINT(DBGTARGET, "HTTP: in req len=%d\n", inBuf);
#ifndef SELF_TEST
      rc = capros_TCPSocket_receive(KR_SOCKET, inBuf, &got, 0, buf);
      if (RC_OK != rc) {
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
char *swissNumber = NULL;
char *headerName = NULL;
char *headerBuffer = NULL;
int headerBufferLength = 0;
TREENODE *tree = TREE_NIL;   /* A rbtree to hold the headers and values */

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
  if (swissNumber) {
    free(swissNumber); 
    swissNumber = NULL;
  }
  if (headerName) {
    free(headerName);
    headerName = NULL;
  }
  if (headerBuffer) {
    free(headerBuffer);
    headerBuffer = NULL;
  }
  free_tree(tree);
  tree = TREE_NIL;
}

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
  //  int headerIndex = -1;
  static const
    char *methodList[] = {"OPTIONS", "GET", "HEAD", "POST", "PUT", "DELETE",
			   "TRACE", "CONNECT", NULL};
#define Method_OPTIONS 0
#define Method_GET 1
#define Method_HEAD 2
#define Method_POST 3
#define Method_PUT 4
#define Method_DELETE 5
#define Method_TRACE 6
#define Method_CONNECT 7

  /* Used internally to send a default page of Javascript when there is
     no Swiss number in the query portion of the request */
#define Method_GET_DEFAULT_PAGE 8
#define Method_HEAD_DEFAULT_PAGE 9

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
  long unsigned int contentLength = 0;
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
  if (!readToken(rs, &rp, " \n")) return 0; /* Read the authority */
  c = *rp.last;
  switch (c) {                       /* See what we found */
  case ' ':                          /* Found a space - save authority */
    authority = malloc(rp.last - rp.first + 1);
    memcpy(authority, rp.first, rp.last - rp.first);
    authority[rp.last - rp.first] = 0;
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
    char *delim = strchr(authority, '?');
    char *qstart;
    char *swissstart = NULL;
    char *ns;
    char *query;

    if (!delim) {                /* query portion missing */
      /* Set up to serve a default page */
      switch (methodIndex) {
      case Method_GET:
      case Method_HEAD:
	break;
      default:
	DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bad request (no query)\n");
	writeStatusLine(rs, 400);  /* Return Bad Request */
	writeMessage(rs, "URI query field missing.", methodIndex==1);
	return 0;                  /* Must get back in sync with client */ 
      }
      delim = authority+strlen(authority);
    }
    /* Save the path */
    pathLength = delim - authority;
    if (pathLength) {
      pathandquery = malloc(strlen(authority)+1); /* maxlen <= len(authority) */
      memcpy(pathandquery, authority, pathLength);
      pathandquery[pathLength] = 0;
    } else {                     /* path is empty */
      pathandquery = malloc(2+strlen(authority));
      strcpy(pathandquery, "/");
      pathLength = 1;
    }
    query = pathandquery + pathLength; /* End of path */

    /* Extract the Swiss number from the query. The Swiss number will be
       the s=number parameter. These keyword parameters are separated by
       either the ampersand (&) character or the semicolon (;) character.
       The Swiss number will be the last s= if there are more than one
       s= in the query. */
    if (*delim) {
      qstart = delim+1;                 /* Point at the query */
      if (('s' == *qstart && '=' == *(qstart+1))) {
	/* There is an s= which starts the query */
	swissstart = qstart;            /* Save it as the first */
	ns = qstart+2;
      } else {
	swissstart = NULL;              /* No s= found yet */
	ns = qstart;
      }
      
      while (1) {
	char *sand = strstr(ns, "&s=");
	char *ssemi = strstr(ns, ";s=");
	if (sand) {
	  swissstart = sand+1;
	  if (ssemi) if (sand < ssemi) swissstart = ssemi+1;
	  ns = swissstart+2;
	} else if (ssemi) {
	  swissstart = ssemi+1;
	  ns = swissstart+2;
	} else {
	  break;
	}
      }
    }

    if (!swissstart) {           /* No Swiss number */
      switch (methodIndex) {
      case Method_GET:
	methodIndex = Method_GET_DEFAULT_PAGE;
	break;      
      case Method_HEAD:
	methodIndex = Method_HEAD_DEFAULT_PAGE;
	break;
      default:
	DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bad request (no query)\n");
	writeStatusLine(rs, 400);  /* Return Bad Request */
	writeMessage(rs, "URI query field missing.", methodIndex==1);
	return 0;                  /* Must get back in sync with client */ 
      }
    } else {
      memcpy(query, qstart, swissstart-qstart); /* Save first part of query */
      query[swissstart-qstart] = 0;
      
      /* When we get here, swissstart points to the beginning of the s= 
	 parameter and pathandquery holds the portion of the path concatenated
	 with the portion of the query that proceeded the s= parameter */
      delim = strchr(swissstart, '&');
      if (!delim) delim = strchr(swissstart, ';');
      if (delim) {               /* More than just s= */
	int len = delim - swissstart - 2;
	swissNumber = malloc(len + 1);
	memcpy(swissNumber, swissstart+2, len);
	swissNumber[len] = 0;
	
	len = strlen(delim + 1); /* Length of rest of query w/o leading &/; */
	if (len) {               /* If there is anything there */
	  strcat(pathandquery, delim+1);
	} /* Else nothing else to add to query */
      } else {                   /* s= is the only query */
	swissNumber = malloc(strlen(swissstart+2) + 1);
	strcpy(swissNumber, swissstart+2);
	/* Don't add to query */
      }
      /* HTTP 1.1 must handle % encoding of the Swiss number, but we may
	 foil a few attacks by not handling that, and our Swiss numbers do
	 not include characters that need to be % encoded. */
    }
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

    if (TREE_NIL == existing) existing = tree_find(tree, headerName);
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
      tree = tree_insert(tree, node);
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
    TREENODE *node = tree_find(tree, "Connection");
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
    TREENODE *node = tree_find(tree, "Expect");
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
    TREENODE *node = tree_find(tree, "Content-length");
    
    if (TREE_NIL != node) {
      len = strlen(node->value);
      if (len > 15 || len == 0) {
	writeStatusLine(rs, 413);   /* Say the entity is too large */
	writeMessage(rs, "File too long for upload", methodIndex==1);
	freeStorage();
	return 0;     /* Ignoring warnings in the RFC to make sure zapping
			 the connection doesn't destroy the response */
      }
      contentLength = atol(node->value);
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
  
  /* We've read all the headers and are ready to do the method. First
     we look up the Swiss number and see if we are serving a file, or 
     if some domain will handle the request */

  if (Method_GET_DEFAULT_PAGE != methodIndex
      && Method_HEAD_DEFAULT_PAGE != methodIndex) {
    if (!lookUpSwissNumber(swissNumber, methodIndex, 
			   pathLength, pathandquery)) {
      /* Swiss number not found or other error */
      writeStatusLine(rs, 404);
      writeMessage(rs, "File not found on server.", methodIndex==1);
      freeStorage();
      return 0;
    }
  } else {
    theResourceType = capros_HTTPResource_RHType_File;
  }
  
    switch (theResourceType) {
    case capros_HTTPResource_RHType_HTTPRequestHandler:
      {
#ifdef SELF_TEST
	DBGPRINT(DBGTARGET, "lookUpSwissNumber gave HTTPRequestHandler");
	writeStatusLine(rs, 500);
	writeMessage(rs, "lookUpSwissNumber gave HTTPRequestHandler response",
		     methodIndex==1);
	freeStorage();
	return 0;
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
      unsigned char *headerBuffer = malloc(headerBufferLength);
      unsigned char *bp = headerBuffer;
      unsigned char *bufend;
      int kl, vl;
      uint16_t statusCode;
      capros_HTTPRequestHandler_TransferEncoding bodyTransferEncoding;      
      uint64_t contentLength;
      uint32_t lengthOfBodyData;
      char cl[128];       /* For generating a content length header */

      for (node = tree_min(tree); 
	   node != TREE_NIL; 
	   node = tree_succ(node)) {
	kl = strlen(node->key);
	if ( (kl==17 && memcmpci(node->key, "Transfer Encoding", kl)==0)) {
	  continue;      /* Skip the transfer encoding header */
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
      *bp++ = 0;         /* Terminate with zero length header name */
      bufend = bp;

      /* Now send the headers to the handler */
      bp = headerBuffer;
      while (bufend - bp) {
	uint32_t len = (bufend-bp < theSendLimit ? bufend-bp : theSendLimit);
      	rc = capros_HTTPRequestHandler_headers(KR_FILE, len, bp, &theSendLimit);
	if (RC_OK != rc) {
          DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: RH_headers got rc=%#x\n", rc);
	  writeStatusLine(rs, 500);
	  writeMessage(rs, "Bad HTTPRequestHandler.headers response.",
		       methodIndex==1);
	  freeStorage();
	  return 0;
	}
        bp += len;
      }

      /* If there is a 100-continue expectation, get result from handler */
      if (expect100) {
	rc = capros_HTTPRequestHandler_getContinueStatus(KR_FILE, &statusCode);
	if (RC_capros_key_UnknownRequest == rc) {
	  statusCode = 100;
	} else if (RC_OK != rc) {
	  writeStatusLine(rs, 500);
	  writeMessage(rs,
		       "Bad HTTPRequestHandler.getContinueStatus response.",
		       methodIndex==1);
	  freeStorage();
	  return 0;
	}
	writeStatusLine(rs, statusCode); 
	if (statusCode != 100) {
	  writeMessage(rs, "HTTPRequestHandler response for 100-continue.",
		       methodIndex==1);
	  freeStorage();
	  return 0;
	}
      }

      /* Now transfer the body of the request */
      //TODO handle chunked transfers.
      while ( contentLength > 0) {
	int len;

	if (!readExtend(rs, &rp)) {
	  /* Network I/O error */
	  //TODO Notify HTTPRequestHandler of error
	  freeStorage();
	  return 0; /* Kill the connection */
	}
	len = rp.last - rp.first;
	if (contentLength < len) len = contentLength;
	if (len > theSendLimit) len = theSendLimit;
	rc = capros_HTTPRequestHandler_body(KR_FILE, len, 
					    (unsigned char *)rp.first,
					    &theSendLimit);
        DEBUG(http) DBGPRINT(DBGTARGET,
                             "HTTP: Sent body to Resource rc=%#x\n", rc);
	if (RC_OK == /*??? FIXME*/ rc) {      /* handler error */
	  writeStatusLine(rs, 500);
	  writeMessage(rs, "HTTPRequestHandler error on body.", 0);
	  freeStorage();
	  return 0;         /* Need to get back in sync with client */
	}
	contentLength -= len;
	readConsume(rs, rp.first+len);
      }

      /* Get the response status */
      rc =  capros_HTTPRequestHandler_getResponseStatus(KR_FILE, &statusCode,
							&bodyTransferEncoding,
							&contentLength);
      writeStatusLine(rs, statusCode);
      switch (bodyTransferEncoding) {
      case capros_HTTPRequestHandler_TransferEncoding_none:
	freeStorage();
	if (mustClose) return 0;
	return 1;
	break;
      case capros_HTTPRequestHandler_TransferEncoding_identity:
	sprintf(cl, "Content-Length: %ld\r\n", contentLength);
	writeString(rs, cl);
	break;
      case capros_HTTPRequestHandler_TransferEncoding_chunked:
	writeString(rs, "Transfer-Encoding: chunked\r\n");
	break;
      }
      //TODO generate a date header

      /* Get additional headers from the handler */
      if (0 == transferHeaders(rs,
		     capros_HTTPRequestHandler_getResponseHeaderData)) {
        /* Error. We've already reported a status, so we can't give 500. 
	   just zap the circuit */
	freeStorage();
	return 0;
      }

      /* Receive the data from the handler and send it to the connection */
      if (Method_HEAD != methodIndex) {
	while (1) {
	  rc = capros_HTTPRequestHandler_getResponseBody(KR_FILE, 
							 headerBufferLength,
							 &lengthOfBodyData,
							 headerBuffer);
	  if (RC_OK != rc) {
	    freeStorage();
	    return 0;
	  }
	  if (0 == lengthOfBodyData) break;
	  writeSSL(rs, headerBuffer, lengthOfBodyData); /* Write it */
	}
      }

      /* Send out any trailer headers */
      if (0 == transferHeaders(rs,
		     capros_HTTPRequestHandler_getResponseTrailer)) {
      /* Error. We've already reported a status, so we can't give 500. 
	 just zap the circuit */
	freeStorage();
	return 0;
      }
#endif
    }
    break;
  case  capros_HTTPResource_RHType_MethodNotAllowed:
    writeStatusLine(rs, 405); 
    writeMessage(rs, "Method Not Allowed", methodIndex==1);
    freeStorage();
    break;
      
  case capros_HTTPResource_RHType_File:
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
	  char buf[2048];
	  int len;
	  
	  if (isUsingChunked) {
	    do {
	      len = readFile(buf, sizeof(buf));
	      sprintf(cl, "%x\r\n", len);
	      writeString(rs, cl);
	      if (0 != len) {
		if (!writeSSL(rs, buf, len)) {
		  /* Write error */
		  freeStorage();
		  return 0; /* Kill the connection */
		}
	      }
	      writeString(rs, "\r\n");
	    } while (0 != len);
	  } else {
	    while ( (len=readFile(buf, sizeof(buf))) > 0) {
	      if (!writeSSL(rs, buf, len)) {
		freeStorage();
		return 0;
	      }
	    }
	  }
	  closeFile();
	  freeStorage();
	  if (0 != len) return 0;  /* Read error, kill the connection */
	}
      }
      break;
    case Method_GET_DEFAULT_PAGE:
    case Method_HEAD_DEFAULT_PAGE:
      {
	unsigned long int len; /* The file length */
	char cl[128];
	
	len = strlen(defaultPageData);
	writeStatusLine(rs, 200);
	sprintf(cl, "Content-Length: %ld\r\n", len);
	writeString(rs, cl);

	// TODO Consider what Cache-Control directives to issue, if any
	writeString(rs, "Content-Type: text/html\r\n\r\n");
	if (Method_GET_DEFAULT_PAGE == methodIndex) { /* not HEAD - send body */
	  /*  Actually send the file */
	  if (!writeSSL(rs, defaultPageData, len)) {
	    freeStorage();
	    return 0;
	  }
	}
	freeStorage();
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
	  if (!readExtend(rs, &rp)) {
	    /* Network I/O error */
	    destroyFile();
	    freeStorage();
	    return 0; /* Kill the connection */
	  }
	  len = rp.last - rp.first;
	  if (contentLength < len) len = contentLength;
	  len = writeFile(rp.first, len);
	  if (len < 0) {      /* File write error */
	    writeStatusLine(rs, 500);
	    writeMessage(rs, "File I/O error on write.", 0);
	    destroyFile();
	    freeStorage();
	    return 0;         /* Need to get back in sync with client */
	  }
	  contentLength -= len;
	  readConsume(rs, rp.first+len);
	}
	closeFile();
	freeStorage();
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
    break;
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
 * @return is 0 if there was an error otherwise 1. If return is 1, rp->first
 *         will point to the token, and rp->next will point to the next
 *         separator. The characters from rp->first to rp->last-1 inclusive
 *         are the token.
 */
static int
readToken(ReaderState *rs, ReadPtrs *rp, char *sepStr) {
  char *cp;

  if (!readConsumeSeps(rs, rp, rp->last, sepStr)) return 0;
  for (;;) {
    cp = findSeparator(rp, sepStr);
    if (cp) break;
    if (!readExtend(rs, rp)) return 0;
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
static int
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
static void
  readConsume(ReaderState *rs, char *first) {
  rs->current = first - rs->buf;
}

/**
 * readConsumeSeps - Indicate that data in the buffer is no longer
 *                   needed and remove any leading separators. This
 *                   routine will re-fill the buffer to remove leading
 *                   separators.
 * 
 * @param[in] rs is the ReaderState for this connection.
 * @param[in] rp is a ReadPtrs.
 * @param[in] first is the first byte in the buffer still needed.
 * @param[in] sepStr is a string of separator characters to scan for
 *
 * @return is 0 if an internal error occured otherwise 1. If the return is 1,
 *         rp will be set to the first non-separator and the amount of input
 *         available.
 */

static int
readConsumeSeps(ReaderState *rs, ReadPtrs *rp, char *first, char *sepStr) {
  char c;

  readConsume(rs, first);
  do {
    while (rs->current < rs->last) {
      c = rs->buf[rs->current];
      if (!strchr(sepStr, c)) {
	rp->first = rs->buf + rs->current;
	rp->last = rs->buf + rs->last;
	return 1;
      }
      rs->current++;
    }
  } while (readExtend(rs, rp));
  return 0;
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
 * @param[in] rp is a ReadPtrs which will be updated to show the available
 *               data.
 *
 * @return is the result of the operation as follows:
 *     0 - An ssl read error has been detected. bail out
 *     1 - Data is available in ReadPtrs
 */
static int
readExtend(ReaderState *rs, ReadPtrs *rp) {
  int rc;
  
  if (rs->current) {    /* Some data has been consumed, push it down */
    memcpy(rs->buf, rs->buf+rs->current, rs->last-rs->current);
    rs->last -= rs->current;
    rs->current = 0;
  }
  /* Read the HTTP request */
  while (1){
    int err;
    
#ifdef SHORT_READ
    rc = SSL_read(rs->plain, rs->buf+rs->last, 1);
#else
    rc = SSL_read(rs->plain, rs->buf+rs->last, HTTP_BUFSIZE-rs->last);
#endif
    err = SSL_get_error(rs->plain, rc);
    if (SSL_ERROR_NONE == err) break;
    else if (SSL_ERROR_WANT_WRITE == err || SSL_ERROR_WANT_READ == err) {
      DEBUG(netio) DBGPRINT(DBGTARGET, "SSL_read netio needed\n");
      push_ssl_data(rs->network);
      continue;
    } else {
      print_SSL_error_queue();
      return 0;
    }
  }
  if (0 == rc) return 0;   /* Connection shutdown */
  if (rc > 0) {
    DEBUG(http) DBGPRINT(DBGTARGET, "Read: %.*s\n", rc, rs->buf+rs->last);
  }
  rs->last += rc;
  /* rc is the amount of data read from ssl - sanity check it */
  if (rs->last > HTTP_BUFSIZE || rc < 0) {
    DBGPRINT(DBGTARGET, "Read size error, read=%d, inBuffer=%d\n",
	     rc, rs->last);
    print_SSL_error_queue();
    return 0;
  }
  rp->first = rs->buf;
  rp->last = rs->buf + rs->last;
  return 1;
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
static int
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
static int
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
static int
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
static int
writeSSL(ReaderState *rs, void *data, int len) {
  int rc;
  
  DEBUG(netio) DBGPRINT(DBGTARGET, "HTTP SSL write=%.*s\n", len, (char*)data);
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


#ifndef SELF_TEST
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
    while (bl > 0) {
      rc = getProc(KR_FILE, bl, &headerLength, (unsigned char *)b);
      DEBUG(http) DBGPRINT(DBGTARGET, "HTTP: got headers rc=%#x hl=%d\n",
                           rc, headerLength);
      if (RC_OK != rc) {
        DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: getProc rc=%#x\n", rc);
	/* Handler had problems, just die */
	return 0;
      }
      if (0 == headerLength) break;
      b += headerLength;
      bl -= headerLength;
    }
    if (0 == headerLength) break;
    bl = headerBufferLength - bl;    /* Amount in the buffer */
    headerBufferLength *= 2;      /* Size of new buffer, twice old size */
    b = malloc(headerBufferLength);
    memcpy(b, headerBuffer, bl);  /* Copy what we have to new buffer */
    free(headerBuffer);           /* Free old */
    headerBuffer = (char *)b;     /* Point at start of all the headers */
    b += bl;                      /* Place for next byte of header data */
    bl = headerBufferLength - bl; /* Len left is (new len - what we got) */
  }
  /* Now put them out as headers */
  bl = b - headerBuffer;
  b = headerBuffer;
#define getByte(p) (*(unsigned char *)p)
  for (ln=getByte(b); ln; ln=getByte(b)) {  /* Get length of next name */
    if (!ln) break;                       /* Zero is end of headers */
    
    lv = (getByte(b+1) << 8) + getByte(b+2);
    bl -= ln +lv + 3;
    if (bl < 0) {
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bl=%d\n", bl);
      /* Error from handler, we've already reported a status, so we
	 can't give 500. just zap the circuit */
      return 0;         /* Need to get back in sync with client */
    }
    
    writeSSL(rs, b+2, ln);                /* Write it to connection */
    writeSSL(rs, ": ", 2);                /* Write colon and space */
    writeSSL(rs, b+2+ln, lv);             /* Write the value */
    writeSSL(rs, "\r\n", 2);              /* Finish with a CRLF */
    b += ln + lv + 3;
    if (!bl) break;
  }
  writeSSL(rs, "\r\n", 2);              /* Finish with blank line */
  return 1;
}
#endif


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
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Get file key \"%s\" rc=%#x\n",
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
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Read file rc=%d numRead=%d\n",
		       rc, size);
  theFileCursor += size;
  if (RC_OK == rc) return size;
  else return -1;
#endif
}




#ifndef SELF_TEST
/* Compatibility routines */
#include <errno.h>

int raise(int signo) {
  DBGPRINT(DBGTARGET, "HTTP: raise %d\n", signo);
  errno = ESRCH;
  return -1;
}

#ifdef EROS_TARGET_arm
int _isatty(int fd) {
#else
int isatty(int fd) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: isatty %d\n", fd);
  return 0;
}
#if EROS_TARGET_arm
int _stat(char *fn, int *stat) {
#else
int stat(char *fn, int *stat) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _stat %s\n", fn);
  errno = ENOENT;
  return -1;
}

 int _fstat(int fd, int *stat) {
   DBGPRINT(DBGTARGET, "HTTP: _fstat\n");
   errno = EIO;
   return -1;
 }

int kill(int pid, int signo) {
  DBGPRINT(DBGTARGET, "HTTP: _kill pid= %d, sig=%d\n", pid, signo);
  errno = ESRCH;
  return -1;
}

int getpid() {
  DBGPRINT(DBGTARGET, "HTTP: getpid\n");
  return 55;
}

#if EROS_TARGET_arm
int _lseek(int fd, int offset, int whence) {
#else
int lseek(int fd, int offset, int whence) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _lseek fd= %d, off=%d whence=%d\n",
	   fd, offset, whence);
  errno = ESPIPE;
  return -1;
}

int fstat(int fd, int *stat) {
  DBGPRINT(DBGTARGET, "HTTP: _fstat %d\n", fd);
  errno = EBADF;
  return -1;
}

#if EROS_TARGET_arm
int _gettimeofday(int tv, int tz) {
#else
int gettimeofday(int tv, int tz) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _gettimeofday\n");
  errno = EFAULT;
  return -1;
}

#if EROS_TARGET_arm
int _read(int fd, void *buf, size_t len) {
#else
int read(int fd, void *buf, size_t len) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _read fd=%d\n", fd);
  return 0;  // Signal EOF
}

#if EROS_TARGET_arm
int _write(int fd, void *buf, size_t len) {
#else
int write(int fd, void *buf, size_t len) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _read fd=%d\n", fd);
  return len;  // Say we wrote it
}

#if EROS_TARGET_arm
int _open(char *path, int flags, int mode) {
#else
int open(char *path, int flags, int mode) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _open fn=%s\n", path);
  errno = ENOENT;
  return -1;
}

#if EROS_TARGET_arm
int _close(int fd) {
#else
int close(int fd) {
#endif
  DBGPRINT(DBGTARGET, "HTTP: _close %d\n", fd);
  errno = EBADF;
  return -1;
}

int select(int fdno, void *fds, void *timeout) {
  return fdno;
}

int getuid(void) {
  DBGPRINT(DBGTARGET, "HTTP: getuid\n");
  return 55;
}

typedef void (*sig_t) (int);
sig_t signal(int sig, sig_t func) {
  DBGPRINT(DBGTARGET, "HTTP: signal=%d, func=%d\n", sig, func);
  return 0;
}
#endif
