#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/endian.h>
#include <eros/cap-instr.h>

#include <idl/eros/Sleep.h>
#include <idl/eros/Stream.h>

#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/TftpKey.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#include <string.h>

#define KR_OSTREAM      KR_APP(0)
#define KR_NETSYS_C     KR_APP(1)
#define KR_NETSYS_S     KR_APP(2)
#define KR_NETSYS       KR_APP(3)
#define KR_SLEEP        KR_APP(4)
#define KR_DEVPRIVS     KR_APP(5)
#define KR_START        KR_APP(6)
#define KR_ETERM        KR_APP(7)

#define TFTP_RRQ    1
#define TFTP_WRQ    2
#define TFTP_DATA   3
#define TFTP_ACK    4
#define TFTP_ERR    5

#define TFTP_BLOCKSIZE_DEFAULT    512
#define TFTP_PORT                 69

#define MAX_FILE_NAME  32
#define MAXRETRY        3

#define DEBUGTFTP if(0)

/* Globals */
char  rcv_buffer[MAX_FILE_NAME];  
uint32_t server_ipaddr;

static const char *tftp_error_msg[] = {
  "Undefined error",
  "File not found",
  "Access violation",
  "Disk full or allocation error",
  "Illegal TFTP operation",
  "Unknown transfer ID",
  "File already exists",
  "No such user"
};

/* Function prototypes */
int tftp_init_rrq(char * filename,uint32_t ipaddr);

static void
stream_write(cap_t strm, const char *s, size_t len)
{
  size_t i;

  for (i = 0; i < len; i++) {
    //   kprintf(KR_OSTREAM,"PRINTING  %d =  %d = %c",i,s[i],s[i]);
    (void) eros_Stream_write(strm, s[i]);
  }
}

void
stream_writes(cap_t strm, const char *s)
{
  stream_write(strm, s, strlen(s));
}


int
ProcessRequest(Message *msg)
{
  /* Dispatch the request on the appropriate interface */
  switch(msg->rcv_code) {
  case OC_TFTP_GET:
    {
      DEBUGTFTP kprintf(KR_OSTREAM,"Process request GET");
      COPY_KEYREG(KR_ARG(0),KR_ETERM);
      tftp_init_rrq(&rcv_buffer[0],msg->rcv_w1);
    } break;
  
  default:
    break;
  }
  return 1;
}

int 
tftp_init_rrq(char *filename,uint32_t ipaddr)
{
  //  uint32_t result;
  Message msg;
  char buf[1500];
  int totlen = 0;
  int block_nr = 1;
  int retry_count = MAXRETRY;

  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  
  /* We now have netsys session creator key. 
   * So go ahead and create a session*/
  msg.snd_invKey = KR_NETSYS_S;
  msg.snd_key0 = KR_BANK;
  msg.rcv_key0 = KR_NETSYS;
  msg.snd_code = OC_NetSys_GetNewSessionKey;
  CALL(&msg);
  
  /* We now have a session key  */
  msg.snd_code = OC_NetSys_UDPBind; //Bind a udp socket 
  msg.snd_invKey = KR_NETSYS;
  msg.snd_w3 = 0x100; /* local port to which we are bound */
  msg.rcv_key0 = KR_VOID;
  CALL(&msg);
  DEBUGTFTP kprintf(KR_OSTREAM,"Bind: returned result %d", msg.rcv_code);

  /* Connect to port */
  msg.snd_code = OC_NetSys_UDPConnect; //connect to tftp port :69
  msg.snd_invKey = KR_NETSYS;
  //msg.snd_w2 = server_ipaddr;
  msg.snd_w2 = ipaddr;
  //msg.snd_w2 = 0xB27BA8C0; //192.168.123.178
  msg.snd_w3 = TFTP_PORT;       //:69
  CALL(&msg);
  DEBUGTFTP kprintf(KR_OSTREAM,"connect: returned result %d",msg.rcv_code);
  
  /* Request for a file "trial" cmd = 1 == RRQ*/
  
  *((unsigned short *) buf) = htons(TFTP_RRQ);
  totlen += 2;
  
  strcpy(&buf[totlen],filename);
  totlen += strlen(filename);
  
  DEBUGTFTP 
    kprintf(KR_OSTREAM,"filename %s len = %d",filename,strlen(filename));
  //strcpy(&buf[totlen],"ltrial");
  //totlen += strlen("ltrial");
  
  buf[totlen++] = '\0';
  memcpy(&buf[totlen], "octet", 6);
  totlen += 6;
  
 RRQ:
  // UDP send 
  msg.snd_code = OC_NetSys_UDPSend; //Send to tftp port remote_ip:69
  msg.snd_invKey = KR_NETSYS;
  msg.snd_data = &buf[0];
  msg.snd_len = totlen;
  CALL(&msg);

  //eros_Sleep_sleep(KR_SLEEP,3000);
  //CALL(&msg); // retry our call this time wih our ARP cache refreshed
  
 RECEIVE:
  msg.snd_len = 0;
  msg.snd_data = 0;
  msg.rcv_data = &buf[0];
  msg.rcv_limit = 1500;
  msg.snd_code = OC_NetSys_UDPReceive; //Receive 
  msg.snd_invKey = KR_NETSYS;
  msg.snd_w2 = 40000; //120 ms timeout
  CALL(&msg);
  
  DEBUGTFTP
    kprintf(KR_OSTREAM,"---------tftp received %d-----------",msg.rcv_code);
  
  if(msg.rcv_code != RC_OK) {
    kprintf(KR_OSTREAM,"tftp received timeout retry count %d",retry_count);
    retry_count--;
    if (retry_count > 0) {
      if (block_nr ==1 ){
	kprintf(KR_OSTREAM,"Resending request");
	goto RRQ; 
      }else {goto RECEIVE;}
    } 
    
    msg.snd_code = OC_NetSys_UDPClose; //Close connection
    msg.snd_invKey = KR_NETSYS;
    msg.rcv_data = NULL;
    msg.rcv_limit = 0;
    CALL(&msg);
    return RC_TFTP_TimedOut;
  }else {

    int opcode = ntohs(*((unsigned short *) buf));
    unsigned short tmp = ntohs(*((unsigned short *) &buf[2]));
    retry_count=MAXRETRY;    
    if (opcode == TFTP_ERR) {
      char *msg = NULL;
      retry_count =0;
      if (buf[4] != '\0') {
	msg = &buf[4];
	buf[TFTP_BLOCKSIZE_DEFAULT - 1] = '\0';
      } else if (tmp < (sizeof(tftp_error_msg) / sizeof(char *))) {
	msg = (char *) tftp_error_msg[tmp];
      }
      
      if (msg) {
	kprintf(KR_OSTREAM,"\n\nserver says: %s", msg);
      }
      
      goto CLOSE;
    }/* end of TFTP_ERROR */
    
    if(opcode == TFTP_DATA ) {
      if(tmp == block_nr) {
	int i;
	//#define SHAP
#ifdef SHAP
	char mybuf[TFTP_BLOCKSIZE_DEFAULT * 2];
	char *pBuf = mybuf;

	for (i = 4; i < msg.rcv_sent; i++) {
	  if (buf[i] == '\n')
	    *pBuf++ = '\r';
	  *pBuf++ = buf[i];
	}

	stream_write(KR_ETERM, mybuf, pBuf - mybuf);
#else	
	int lastsegment = 4;

	DEBUGTFTP kprintf(KR_OSTREAM,"%d = len",msg.rcv_sent);
	for(i=4;i<msg.rcv_sent;i++) {
	  
	  DEBUGTFTP kprintf(KR_OSTREAM,"PRINTING  %d =  %d = %c",i,buf[i],buf[i]);
	  if(buf[i] == '\n' ) {
	    stream_write(KR_ETERM,&buf[lastsegment],i-lastsegment);
	    DEBUGTFTP kprintf(KR_OSTREAM,"newline %d -> %d",lastsegment,i);
	    stream_write(KR_ETERM,"\r\n",2);
	    lastsegment = i+1;
	    continue;
	  }
	  if(i==msg.rcv_sent-1) {
	    stream_write(KR_ETERM,&buf[lastsegment],i-lastsegment+1);
	    DEBUGTFTP kprintf(KR_OSTREAM,"printing from %d -> %d",lastsegment,i);
	  }
	}
#endif
		
	
	/* Now we try to send out an ack */
	totlen = 0;
	
	*((unsigned short *) buf) = htons(TFTP_ACK);
	totlen += 2;
	
	*((unsigned short *) &buf[totlen]) = htons(block_nr++);
	totlen += 2;
	msg.snd_code = OC_NetSys_UDPSend; //Send to tftp port remote_ip:69
	msg.snd_invKey = KR_NETSYS;
	msg.snd_data = &buf[0];
	msg.snd_len = totlen;
	CALL(&msg);
	
	if(msg.rcv_sent - 4 < TFTP_BLOCKSIZE_DEFAULT )  goto CLOSE;
	else goto RECEIVE;
      }
      else {
	/* Now we try to send out an ack */
	totlen = 0;
	
	*((unsigned short *) buf) = htons(TFTP_ACK);
	totlen += 2;
	
	*((unsigned short *) &buf[totlen]) = htons(block_nr-1);
	totlen += 2;
	msg.snd_code = OC_NetSys_UDPSend; //Send to tftp port remote_ip:69
	msg.snd_invKey = KR_NETSYS;
	msg.snd_data = &buf[0];
	msg.snd_len = totlen;
	CALL(&msg);
	
	if(msg.rcv_sent - 4 < TFTP_BLOCKSIZE_DEFAULT )  goto CLOSE;
	else goto RECEIVE;

      }
    }
  }
  
  
 CLOSE:
  msg.snd_code = OC_NetSys_UDPClose; //Close connection
  msg.snd_invKey = KR_NETSYS;
  msg.rcv_data = NULL;
  msg.rcv_limit = 0;
  CALL(&msg);
 
  return RC_OK;
}

int 
main(void)
{
  uint32_t result;
  Message tmsg;
  
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_NETSYS_C,KR_NETSYS_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP,KR_SLEEP);
  node_extended_copy(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);

  /* Move the DEVPRIVS key to the ProcIOSpace so we can do i/o calls */
  process_swap(KR_SELF, ProcIoSpace, KR_DEVPRIVS, KR_VOID);

  process_make_start_key(KR_SELF,0,KR_START);
  
  /* Construct the network system */
  result = constructor_request(KR_NETSYS_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_NETSYS_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "nettest:Constructing NETSYS...[FAILED]\n");
  }
  
  eros_Sleep_sleep(KR_SLEEP,5000);
  
  /* Initialize the message structure */
  tmsg.snd_key0   = KR_VOID; /* Send back the generic start key */
  tmsg.snd_key1   = KR_VOID;
  tmsg.snd_key2   = KR_VOID;
  tmsg.snd_rsmkey = KR_VOID;
  tmsg.snd_data = 0;
  tmsg.snd_len  = 0;
  tmsg.snd_code = 0;
  tmsg.snd_w1 = 0;
  tmsg.snd_w2 = 0;
  tmsg.snd_w3 = 0;

  tmsg.rcv_key0   = KR_VOID;
  tmsg.rcv_key1   = KR_VOID;
  tmsg.rcv_key2   = KR_VOID;
  tmsg.rcv_rsmkey = KR_VOID;
  tmsg.rcv_data =  NULL;
  tmsg.rcv_limit  = 0;
  tmsg.rcv_code = 0;
  tmsg.rcv_w1 = 0;
  tmsg.rcv_w2 = 0;
  tmsg.rcv_w3 = 0;

  DEBUGTFTP kprintf(KR_OSTREAM,"check for configuration");
  tmsg.snd_invKey = KR_NETSYS_S;
  tmsg.snd_code = OC_NetSys_GetNetConfig;
  CALL(&tmsg); /* Check if we are configured */
  
  if(tmsg.rcv_code != RC_OK ) {
    result = tmsg.rcv_code; /* We have no ip address as yet */
  }else {
    server_ipaddr = tmsg.rcv_w1 & 0xffffff;
    server_ipaddr += (1 << 24); 
    kprintf(KR_OSTREAM,"ipaddr = %x",tmsg.rcv_w1);
    kprintf(KR_OSTREAM,"netmask = %x",tmsg.rcv_w2);
    kprintf(KR_OSTREAM,"gateway = %x",tmsg.rcv_w3);
    
    tmsg.snd_code = OC_NetSys_GetSessionCreatorKey;
    tmsg.rcv_key0 = KR_NETSYS_S;
    CALL(&tmsg); /* Receive a session creator key */
  }
  
  /* Return back to our builder here */
  tmsg.snd_invKey = KR_RETURN;
  tmsg.snd_key0   = KR_START; /* Send back the generic start key */
  tmsg.snd_key1   = KR_VOID;
  tmsg.snd_key2   = KR_VOID;
  tmsg.snd_rsmkey = KR_RETURN;
  tmsg.snd_data = 0;
  tmsg.snd_len  = 0;
  tmsg.snd_code = result;
  tmsg.snd_w1 = 0;
  tmsg.snd_w2 = 0;
  tmsg.snd_w3 = 0;

  tmsg.rcv_key0   = KR_ARG(0);
  tmsg.rcv_key1   = KR_ARG(1);
  tmsg.rcv_key2   = KR_ARG(2);
  tmsg.rcv_rsmkey = KR_RETURN;
  tmsg.rcv_data =  &rcv_buffer[0];
  tmsg.rcv_limit  = MAX_FILE_NAME;
  tmsg.rcv_code = 0;
  tmsg.rcv_w1 = 0;
  tmsg.rcv_w2 = 0;
  tmsg.rcv_w3 = 0;
  
  do {
     RETURN(&tmsg);
     tmsg.snd_key0 = KR_VOID;
 } while (ProcessRequest(&tmsg));
 
  return 0;
}
