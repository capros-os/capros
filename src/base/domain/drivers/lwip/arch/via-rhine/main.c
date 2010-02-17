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
////#include <disk/NPODescr.h>
#include <domain/assert.h>

#include <lwip/err.h>
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

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

err_t
devInitF(struct netif * netif)
{
  assert(!"complete");
  return 0;
}

struct IPConfigv4 IPConf;

void
driver_main(void)
{
  result_t result;

kprintf(KR_OSTREAM, "Via-rhine driver called***\n");////
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

  cap_main(&IPConf);
}
