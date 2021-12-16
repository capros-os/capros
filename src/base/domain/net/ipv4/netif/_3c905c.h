#ifndef __3C905C_H__
#define __3C905C_H__

uint32_t boomerang_interrupt();
uint32_t _3c905c_probe();
uint32_t boomerang_start_xmit(struct pbuf *p,int type,struct ip_addr *ipaddr);


#endif /*__3C905C_H__*/
