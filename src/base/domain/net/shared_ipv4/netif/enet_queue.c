#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <domain/domdbg.h>
#include "../include/pstore.h"
#include "../include/ip_addr.h"
#include "../include/TxRxqueue.h"

#include "netif.h"
#include "enet_session.h"
#include "enetkeys.h"

#define DEBUG_ENETQ if(0)

extern struct enet_client_session ActiveSessions[MAX_SESSIONS];

/* Call the appropriate function, demuxing the packet as a result */
uint32_t 
tx_service() 
{
  int i,ssid,*cur_p;
  uint32_t start_address;
  struct pstore *p;
  int work_done = 0;
  
  for(i=0;i<MAX_SESSIONS;i++) {
    /* Check to see if valid session exists */
    if(ActiveSessions[i].sessionssid != -1) {
      /* Load up all our temporary variables for this session */
      ssid = i;
      cur_p = &ActiveSessions[ssid].mt[XMIT_STACK_SPACE].cur_p;
      start_address = ActiveSessions[ssid].mt[XMIT_STACK_SPACE].start_address;
      p = (void *)(start_address + 
		   (*cur_p)*(PSTORE_POOL_BUFSIZE + sizeof(struct pstore)));
    
      /* We don't have any filled up buffer to service */
      if(p->status == PSTORE_READY) {
	DEBUG_ENETQ
	  kprintf(KR_OSTREAM,
		  "tx_e:ssid=%d s=%d add=%08x len=%dstat=%d cur_p = %d",
		  ssid,XMIT_STACK_SPACE,&p[0],p->tot_len,p->status,cur_p);
	
	/* Now transmit our packet */
	NETIF.start_xmit(p,ssid);
	
	/* advance our pointer */
	*cur_p = *cur_p + 1 >= PSTORE_POOL_SIZE ? 0 : *cur_p + 1;
#if 0
	/* Our next buffer to look at */
	if(0 == *cur_p) p = (void *)start_address;
	else p = (void*)((uint32_t)&p[0] + PSTORE_POOL_BUFSIZE + 
			 sizeof(struct pstore));
	work_done = 1;
#endif
	return 1;
      }
    }
  }  
  
  return work_done;
}
  
