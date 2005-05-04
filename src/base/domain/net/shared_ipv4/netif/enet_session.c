#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>

#include <domain/domdbg.h>
#include <domain/EnetKey.h>

#include "enet_session.h"
#include "enetkeys.h"

/* Maintains a list of all active sessions */
struct enet_client_session ActiveSessions[MAX_SESSIONS];
static uint32_t ActiveNo = 0;

uint32_t 
init_active_sessions() 
{
  int i;
  
  for(i=0;i<MAX_SESSIONS;i++) {
    ActiveSessions[i].sessionssid = -1;
  }
  return RC_OK;
}

/* Create a new stack session */
uint32_t 
enet_create_stack_session(int stackid,uint32_t addr1,uint32_t addr2) 
{
  uint32_t result;
  
  result = enet_create_client_session(stackid,0,addr1,addr1,addr2,addr2);

  return result;
}

/* Create a new client session */
uint32_t 
enet_create_client_session(int stackssid,int ssid,
			   uint32_t xmit_client_buffer,
			   uint32_t xmit_stack_buffer,
			   uint32_t recv_client_buffer,
			   uint32_t recv_stack_buffer)
{
  int i;
  
  if(ActiveNo < MAX_SESSIONS) {
    /* create a new session */
    for(i=0;i<MAX_SESSIONS;i++) 
      if(ActiveSessions[i].sessionssid == -1) break;
    
    ActiveSessions[i].stackssid = stackssid;
    ActiveSessions[i].sessionssid=ssid;
    ActiveSessions[i].mt[0].start_address = xmit_client_buffer;
    ActiveSessions[i].mt[0].sector = XMIT_CLIENT_SPACE;
    ActiveSessions[i].mt[0].cur_p = 0;
    
    ActiveSessions[i].mt[1].start_address = xmit_stack_buffer;
    ActiveSessions[i].mt[1].sector = XMIT_STACK_SPACE;
    ActiveSessions[i].mt[1].cur_p = 0;
    
    ActiveSessions[i].mt[2].start_address = recv_client_buffer;
    ActiveSessions[i].mt[2].sector = RECV_CLIENT_SPACE;
    ActiveSessions[i].mt[2].cur_p = 0;
    
    ActiveSessions[i].mt[3].start_address = recv_stack_buffer;
    ActiveSessions[i].mt[3].sector = RECV_STACK_SPACE;
    ActiveSessions[i].mt[3].cur_p = 0;
    
    ActiveNo++;

    return RC_OK;
  }else 
    /* All sessions exhausted */
    return RC_ENET_no_session_free;
}

void 
debug_active_sessions() 
{
  int i;

  for(i=0;i<MAX_SESSIONS;i++) {
    if(ActiveSessions[i].sessionssid != -1) {
      kprintf(KR_OSTREAM, "%d::%d -  %d--%08x,  %d--%08x,  "
	      "%d--%08x,  %d--%08x",
	      i,
	      ActiveSessions[i].sessionssid,
	      ActiveSessions[i].mt[0].sector,
	      ActiveSessions[i].mt[0].start_address,
	      ActiveSessions[i].mt[1].sector,
	      ActiveSessions[i].mt[1].start_address,
	      ActiveSessions[i].mt[2].sector,
	      ActiveSessions[i].mt[2].start_address,
	      ActiveSessions[i].mt[3].sector,
	      ActiveSessions[i].mt[3].start_address);
    }
  }
}
