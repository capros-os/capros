#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/ProcessKey.h>
#include <eros/endian.h>

#include <idl/capros/Sleep.h>
#include <idl/capros/Stream.h>

#include <domain/ConstructorKey.h>
#include <domain/NetSysKey.h>
#include <domain/TftpKey.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include "constituents.h"

#include <ctype.h>
#include <string.h>

#define KR_OSTREAM      KR_APP(0)
#define KR_TFTP_C       KR_APP(1)
#define KR_TFTP         KR_APP(2)
#define KR_NETSYS       KR_APP(3)
#define KR_SLEEP        KR_APP(4)
#define KR_ETERM        KR_APP(5)

#define MAX_FILE_SIZE   10000
#define CALLTWICE       0

int inet_aton(const char *cp)
{
  uint32_t val;
  int base;
  char c;
  uint8_t parts[4];
  uint8_t partno = 0;
 
  c = *cp;
  for (;;) {
    /*
     * Collect number up to ``.''.
     * Values are specified as for C:
     * 0x=hex, 0=octal, isdigit=decimal.
     */
    if (!isdigit(c)) {
      return (0);
    }
    val = 0; base = 10;
    if (c == '0') {
      c = *++cp;
      if (c == 'x' || c == 'X')
	base = 16, c = *++cp;
      else
	base = 8;
    }
    for (;;) {
      if (isascii(c) && isdigit(c)) {
	val = (val * base) + (c - '0');
	c = *++cp;
      } else if (base == 16 && isascii(c) && isxdigit(c)) {
	val = (val << 4) |
	  (c + 10 - (islower(c) ? 'a' : 'A'));
	c = *++cp;
      } else
	break;
    }
    if (c == '.') {
      /*
       * Internet format:
       *  a.b.c.d
       *  a.b.c   (with c treated as 16 bits)
       *  a.b (with b treated as 24 bits)
       */
      if(partno < 4) {
	parts[partno++] = (uint8_t)val;
      }else break;

      c = *++cp;
    } else
      break;
  }
  
  parts[3] = (uint8_t)val;

  val = 0;
  
  val += parts[3] << 24;;
  val += parts[2] << 16;
  val += parts[1] << 8;
  val += parts[0];
    
  return val;
}

int
main(void)
{
  uint32_t result;
  Message msg;
  //char rcv_buffer[MAX_FILE_SIZE];
  char *filename = "demo.txt";
  char  *ip = "192.168.123.178"; 
  uint32_t ipaddr ;//= 0xB27BA8C0;
  char exit = '\0';
  
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_TFTP_C,KR_TFTP_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP,KR_SLEEP);
  node_extended_copy(KR_CONSTIT, KC_ETERM,KR_ETERM);
  
  ipaddr = inet_aton((const char *)ip);
    
  /* construct the console stream interface */
  result = constructor_request(KR_ETERM,KR_BANK,KR_SCHED,
                               KR_VOID,KR_ETERM);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "tftpcaller:Constructing consolestream...[FAILED]\n");
  }
  
  /* Construct the tftp domain */
  result = constructor_request(KR_TFTP_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_TFTP);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "tftpcaller:Constructing tftp...[FAILED]\n");
  }
  kprintf(KR_OSTREAM,"Constructed tfp domain");
  
  msg.snd_key0 = KR_ETERM;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len = 0;
  msg.snd_w1 = ipaddr;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;
  msg.snd_data = filename;
  msg.snd_len = strlen(filename);
  
  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  msg.snd_invKey = KR_TFTP;
  msg.snd_code = OC_TFTP_GET;
  CALL(&msg);
  
  if(msg.rcv_code == RC_OK) 
    kprintf(KR_OSTREAM,"Tftp-get of %s ... [SUCCESS]",filename);
  else 
    kprintf(KR_OSTREAM,"Tftp-get of %s ... [FAILED %d]",filename,msg.rcv_code);

#if CALLTWICE
  msg.snd_invKey = KR_TFTP;
  msg.snd_code = OC_TFTP_GET;
  CALL(&msg);
  if(msg.rcv_code == RC_OK) 
    kprintf(KR_OSTREAM,"Tftp-get of %s ... [SUCCESS]",filename);
  else 
    kprintf(KR_OSTREAM,"Tftp-get of %s ... [FAILED %d]",filename,msg.rcv_code);
#endif
  /* wait for a 'q or 'Q' to exit */
  for(;;) {
    capros_Stream_read(KR_ETERM,&exit);
    if(exit == 'q' || exit == 'Q') break;
  }
  
  kprintf(KR_OSTREAM,"tftpcaller exiting");
  return 0;
}


