#ifndef __3C905C_H__
#define __3C905C_H__

uint32_t boomerang_interrupt();
uint32_t _3c905c_probe(struct pci_dev_data *, struct netif *);
uint32_t boomerang_start_xmit(struct pstore *,int ssid);


#endif /*__3C905C_H__*/
