#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/endian.h>

#include <idl/eros/Sleep.h>

#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_NETSYS_C     KR_APP(1)
#define KR_NETSYS_S     KR_APP(2)
#define KR_NETSYS       KR_APP(3)
#define KR_SLEEP        KR_APP(4)
#define KR_DEVPRIVS     KR_APP(5)

#define IP_PROTO_ICMP  1

#define ICMP_ER        0     /* echo reply */
#define ICMP_ECHO      8     /* echo request */

struct icmp_echo_hdr {
  uint16_t _type_code;
  uint16_t chksum;
  uint16_t id;
  uint16_t seqno;
};

struct ip_addr {
  uint32_t addr;
};

#define ICMPH_TYPE(hdr) (ntohs((hdr)->_type_code) >> 8)
#define ICMPH_CODE(hdr) (ntohs((hdr)->_type_code) & 0xff)

#define ICMPH_TYPE_SET(hdr,type) ((hdr)->_type_code = htons(ICMPH_CODE(hdr) \
                                                      | ((type) << 8)))
#define ICMPH_CODE_SET(hdr,code) ((hdr)->_type_code = htons((code) \
                                                   | (ICMPH_TYPE(hdr) << 8)))


/* Globals */
char pkt[64];   /* The ip level packet which we will with icmp data */
uint32_t tickspermillisec; /* A calibrated cpu ticks per milli second */

/* Get cpu ticks since last reboot */
uint64_t 
rdtsc()
{
  uint64_t x;
  __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
  return x;
}

/* Calibrate the cpu speed in milliseconds 
 * Sleep for "time" milliseconds and normalize the cpu ticks
 * spent meanwhile */
void 
calibrate(uint16_t time)
{
  uint64_t init,finish;
  
  init = rdtsc();
  eros_Sleep_sleep(KR_SLEEP,time);
  finish = rdtsc();
  
  tickspermillisec = (finish - init)/time;
}

uint32_t 
chksum(void *dataptr, int len)
{
  uint32_t acc;
    
  for(acc = 0; len > 1; len -= 2)  
    acc += *((uint16_t *)dataptr)++;
  
  /* add up any odd byte */
  if(len == 1)  acc += htons((uint16_t)((*(uint8_t *)dataptr) & 0xff) << 8);
  
  acc = (acc >> 16) + (acc & 0xffffu);
  
  if ((acc & 0xffff0000) != 0) {
    acc = (acc >> 16) + (acc & 0xffffu);
  }
  
  return (uint16_t) acc;
}


uint16_t
inet_chksum(void *dataptr, uint16_t len)
{
  uint32_t acc;

  acc = chksum(dataptr, len);
  while(acc >> 16) {
    acc = (acc & 0xffff) + (acc >> 16);
  }    
  return ~(acc & 0xffff);
}

/* This function prepares an icmp echo packet */  
void
prepare_echo_packet(void *s,int size,uint16_t id,uint16_t seqno)
{
  struct icmp_echo_hdr *iecho;
  int i;

  iecho = (struct icmp_echo_hdr *)s;
  
  ICMPH_TYPE_SET(iecho,ICMP_ECHO);
  ICMPH_CODE_SET(iecho,0);
  
  iecho->id = id; /* Magic Id of our connection  */
  iecho->seqno = seqno;   /* Seqno of ping packet*/

  /* Fill in some data */
  for(i=0;i<size;i++) {
    ((char *)s)[i + sizeof(struct icmp_echo_hdr)] = i;
  }
  
  /* calculate checksum */
  iecho->chksum = 0;
  iecho->chksum = inet_chksum(s,size);
  
}


int 
main(void)
{
  uint32_t result;
  uint64_t init,finish,rtt = 0;
  //uint32_t ipaddr = 0x6333EFD8;
  uint32_t ipaddr  = 0x018F10AC;
  static uint16_t seqno;
  uint16_t MAGICID = 0x1234;
  Message msg;
  
  
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_NETSYS_C,KR_NETSYS_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP,KR_SLEEP);
  node_extended_copy(KR_CONSTIT,KC_DEVPRIVS,KR_DEVPRIVS);
  
  /* Move the DEVPRIVS key to the ProcIOSpace so we can do i/o calls */
  process_swap(KR_SELF, ProcIoSpace, KR_DEVPRIVS, KR_VOID);

  /* Construct the network system */
  result = constructor_request(KR_NETSYS_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_NETSYS_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "nettest:Constructing NETSYS...[FAILED]\n");
  }
  
  /* Calibaration of cpu ticks per milliseconds - also the dhcp client
   * gets configured meanwhile */
  calibrate(3000);
  
  
  msg.snd_key0 = KR_VOID;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  
  msg.rcv_key0 = KR_NETSYS;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  msg.snd_invKey = KR_NETSYS_S;
  msg.snd_code = OC_NetSys_GetSessionCreatorKey;
  CALL(&msg); /* Receive a session creator key */
  
  /* We now have netsys session creator key. 
   * So go ahead and create a session*/
  msg.snd_invKey = KR_NETSYS;
  msg.snd_key0 = KR_BANK;
  msg.snd_code = OC_NetSys_GetNewSessionKey;
  CALL(&msg);
  
  
  /* We now have a session key  */
  msg.snd_code = OC_NetSys_ICMPOpen; //open an IP level socket 
  msg.snd_invKey = KR_NETSYS;
  msg.rcv_key0 = KR_VOID;
  CALL(&msg);
  
  /* Since we don't yet support ARP queueing - 
   * Get our ARP refreshed with the address of the to-be-pinged machine */
  prepare_echo_packet(pkt,sizeof(pkt),MAGICID,seqno++);
  msg.snd_data = pkt;
  msg.snd_len = sizeof(pkt);
  msg.snd_code = OC_NetSys_ICMPPing; //Send our ping packet 
  msg.snd_w2 = ipaddr;
  msg.snd_w3 = IP_PROTO_ICMP;        //Protocol next to IP
  eros_Sleep_sleep(KR_SLEEP,1000);
  CALL(&msg);

  kprintf(KR_OSTREAM,"ping returned");
  for(;;) {
    msg.snd_code = OC_NetSys_ICMPPing;//Ping
    msg.snd_data = pkt;
    msg.snd_len = sizeof(pkt);
    eros_Sleep_sleep(KR_SLEEP,500);
    init = rdtsc();
    CALL(&msg);
    
    msg.snd_code = OC_NetSys_ICMPReceive; //ping recv
    CALL(&msg);
    /* Calculate the milliseconds passed by */
    finish = rdtsc();
    rtt = finish - init;
    
    kprintf(KR_OSTREAM,"pinging %u.%u.%u.%u: time = %d ms",
	    (uint8_t)(ntohl(ipaddr) >> 24 & 0xff),
	    (uint8_t)(ntohl(ipaddr) >> 16 & 0xff),
	    (uint8_t)(ntohl(ipaddr) >> 8 & 0xff),
	    (uint8_t)(ntohl(ipaddr) & 0xff), rtt/tickspermillisec);
  }
      
  msg.snd_code = OC_NetSys_ICMPClose; //close
  eros_Sleep_sleep(KR_SLEEP,2000);
  CALL(&msg);

  return 0;
}

