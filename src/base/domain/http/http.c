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
static int processRequest(Message *argmsg);
static uint32_t connection(void);
static int setUpContext(SSL_CTX *ctx);
static void print_SSL_error_queue(void);
static void push_ssl_data(BIO *network_bio);
static ReaderState *readInit(SSL *ssl, BIO *network_bio, ReadPtrs *rp);
static int readExtend(ReaderState *rs, ReadPtrs *rp);
static void readConsume(ReaderState *rs, char *first);
static int readConsumeSeps(ReaderState *rs, ReadPtrs *rp, char *first, 
			   char *sepStr);
static void readSkipDelim(ReaderState *rs, ReadPtrs *rp);
static int readToken(ReaderState *rs, ReadPtrs *rp, char *sepStr);
static int process_http(SSL *ssl, BIO *network_bio);
static char *findSeparator(ReadPtrs *rp, char *sepStr);
static int compareToken(ReadPtrs *rp, char *list[]);
static int writeSSL(ReaderState *rs, void *data, int len);
static int writeStatusLine(ReaderState *rs, int statusCode);
static int writeString(ReaderState *rs, char *str);
static int writeMessage(ReaderState *rs, char *msg, int isHEAD);
static int openFile(char *name, int isRead);
static int closeFile(void);
static void destroyFile(char * name);
static uint64_t getFileLen(void);
static int readFile(void *buf, int len);
static int writeFile(void *buf, int len);



int
main(void)
{
  Message msg;

#ifndef SELF_TEST
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



  while (process_http(ssl, network_bio)) { /* Process html until error */
    push_ssl_data(network_bio);
  }

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
process_http(SSL *ssl, BIO *network_bio) {
  /* Parse the HTTP request. The request is terminated by a cr/lf/cr/lf
     sequence. */
  //  char *breakChars = "()<>@,;:\\\"/[]?={} \t\r\n";
  ReadPtrs rp;
  ReaderState *rs = readInit(ssl, network_bio, &rp);
  int mustClose = 0;    /* We must close the connection after response */
  int methodIndex;
  int versionIndex;
  int headerIndex = -1;
  char *requestList[] = {"GET", "HEAD", "PUT", NULL};
  char *versionList[] = {"HTTP/1.1", NULL};
  char *headerList[] = {"User-Agent", "Host", "Accept", "Accept-Language",
			"Accept-Encoding", "Accept-Charset", "Keep-Alive",
			"Connection", "Cache-Control", "Expect",
			"Content-Encoding", "Content-Length", NULL};
  char *connectionList[] = {"close", NULL};
  char *expectationList[] = {"100-continue", NULL};
  char *fileName = NULL;
  long unsigned int contentLength = 0;
  int expect100 = 0;
  char c;

  /* TODO skip leading blank lines */
  if (!readToken(rs, &rp, " \n")) return 0;
  if (*rp.last != ' ') {
    DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bad request format\n");
    writeStatusLine(rs, 400);  /* Return Bad Request */
    writeSSL(rs, "\r\n", 2);
    return 1;
  }
  methodIndex = compareToken(&rp, requestList);
  if (-1 == methodIndex) {
    DEBUG(http) DBGPRINT(DBGTARGET, "HTTP: request not recognized %.*s\n",
			 rp.last - rp.first, rp.first);
    writeStatusLine(rs, 501); /* Return Not Implemented */
    writeSSL(rs, "\r\n", 2);
    return 1;
  }

  /* Get the URI part of the request */
  if (!readToken(rs, &rp, "? \n")) return 0; /* Read authority to '?' */
  /* TODO handle % HEX HEX encoding in the request URI (1.1 must) */
  c = *rp.last;
  switch (c) {                       /* See what we found */
  case '\?':       
    readSkipDelim(rs, &rp);          /* Found a '?' - skip it */
    if (!readToken(rs, &rp, " \n")) return 0;
    fileName = malloc(rp.last - rp.first + 1);
    memcpy(fileName, rp.first, rp.last - rp.first);
    fileName[rp.last - rp.first] = 0;
    /* TODO handle % HEX HEX encoding in the request URI (1.1 must) */
    break;
  case ' ':                                   /* Found a space */
    // TODO serve a default page
    return 0;//TODO remove this
    break;
  default:                                    /* Newline etc bad request */
    DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: bad request (newline etc)\n");
    writeStatusLine(rs, 400);  /* Return Bad Request */
    writeSSL(rs, "\r\n", 2);
    return 1;
  }

  /* Get the http version */
  if (!readToken(rs, &rp, " \n\r")) return 0;
  versionIndex = compareToken(&rp, versionList);
  if (-1 == versionIndex) {
    DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: version not supported\n");
    writeStatusLine(rs, 505);  /* Return HTTP Version Not Supported*/
    writeMessage(rs, "This server only supports HTTP/1.1", methodIndex==1);
    free(fileName);
    return 1;
  }

  /* We should now have a CRLF as the next item */
  if (!readToken(rs, &rp, "\n")) {
    if(fileName) free(fileName);
    return 0;
  }
  if (rp.last-rp.first > 1) {
    if (fileName) free(fileName);
    writeStatusLine(rs, 400);  /* Return Bad Request */
    writeSSL(rs, "\r\n", 2);
    return 1;
  }
  readSkipDelim(rs, &rp);

  /* Process the message headers */
  while (1) {
    if (!readToken(rs, &rp, ":\n")) {
      if (fileName) free(fileName);
      return 0;
    }
    if ((rp.last - rp.first == 0) /* LF only, not kosher, but accepted */
	|| (rp.last - rp.first == 1 && *rp.first == '\r')) {
      /* Clear out the LF from the blank line after the headers */
      readSkipDelim(rs, &rp);
      break;
    }
    /* A header may be continued by having a line starting with SP or HT all
       CRLF, HT, SP should be replaced with one SP before interpratation. */
    if ( !(*rp.first == ' ' || *rp.first =='\t')) {
      headerIndex = compareToken(&rp, headerList);    
      readSkipDelim(rs, &rp);           /* Skip the : */
    }
    /* We are now pointed at either the character after the :, which may be
       whitespace or a parameter, or at the leading whitespace in a 
       continuation line. */
    switch (headerIndex) {
    case 0:               /* "User-Agent" */
    case 1:               /* "Host" */
      break;
    case 2:               /* "Accept" */
      // We really should make sure our text/text is acceptable - someday
      // if we don't see text/text | text/* | */*, we should give 406
      break;
    case 3:               /* "Accept-Language" */
      // We have no idea what human language the response is in. It may be
      // binary for all we know. Hope you like it.
      break;
    case 4:               /* "Accept-Encoding" */
      // We should check for "identity" with a q=0 saying it is unacceptable.
      // Hay, life is rough all over, so take the identity encoding, 'cus
      // that's all we know.
      break;
    case 5:               /* "Accept-Charset" */
      // We don't know character sets from adam. RFC2616 says we should send
      // 406 if we don't have a response in an acceptable character set, 
      // although sending some unacceptable character set is also allowed.
      // So we'll send what we have.
      break;
    case 6:               /* "Keep-Alive" */
      // This header is triggered by Connection: keep-alive.
      break;
    case 7:               /* "Connection" */
      // We must handle Connection: close, and close the connection after 
      // our response.
      while (1) {
	int i;
	
	if (!readToken(rs, &rp, ", \t\r\n")) {
	  if (fileName) free(fileName);
	  return 0;
	}
	i = compareToken(&rp, connectionList);
	if (0==i) mustClose = 1;
	if (*rp.last == '\r' || *rp.last == '\n') break;
      }
    case 8:               /* "Cache-Control" */
      // We don't do caching, so we can ignore any Cache-Control directives.
      break;
    case 9:               /* "Expect" */
      {
	// We need to send a interum response of 100 if we get 100-continue
	int i;
	
	if (!readToken(rs, &rp, ", \r\n")) {
	  if (fileName) free(fileName);
	  return 0;
	}
	i = compareToken(&rp, expectationList);
	if (i == 0) {
	  expect100 = 1;
	} else {
	  writeStatusLine(rs, 417);
	  writeSSL(rs, "\r\n", 2);
	  return 1;
	}
      }
      break;
    case 10:              /* Content-Encoding */
      // We only accept "identity", so don't even look for it
      break;
    case 11:              /* Content-Length */
      {  
	// We need this header for upload length
	char cl[16];
	int len;
	
	if (!readToken(rs, &rp, " \r\n")) {
	  if (fileName) free(fileName);
	  return 0;
	}
	len = rp.last - rp.first;
	if (len > sizeof(cl)-1 || len == 0) {
	  writeStatusLine(rs, 413);   /* Say the entity is too large */
	  writeMessage(rs, "File too long for upload", methodIndex==1);
	  if (fileName) free(fileName);
	  return 0;     /* Ignoring warnings in the RFC to make sure zapping
			   the connection doesn't destroy the response */
	}
	memcpy(cl, rp.first, len);
	cl[len] = 0;
	contentLength = atol(cl);
      }
      break;
    default:
      if (rp.last > rp.first) {
	DBGPRINT(DBGTARGET, "Header %.*s not handled\n", 
		 rp.last-rp.first, rp.first);
      } else{
	DBGPRINT(DBGTARGET, "Non-header line encountered\n");
      }
      break;
    }
    /* clean out to next CRLF */
    if (!readToken(rs, &rp, "\n")) { 
      if (fileName) free(fileName);
      return 0;
    }
    readSkipDelim(rs, &rp);
  }     /* End of while(1) to read all the headers */
  /* When we get here, the LF of the blank line at the end of the headers
     should have been consumed */

    
  /* Now do the method */
  switch (methodIndex) {
  case 0:          /* Handle a GET method */
  case 1:          /* Handle a HEAD method */
    if (fileName && openFile(fileName, 1)) {
      unsigned long int len; /* The file length */
      char cl[128];
      
      len = getFileLen();
      sprintf(cl, "Content-Length: %ld\r\n", len);
      
      writeStatusLine(rs, 200);
      writeString(rs, cl);
      writeString(rs, "Content-Type: text/text\r\n\r\n");
      //      writeString(rs, "Content-Type: text/html\r\n\r\n");
      // TODO Consider what Cache-Control directives to issue, if any
      if (0 == methodIndex) {       /* GET, not HEAD - send message-body */
	/*  Actually send the file */
	char buf[2048];
	int len;
	
	while ( (len=readFile(buf, sizeof(buf))) > 0) {
	  if (!writeSSL(rs, buf, len)) {
	    /* Write error */
	    if(fileName) free(fileName);
	    return 0; /* Kill the connection */
	  }
	}
	closeFile();
	if(fileName) free(fileName);
	fileName = NULL;
	if (0 != len) return 0;  /* Read error, kill the connection */
      }
    } else {
      writeStatusLine(rs, 404);
      writeMessage(rs, "File not found on server.", methodIndex==1);
    }
    break;
  case 2:             /* Handle the PUT method */
    if (fileName && openFile(fileName, 0)) {
      int len;

      if (expect100) {
	writeStatusLine(rs, 100);
	writeSSL(rs, "\r\n", 2);
      }
      /*  Actually receive the file */
      
      while ( contentLength > 0) {
	if (!readExtend(rs, &rp)) {
	  /* Network I/O error */
          destroyFile(fileName);
	  free(fileName);
	  return 0; /* Kill the connection */
	}
	len = rp.last - rp.first;
	if (contentLength < len) len = contentLength;
	len = writeFile(rp.first, len);
	if (len < 0) {      /* File write error */
	  writeStatusLine(rs, 500);
	  writeMessage(rs, "File I/O error on write.", 0);
          destroyFile(fileName);
	  free(fileName);
	  return 0;         /* Need to get back in sync with client */
	}
	contentLength -= len;
	readConsume(rs, rp.first+len);
      }
      closeFile();
      free(fileName);
      fileName = NULL;
      writeStatusLine(rs, 200);
      writeString(rs, "Content-Length: 0\r\n");
      writeSSL(rs, "\r\n", 2);
      //      writeMessage(rs, "File Received", 0);
    } else {
      writeStatusLine(rs, 404);
      writeMessage(rs, "File not found on server.", 0);
    }
    break;
  default: {
    char msg[128];
    
    sprintf(msg, "Method %s not handled", requestList[methodIndex]);
    DBGPRINT(DBGTARGET, "%s\n", msg);
    writeStatusLine(rs, 500);    /* Internal server error */
    writeMessage(rs, msg, methodIndex==1);

  }
  }
  /* Push any queued data to the network */
  if(fileName) free(fileName);
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
      writeSSL(rs, "\r\n", 2);
      DEBUG(http) DBGPRINT(DBGTARGET, "BadChar: %x\n", c);
      return 0;
    }
  }
  return 1;
}

/**
 * compareToken - Compare a token with a list of valid ones and return the
 *                tokens index in the list.
 * 
 * @param[in] rp is a pointer to a ReadPtr structure defining the token
 * @param[in] list is an array of pointers to strings, the last is NULL
 * 
 * @return is the index in the list, or -1 if it is not in the list.
 */
static int
compareToken(ReadPtrs *rp, char *list[]) {
  int i;
  int len = rp->last - rp->first;

  for (i=0; list[i]; i++) {
    if (strlen(list[i]) == len && 0 == memcmp(rp->first, list[i], len)) {
      return i;
    }
  }
  return -1;
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
 *
 * @return is a new ReaderState object for this connection.
 */
static ReaderState *
readInit(SSL *ssl, BIO *network_bio, ReadPtrs *rp) {
  ReaderState *state = (ReaderState*)malloc(sizeof(ReaderState));
  state->plain = ssl;
  state->network = network_bio;
  state->current = 0;
  state->last = 0;
  rp->first = rp->last = state->buf;
  return state;
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

#ifdef SELF_TEST
int theFile;
#else
// File is in KR_FILE.
capros_File_fileLocation theFileCursor;
capros_File_fileLocation theFileSize;
#endif
/**
 * openFile - Open a HTTP referenced file. Only one file at a time can be open.
 *
 * @param[in] name is the name of the file
 * @param[in] isRead is true if the access will be read, false if write
 *
 * @return is 1 if the file is opened, 0 if there was an error.
 */
static int
openFile(char *name, int isRead) {
#ifdef SELF_TEST
/* To keep from exposing the whole Unix file system, limit the character
   set of name */
  int i;
  char fullname[strlen(name) + 256];
  for (i=0; i<strlen(name); i++) {

    if (NULL == 
	strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",
	       name[i])) {
      DBGPRINT(DBGTARGET, 
	       "Bad character \"%c\" in filename \"%s\"\n", name[i], name);
      theFile = -1;
      return 0;
    }
  }
  strcpy(fullname, "/home/");
  strcat(fullname, getlogin());
  strcat(fullname, "/testdir/");
  strcat(fullname, name);
  theFile = open(fullname, (isRead ? O_RDONLY : O_WRONLY|O_CREAT),
		 S_IRUSR | S_IWUSR);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Open \"%s\" result %d\n",
		       fullname, theFile);
  if (-1 == theFile) return 0;
  return 1;
#else
  result_t rc;
  uint32_t len = strlen(name);
  // TODO create file for write and handle existing file if any
  if (len > capros_IndexedKeyStore_maxNameLen) {
    DBGPRINT(DBGTARGET, "Name %s too long, len=%d\n", name, len);
    return 0;
  }
  theFileCursor = 0;
  rc = capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_Directory, KR_DIRECTORY);
  assert(RC_OK == rc);
  if (isRead) {
    rc = capros_IndexedKeyStore_get(KR_DIRECTORY, len, (uint8_t*)name, KR_FILE);
    DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Get file key \"%s\" rc=%#x\n",
		         name, rc);
    if (RC_OK != rc) {
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: Directory_get returned %#x\n",
                             rc);
      return 0;
    }
    rc = capros_File_getSize(KR_FILE, &theFileSize);
    DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Get file size %lld rc=%#x\n",
		         theFileSize, rc);
    if (RC_OK != rc) {
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: File_getSize returned %#x\n",
                             rc);
      return 0;
    }
  } else {	// writing a file
    rc = capros_Node_getSlot(KR_CONSTIT, capros_HTTP_KC_FileServer, KR_TEMP0);
    assert(RC_OK == rc);
    rc = capros_FileServer_createFile(KR_TEMP0, KR_FILE);
    if (RC_OK != rc) {
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: createFile returned %#x\n",
                             rc);
      return 0;
    }
    rc = capros_IndexedKeyStore_put(KR_DIRECTORY, KR_FILE, len, (uint8_t*)name);
    if (RC_OK != rc) {
      DEBUG(errors) DBGPRINT(DBGTARGET, "HTTP: Directory_put returned %#x\n",
                             rc);
      return 0;
    }
  }
  return 1;
#endif
}

/**
 * destroyFile - Destroy the file created for writing.
 */
static void
destroyFile(char * name)
{
#ifdef SELF_TEST
  // FIXME
#else
  capros_IndexedKeyStore_delete(KR_DIRECTORY, strlen(name), (uint8_t*)name);
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
 * @return is the length of the file.
 */
static uint64_t
getFileLen(void) {
#ifdef SELF_TEST
  struct stat sb; 
  
  fstat(theFile, &sb);
  DEBUG(file) DBGPRINT(DBGTARGET, "HTTP: Get file size %d\n", (int)sb.st_size);
  return sb.st_size;
#else
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
