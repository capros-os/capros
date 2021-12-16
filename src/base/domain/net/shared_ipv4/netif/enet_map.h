#ifndef __enet_map_h__
#define __enet_map_h__

uint32_t 
enet_MapStack(cap_t kr_bank,cap_t key1,cap_t key2,
	      	uint32_t *addr1,uint32_t *addr2);

uint32_t 
enet_MapClient_single_buff(cap_t kr_bank,cap_t kr_newspace,
			   uint32_t *buffer_addr);

/* Map a client to the stack */
uint32_t enet_MapClient(cap_t krBank,cap_t key1,cap_t key2,
			cap_t key3,cap_t key4,
			uint32_t *addr1,uint32_t *addr2,
			uint32_t *addr3,uint32_t *addr4);

#endif /*__enet_map_h__*/
