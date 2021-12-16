/*
 * Copyright (C) 2008-2010, Strawberry Development Group
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <string.h>
#include <lwip/stats.h>
#include <lwip/pbuf.h>
#include <netif/etharp.h>
#include <ipv4/lwip/ip.h>

#include <domain/assert.h>

#define dbg_errors 0x01
#define dbg_rx     0x02
#define dbg_tx     0x04

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

extern struct netif * gNetif;

void
printPacket(uint8_t * data, unsigned int pktLength,
  unsigned int maxBytesToPrint)
{
#define maxMaxBTP 100
  char printBuffer[maxMaxBTP *3 + 1];
  if (maxBytesToPrint > maxMaxBTP)
    maxBytesToPrint = maxMaxBTP;	// take min
  char * printCursor = &printBuffer[0];
  unsigned int printLength = pktLength;
  if (printLength > maxBytesToPrint)
    printLength = maxBytesToPrint;
  int i;
  for (i = 0; i < printLength; i++) {
    sprintf(printCursor, " %.2x", data[i]);
    printCursor += 3;
  }
  printk("%s +%dB\n", printBuffer, pktLength-printLength);
}

void
ethInput(void * data, unsigned int length)
{
  uint8_t * b = data;

  DEBUG(rx) {
    printk("Eth rcvd: ");
#if 1	// show input data
    printPacket(data, length, 56);
#endif
  }

  unsigned int lenType = ntohs(((struct eth_hdr *)b)->type);
  if (lenType <= 1500		// IEEE 802.3 length field
      || lenType == 0x86dd	/* IPv6 */ ) {
    // We don't support such packets. Just drop it.
  } else {
    struct pbuf * p =  pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if (p != NULL) {
      assert(p->tot_len == length);
      struct pbuf * q;
      for (q = p; q != NULL; b += q->len, q = q->next) {
        memcpy(q->payload, b, q->len);
      }

      err_t errNum;

      switch (lenType) {
        /* IP or ARP packet? */
        case ETHTYPE_IP:
        case ETHTYPE_ARP:
#if PPPOE_SUPPORT
        /* PPPoE packet? */
        case ETHTYPE_PPPOEDISC:
        case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
          /* full packet send to tcpip_thread to process */
          // Note, netif->input is ethernet_input().
          errNum = gNetif->input(p, gNetif);
          if (errNum != ERR_OK) {
            // ethernet_input doesn't return errors, so this never happens.
            DEBUG(errors) kdprintf(KR_OSTREAM,
                                "ethernet_input error %d\n", errNum);
            LWIP_DEBUGF(NETIF_DEBUG,
                    ("ethernetif_input: IP input error\n"));
            pbuf_free(p);
            p = NULL;
          }
          break;

        default:
          DEBUG(errors) {
            printPacket(data, length, 60);
            kprintf(KR_OSTREAM, "Eth pkt type is %#x\n", lenType);
                            }
        case 0x0842:	// Wake-on-LAN magic packet, ignore it
          pbuf_free(p);
          p = NULL;
          break;
      }

      LINK_STATS_INC(link.recv);
    } else {
    	DEBUG(errors) kdprintf(KR_OSTREAM, "Can't alloc pbuf!\n");
    	LINK_STATS_INC(link.memerr);
    	LINK_STATS_INC(link.drop);
    }
  }
}

void
ethOutput(struct netif *netif, struct pbuf *p, uint8_t * data)
{
  // mtu is the max payload size. It does not include the Ethernet header.
  assert(p->tot_len <= netif->mtu + sizeof(struct eth_hdr));

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  /* We are forced to copy the data here, because the data may be
  deallocated as soon as we return.
  lwIP provides no mechanism to defer the deallocation. */

  uint8_t * bp = data;
  struct pbuf * q = p;
  while (1) {
    int len = q->len;	// The size of the data in the pbuf

    DEBUG(tx) kprintf(KR_OSTREAM,
                 "Sending pbuf %#x payload %#x len %d\n",
		 q, q->payload, len);

    memcpy(bp, q->payload, len);
    bp += len;

    if (q->tot_len == len)
      break;	// last pbuf of the packet
    q = q->next;
  }

  DEBUG(tx) {
#if 1	// show all output data
    int i;
    for (i = 0; i < (p->tot_len & 0xfff); i++)
      printk(" %.2x", data[i]);
    printk("\n");
#endif
  }

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
  
  LINK_STATS_INC(link.xmit);
}
