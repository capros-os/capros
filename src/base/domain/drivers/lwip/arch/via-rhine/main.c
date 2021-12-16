/*
 * Copyright (C) 2010, Strawberry Development Group
 *
 * This file is part of the CapROS Operating System.
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
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <domain/assert.h>

#include <lwip/err.h>
#include <lwip/netif.h>
#include <netif/etharp.h>
#include <ipv4/lwip/inet.h>
#include <eros/Invoke.h>
#include <idl/capros/Number.h>
#include <idl/capros/Node.h>
#include <idl/capros/IPInt.h>
#include <domain/PCIDrvr.h>

#include <lwipCap.h>
#include "../../cap.h"

#define dbg_tx     0x1
#define dbg_rx     0x2
#define dbg_errors 0x4
#define dbg_probe  0x8

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

// From via-rhine.c:
extern struct pci_driver rhine_driver;
err_t low_level_output(struct netif *netif, struct pbuf *p);

struct netif * gNetif;

err_t
devInitF(struct netif * netif)
{
  int ret;
  gNetif = netif;

  NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 100000000 /* 100Mbps */);

  netif->output = etharp_output;
  netif->linkoutput = &low_level_output;

  thePCIDev.dev.driver = &rhine_driver.driver;
  struct pci_device_id * id = (struct pci_device_id *) &theNdd.deviceId;
  id->driver_data = (unsigned long) &rhine_driver;
  // Call the probe function:
  DEBUG(probe) kprintf(KR_OSTREAM, "Via-rhine driver calling probe***\n");
  ret = (*rhine_driver.probe)(&thePCIDev, id);
  if (ret) {
    kdprintf(KR_OSTREAM, "rhine_driver.probe returned %d\n", ret);
    thePCIDev.dev.driver = NULL;
  }

  struct net_device * netdev = pci_get_drvdata(&thePCIDev);

  ret = (* netdev->netdev_ops->ndo_open)(netdev);
  if (ret) {
    kdprintf(KR_OSTREAM, "rhine_open returned %d\n", ret);
  }

  // Get our MAC address.
  assert(ETHARP_HWADDR_LEN == netdev->addr_len);
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  memcpy(netif->hwaddr, &netdev->dev_addr, ETHARP_HWADDR_LEN);

  netif->mtu = netdev->mtu;	// Ethernet maximum transmission unit
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;
  DEBUG(probe) kprintf(KR_OSTREAM, "Via-rhine devInitF done.\n");

  return ERR_OK;
}

struct IPConfigv4 IPConf;

void
driver_main(void)
{
  result_t result;
  Message Msg = {
    .snd_invKey = KR_RETURN,
    .snd_w1 = 0,
    .snd_w2 = 0,
    .snd_w3 = 0,
    .snd_key0 = KR_VOID,
    .snd_key1 = KR_VOID,
    .snd_key2 = KR_VOID,
    .snd_rsmkey = KR_VOID,
    .snd_len = 0,
  };

  DEBUG(probe) kprintf(KR_OSTREAM, "Via-rhine driver called***\n");
  PCIDriver_mainInit("Via-Rhine eth");

  // KC_NetConfig has a number cap
  // with the IP address, mask, and gateway.
  uint32_t ipInt, msInt, gwInt;
  result = capros_Node_getSlotExtended(KR_CONSTIT, KC_NetConfig, KR_TEMP0);
  assert(result == RC_OK);
  result = capros_Number_get(KR_TEMP0, &ipInt, &msInt, &gwInt);
  assert(result == RC_OK);
  IPConf.addr.addr = htonl(ipInt);
  IPConf.mask.addr = htonl(msInt);
  IPConf.gw.addr   = htonl(gwInt);

  // The following is all we need of rhine_init():
  rhine_driver.driver.name = rhine_driver.name;

  result = cap_init(&IPConf);
  assert(result == RC_OK);	// FIXME
  // FIXME Destroy self if result != RC_OK.

  // Return to the registry, still in KR_RETURN:
  Msg.snd_code = result;
  PSEND(&Msg);

  cap_main();
}
