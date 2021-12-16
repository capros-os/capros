/*
 * Copyright (C) 2010, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <string.h>
#include <lwip/err.h>
#include <lwip/netif.h>
#include <netif/etharp.h>
#include <ipv4/lwip/inet.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <asm/arch/ep93xx-regs.h>
#include <asm/arch/irqs.h>
#include <eros/Invoke.h>
#include <idl/capros/Number.h>

#include <lwipCap.h>
#include "../../cap.h"

struct netif * gNetif;
void __iomem * hwBaseAddr;
unsigned int ep93xx_phyad;	// PHY address

// From ep93xx.c:
int ep93xx_open(void);
err_t low_level_output(struct netif *netif, struct pbuf *p);
void init_mii(void);

err_t
ep93xxDevInitF(struct netif * netif)
{
  int ret;
  unsigned char macAddress[6];
  gNetif = netif;

  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100000000 /* 100Mbps */);

  netif->output = etharp_output;
  netif->linkoutput = &low_level_output;

  // Map device registers:
  hwBaseAddr = ioremap(EP93XX_ETHERNET_PHYS_BASE, 0x10000);
  assert(hwBaseAddr);

  // Get our MAC address.
  memcpy(&macAddress, ((char *)hwBaseAddr) + 0x50, 6);

  assert(ETHARP_HWADDR_LEN == 6);
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  memcpy(netif->hwaddr, &macAddress, 6);

  netif->mtu = 1500;	/* Ethernet maximum transmission unit */
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

  init_mii();

  printk(KERN_INFO "ep93xx on-chip ethernet, IRQ %d, "
		 "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x.\n",
		IRQ_EP93XX_ETHERNET,
		macAddress[0], macAddress[1],
		macAddress[2], macAddress[3],
		macAddress[4], macAddress[5]);

  ret = ep93xx_open();
  assert(ret == 0);

  return ERR_OK;
}

struct IPConfigv4 IPConf;

void
driver_main(void)
{
  result_t result;

  // KR_IPAddrs initially has a number cap
  // with the IP address, mask, and gateway.
  uint32_t ipInt, msInt, gwInt;
  result = capros_Number_get(KR_IPAddrs, &ipInt, &msInt, &gwInt);
  assert(result == RC_OK);
  IPConf.addr.addr = htonl(ipInt);
  IPConf.mask.addr = htonl(msInt);
  IPConf.gw.addr   = htonl(gwInt);

  result = cap_init(&IPConf);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "lwip cap_init returned %#x!\n", result);
  } else {
    cap_main();
  }
}
