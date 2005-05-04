#ifndef __enet_session_H__
#define __enet_session_H__

#include <eros/NodeKey.h>
#include "../mapping_table.h"

#define MAX_SESSIONS  3

struct enet_client_session {
  int stackssid;
  int sessionssid;
  struct mapping_table mt[4];
};

/* init the active sessions data structure */
uint32_t init_active_sessions();

/* Create a new client session */
uint32_t 
enet_create_client_session(int stackssid,int ssid,
			   uint32_t xmit_client_buffer,
			   uint32_t xmit_stack_buffer,
			   uint32_t recv_client_buffer,
			   uint32_t recv_stack_buffer);

/* Create a new stack session */
uint32_t enet_create_stack_session(int stackid,uint32_t addr1,uint32_t addr2);

/* debugging aid */
void debug_active_sessions();

/* park  a particular session */
#define parkSession(msg) {\
  msg->invType = IT_Retry;\
  msg->snd_w1 = RETRY_SET_LIK|RETRY_SET_WAKEINFO;\
  msg->snd_w2 = msg->rcv_w1; /* wakeinfo value */\
  msg->snd_key0 = KR_PARK_WRAP;\
}

/* wake and retry a particular session */
#define  wakeSession(parkingNo) \
 node_wake_some_no_retry(KR_PARK_NODE,0,0,parkingNo)

#endif /*__enet_session_H__*/
