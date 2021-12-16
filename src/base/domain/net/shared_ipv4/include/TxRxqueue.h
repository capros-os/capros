#ifndef __TXRXQUEUE_H__
#define __TXRXQUEUE_H__

#include "pstore.h"

#define  Qsize 0                  /* The size of the queues */

struct pstore_queue {
  uint32_t pstoreoffset;
  uint32_t pstoresector;
  int type;
  struct ip_addr *ip_addr;
  int ssid;
  uint8_t status;
};

uint32_t rx_service();
uint32_t tx_service();
uint32_t netif_start_xmit(struct pstore *p,int type,
			  struct ip_addr *ipaddr,int ssid);
uint32_t pstoreQ_init(uint32_t start_addr,struct pstore_queue **pq);

#endif /*__TXRXQUEUE_H__*/
