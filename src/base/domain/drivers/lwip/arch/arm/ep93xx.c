/*
 * EP93xx ethernet network device driver
 * Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 * Dedicated to Marija Kulikova.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2008, Strawberry Development Group
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <stdlib.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#undef TIME_WAIT
#include <disk/NPODescr.h>
#include <domain/assert.h>

#include <lwip/stats.h>
#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/pbuf.h>
#include <netif/etharp.h>
#include <ipv4/lwip/ip.h>
#include <lwip/udp.h>
#include <lwip/tcp.h>

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/mii.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <asm/arch/ep93xx-regs.h>
#include <asm/arch/platform.h>
#include <asm/io.h>
#include <idl/capros/IPInt.h>

#include "../../cap.h"

#define dbg_tx 1
#define dbg_rx 2

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DEBUG(x) if (dbg_##x & dbg_flags)

struct netif * gNetif;
void __iomem * hwBaseAddr;
unsigned int ep93xx_phyad;	// PHY address
unsigned char macAddress[6];

#define DRV_MODULE_NAME		"ep93xx-eth"
#define DRV_MODULE_VERSION	"0.1"

#define RX_QUEUE_ENTRIES	64
#define TX_QUEUE_ENTRIES	8

#define MAX_PKT_SIZE		2044
#define PKT_BUF_SIZE		2048

#define REG_RXCTL		0x0000
#define  REG_RXCTL_DEFAULT	0x00073800
#define REG_TXCTL		0x0004
#define  REG_TXCTL_ENABLE	0x00000001
#define REG_MIICMD		0x0010
#define  REG_MIICMD_READ	0x00008000
#define  REG_MIICMD_WRITE	0x00004000
#define REG_MIIDATA		0x0014
#define REG_MIISTS		0x0018
#define  REG_MIISTS_BUSY	0x00000001
#define REG_SELFCTL		0x0020
#define  REG_SELFCTL_RESET	0x00000001
#define REG_INTEN		0x0024
#define  REG_INTEN_TX		0x00000008
#define  REG_INTEN_RX		0x00000007
#define REG_INTSTSP		0x0028
#define  REG_INTSTS_TX		0x00000008
#define  REG_INTSTS_RX		0x00000004
#define REG_INTSTSC		0x002c
#define REG_AFP			0x004c
#define REG_INDAD0		0x0050
#define REG_INDAD1		0x0051
#define REG_INDAD2		0x0052
#define REG_INDAD3		0x0053
#define REG_INDAD4		0x0054
#define REG_INDAD5		0x0055
#define REG_GIINTMSK		0x0064
#define  REG_GIINTMSK_ENABLE	0x00008000
#define REG_BMCTL		0x0080
#define  REG_BMCTL_ENABLE_TX	0x00000100
#define  REG_BMCTL_ENABLE_RX	0x00000001
#define REG_BMSTS		0x0084
#define  REG_BMSTS_RX_ACTIVE	0x00000008
#define REG_RXDQBADD		0x0090
#define REG_RXDQBLEN		0x0094
#define REG_RXDCURADD		0x0098
#define REG_RXDENQ		0x009c
#define REG_RXSTSQBADD		0x00a0
#define REG_RXSTSQBLEN		0x00a4
#define REG_RXSTSQCURADD	0x00a8
#define REG_RXSTSENQ		0x00ac
#define REG_TXDQBADD		0x00b0
#define REG_TXDQBLEN		0x00b4
#define REG_TXDQCURADD		0x00b8
#define REG_TXDENQ		0x00bc
#define REG_TXSTSQBADD		0x00c0
#define REG_TXSTSQBLEN		0x00c4
#define REG_TXSTSQCURADD	0x00c8
#define REG_MAXFRMLEN		0x00e8

struct ep93xx_rdesc
{
	u32	buf_addr;
	u32	rdesc1;
};

#define RDESC1_NSOF		0x80000000
#define RDESC1_BUFFER_INDEX	0x7fff0000
#define RDESC1_BUFFER_LENGTH	0x0000ffff

struct ep93xx_rstat
{
	u32	rstat0;
	u32	rstat1;
};

#define RSTAT0_RFP		0x80000000
#define RSTAT0_RWE		0x40000000
#define RSTAT0_EOF		0x20000000
#define RSTAT0_EOB		0x10000000
#define RSTAT0_AM		0x00c00000
#define RSTAT0_RX_ERR		0x00200000
#define RSTAT0_OE		0x00100000
#define RSTAT0_FE		0x00080000
#define RSTAT0_RUNT		0x00040000
#define RSTAT0_EDATA		0x00020000
#define RSTAT0_CRCE		0x00010000
#define RSTAT0_CRCI		0x00008000
#define RSTAT0_HTI		0x00003f00
#define RSTAT1_RFP		0x80000000
#define RSTAT1_BUFFER_INDEX	0x7fff0000
#define RSTAT1_FRAME_LENGTH	0x0000ffff

struct ep93xx_tdesc
{
	u32	buf_addr;
	u32	tdesc1;
};

#define TDESC1_EOF		0x80000000
#define TDESC1_BUFFER_INDEX	0x7fff0000
#define TDESC1_BUFFER_ABORT	0x00008000
#define TDESC1_BUFFER_LENGTH	0x00000fff

struct ep93xx_tstat
{
	u32	tstat0;
};

#define TSTAT0_TXFP		0x80000000
#define TSTAT0_TXWE		0x40000000
#define TSTAT0_FA		0x20000000
#define TSTAT0_LCRS		0x10000000
#define TSTAT0_OW		0x04000000
#define TSTAT0_TXU		0x02000000
#define TSTAT0_ECOLL		0x01000000
#define TSTAT0_NCOLL		0x001f0000
#define TSTAT0_BUFFER_INDEX	0x00007fff

struct ep93xx_descs
{
	struct ep93xx_rdesc	rdesc[RX_QUEUE_ENTRIES];
	struct ep93xx_tdesc	tdesc[TX_QUEUE_ENTRIES];
	struct ep93xx_rstat	rstat[RX_QUEUE_ENTRIES];
	struct ep93xx_tstat	tstat[TX_QUEUE_ENTRIES];
};

struct ep93xx_priv
{
	struct ep93xx_descs	*descs;
	dma_addr_t		descs_dma_addr;

	void			*rx_buf[RX_QUEUE_ENTRIES];
	void			*tx_buf[TX_QUEUE_ENTRIES];

	unsigned int	rx_pointer;

	/* tx_clean_pointer is index of the beginning of pending entries
	   in tx_buf/descs.t*.
           tx_pointer is the index of the end of pending entries
	   in tx_buf/descs.t*.
	   tx_pending is the number of pending entries.
	   0 <= tx_pending <= TX_QUEUE_ENTRIES
	   "Pending entries" are ones being sent by the descriptor processor.
	 */
	unsigned int	tx_clean_pointer;
	unsigned int	tx_pointer;
	unsigned int	tx_pending;

	struct net_device_stats	stats;

	struct mii_if_info	mii;
	u8			mdc_divisor;
} theEp, *ep = &theEp;

#define rdb(ep, off)	((void)ep, __raw_readb(hwBaseAddr + (off)))
#define rdw(ep, off)	((void)ep, __raw_readw(hwBaseAddr + (off)))
#define rdl(ep, off)	((void)ep, __raw_readl(hwBaseAddr + (off)))
#define wrb(ep, off, val) ((void)ep, __raw_writeb((val), hwBaseAddr + (off)))
#define wrw(ep, off, val) ((void)ep, __raw_writew((val), hwBaseAddr + (off)))
#define wrl(ep, off, val) ((void)ep, __raw_writel((val), hwBaseAddr + (off)))

static int ep93xx_mdio_read(struct net_device *dev, int phy_id, int reg);

#if 0
static struct net_device_stats *ep93xx_get_stats(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	return &(ep->stats);
}
#endif

static void ep93xx_rx(void)
{
	struct ep93xx_priv * ep = &theEp;
	int processed;

	processed = 0;
	while (1) {
		int entry = ep->rx_pointer;
		struct ep93xx_rstat * rstat = ep->descs->rstat + entry;

		u32 rstat0 = rstat->rstat0;
		u32 rstat1 = rstat->rstat1;
		int length = rstat1 & RSTAT1_FRAME_LENGTH;

		DEBUG(rx) printk("ep93xx_rx rcvd entry %d st0=%#x st1=%#x len=%d at %#x\n",
			entry, rstat0, rstat1, length, ep->rx_buf[entry]);

		if (!(rstat0 & RSTAT0_RFP) || !(rstat1 & RSTAT1_RFP)) {
			break;
		}
		// Both RFP bits are on.

		rstat->rstat0 = 0;
		rstat->rstat1 = 0;

		if (!(rstat0 & RSTAT0_EOF))
			printk(KERN_CRIT "ep93xx_rx: not end-of-frame "
					 " %.8x %.8x\n", rstat0, rstat1);
		if (!(rstat0 & RSTAT0_EOB))
			printk(KERN_CRIT "ep93xx_rx: not end-of-buffer "
					 " %.8x %.8x\n", rstat0, rstat1);
		if ((rstat1 & RSTAT1_BUFFER_INDEX) >> 16 != entry)
			printk(KERN_CRIT "ep93xx_rx: entry mismatch "
					 " %.8x %.8x\n", rstat0, rstat1);

		if (!(rstat0 & RSTAT0_RWE)) {
			ep->stats.rx_errors++;
			if (rstat0 & RSTAT0_OE)
				ep->stats.rx_fifo_errors++;
			if (rstat0 & RSTAT0_FE)
				ep->stats.rx_frame_errors++;
			if (rstat0 & (RSTAT0_RUNT | RSTAT0_EDATA))
				ep->stats.rx_length_errors++;
			if (rstat0 & RSTAT0_CRCE)
				ep->stats.rx_crc_errors++;
			goto err;
		}

		if (length > MAX_PKT_SIZE) {
			printk(KERN_NOTICE "ep93xx_rx: invalid length "
					 " %.8x %.8x\n", rstat0, rstat1);
			goto err;
		}

		/* Strip FCS.  */
		if (rstat0 & RSTAT0_CRCI)
			length -= 4;

		struct pbuf * p =  pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
		if (likely(p != NULL)) {
			assert(p->tot_len == length);
			struct pbuf * q;
			char * b;
			for (q = p, b = ep->rx_buf[entry];
			     q != NULL; b += q->len, q = q->next) {
			  memcpy(q->payload, b, q->len);
			}

			struct eth_hdr * ethhdr = p->payload;

			switch (ntohs(ethhdr->type)) {
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
			    if (gNetif->input(p, gNetif) != ERR_OK)
			     { LWIP_DEBUGF(NETIF_DEBUG,
			              ("ethernetif_input: IP input error\n"));
			       pbuf_free(p);
			       p = NULL;
			     }
			    break;

			  default:
			    pbuf_free(p);
			    p = NULL;
			    break;
			}

			LINK_STATS_INC(link.recv);
		} else {
			LINK_STATS_INC(link.memerr);
			LINK_STATS_INC(link.drop);
		}

err:
		ep->rx_pointer = (entry + 1) & (RX_QUEUE_ENTRIES - 1);
		processed++;
	}

	if (processed) {
		wrw(ep, REG_RXDENQ, processed);
		wrw(ep, REG_RXSTSENQ, processed);
	}
}

static int ep93xx_have_more_rx(struct ep93xx_priv *ep)
{
	struct ep93xx_rstat *rstat = ep->descs->rstat + ep->rx_pointer;
	return !!((rstat->rstat0 & RSTAT0_RFP) && (rstat->rstat1 & RSTAT1_RFP));
}

static void ep93xx_poll(void)
{
	struct ep93xx_priv * ep = &theEp;

poll_some_more:
	ep93xx_rx();

	wrl(ep, REG_INTEN, REG_INTEN_TX | REG_INTEN_RX);	// enable RX int
	if (ep93xx_have_more_rx(ep)) {
		wrl(ep, REG_INTEN, REG_INTEN_TX);	// disable RX int
		wrl(ep, REG_INTSTSP, REG_INTSTS_RX);	// clear RX int

		goto poll_some_more;
	}
}

/* low_level_output starts the transmission of a packet in a (possibly chained)
 * pbuf. */
static err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
  assert(p->tot_len <= netif->mtu);
  assert(netif->mtu <= MAX_PKT_SIZE);

  /* We are forced to copy the data here, because the data may be
  deallocated as soon as we return.
  lwIP provides no mechanism to defer the deallocation. */

  if (ep->tx_pending >= TX_QUEUE_ENTRIES)
    // There are no buffers available.
    return ERR_MEM;

#if ETH_PAD_SIZE
  pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

  struct ep93xx_priv * ep = &theEp;
  int entry = ep->tx_pointer;
  uint8_t * bp = ep->tx_buf[entry];

  struct pbuf * q = p;
  while (1) {
    int len = q->len;	// The size of the data in the pbuf

    DEBUG(tx) kprintf(KR_OSTREAM,
                "Sending pbuf %#x payload %#x len %d entry %d\n",
			 q, q->payload, len, entry);

    memcpy(bp, q->payload, len);
    bp += len;

    if (q->tot_len == len)
      break;	// last pbuf of the packet
    q = q->next;
  }
  ep->descs->tdesc[entry].tdesc1 = TDESC1_EOF
    	| (entry << 16) | (p->tot_len & 0xfff);

  ep->tx_pointer = (ep->tx_pointer + 1) & (TX_QUEUE_ENTRIES - 1);
  ep->tx_pending++;

  wrl(ep, REG_TXDENQ, 1);	// add 1 to number of descrs in queue

#if ETH_PAD_SIZE
  pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
  
  LINK_STATS_INC(link.xmit);

  return ERR_OK;
}

static void ep93xx_tx_complete(void)
{
	struct ep93xx_priv * ep = &theEp;

	while (1) {
		int entry = ep->tx_clean_pointer;
		struct ep93xx_tstat * tstat = ep->descs->tstat + entry;

		u32 tstat0 = tstat->tstat0;
		if (!(tstat0 & TSTAT0_TXFP))
			break;	// frame not processed yet

		DEBUG(tx) printk("tx completed entry=%d tstat0=%#x, descs PA=%#x VA=%#x\n",
				entry, tstat0, ep->descs_dma_addr, ep->descs);
		assert(ep->tx_pending > 0);

		if (tstat0 & TSTAT0_FA)
			printk(KERN_CRIT "ep93xx_tx_complete: frame aborted "
					 " %.8x\n", tstat0);
#if 1	// Disable this if we transmit multiple buffers for one frame.
		if ((tstat0 & TSTAT0_BUFFER_INDEX) != entry)
			printk(KERN_CRIT "ep93xx_tx_complete: entry mismatch "
					 " %.8x\n", tstat0);
#endif

		tstat->tstat0 = 0;

		// Track statistics:
		if (tstat0 & TSTAT0_TXWE) {
			int length = ep->descs->tdesc[entry].tdesc1 & 0xfff;

			ep->stats.tx_packets++;
			ep->stats.tx_bytes += length;
		} else {
			ep->stats.tx_errors++;
		}

		if (tstat0 & TSTAT0_OW)
			ep->stats.tx_window_errors++;
		if (tstat0 & TSTAT0_TXU)
			ep->stats.tx_fifo_errors++;
		ep->stats.collisions += (tstat0 >> 16) & 0x1f;

		ep->tx_clean_pointer = (entry + 1) & (TX_QUEUE_ENTRIES - 1);
		ep->tx_pending--;
	}
}

static void
ep93xx_do_irq(u32 status)
{
	DEBUG(tx) printk("ep93xx_do_irq: status=%#x\n", status);

	if (status & REG_INTSTS_RX)
		ep93xx_poll();

	if (status & REG_INTSTS_TX)
		ep93xx_tx_complete();
}

static irqreturn_t ep93xx_irq(int irq, void *dev_id)
{
	struct ep93xx_priv * ep = &theEp;
	u32 status;

	status = rdl(ep, REG_INTSTSC);	// read int status and clear
	if (status == 0)
		return IRQ_NONE;

	/* For concurrency control, do interrupt work in the main thread. */
	/* An alternative design would be to use a semaphore. */
	capros_IPInt_processInterrupt(KR_DeviceEntry,
				(uint32_t)&ep93xx_do_irq, status);

	return IRQ_HANDLED;
}

static void ep93xx_free_buffers(struct ep93xx_priv *ep)
{
	int i;

	for (i = 0; i < RX_QUEUE_ENTRIES; i += 2) {
		if (ep->rx_buf[i] != NULL)
			dma_free_coherent(NULL, PAGE_SIZE, ep->rx_buf[i],
					ep->descs->rdesc[i].buf_addr);
	}

	for (i = 0; i < TX_QUEUE_ENTRIES; i += 2) {
		if (ep->tx_buf[i] != NULL)
			dma_free_coherent(NULL, PAGE_SIZE, ep->tx_buf[i],
					ep->descs->tdesc[i].buf_addr);
	}

	dma_free_coherent(NULL, sizeof(struct ep93xx_descs), ep->descs,
							ep->descs_dma_addr);
}

/*
 * The hardware enforces a sub-2K maximum packet size (namely 1500),
 * so we put two buffers on every hardware page.
 */
static int ep93xx_alloc_buffers(struct ep93xx_priv *ep)
{
	int i;

#define eth_dma_mask 0xffffffff
	ep->descs = capros_dma_alloc_coherent(eth_dma_mask,
				sizeof(struct ep93xx_descs),
				&ep->descs_dma_addr);
	if (ep->descs == NULL)
		return 1;
	memset(ep->descs, 0, sizeof(struct ep93xx_descs));

	for (i = 0; i < RX_QUEUE_ENTRIES; i += 2) {
		void *page;
		dma_addr_t d;

#if 0 // CapROS
		page = (void *)__get_free_page(GFP_KERNEL | GFP_DMA);
		if (page == NULL)
			goto err;

		d = dma_map_single(NULL, page, PAGE_SIZE, DMA_FROM_DEVICE);
		if (dma_mapping_error(d)) {
			free_page((unsigned long)page);
			goto err;
		}
#else
		page = capros_dma_alloc_coherent(eth_dma_mask, PAGE_SIZE, &d);
		if (page == NULL)
			goto err;
#endif

		ep->rx_buf[i] = page;
		ep->descs->rdesc[i].buf_addr = d;
		ep->descs->rdesc[i].rdesc1 = (i << 16) | PKT_BUF_SIZE;

		ep->rx_buf[i + 1] = page + PKT_BUF_SIZE;
		ep->descs->rdesc[i + 1].buf_addr = d + PKT_BUF_SIZE;
		ep->descs->rdesc[i + 1].rdesc1 = ((i + 1) << 16) | PKT_BUF_SIZE;
	}

	for (i = 0; i < TX_QUEUE_ENTRIES; i += 2) {
		void *page;
		dma_addr_t d;

#if 0 // CapROS
		page = (void *)__get_free_page(GFP_KERNEL | GFP_DMA);
		if (page == NULL)
			goto err;

		d = dma_map_single(NULL, page, PAGE_SIZE, DMA_TO_DEVICE);
		if (dma_mapping_error(d)) {
			free_page((unsigned long)page);
			goto err;
		}
#else
		page = capros_dma_alloc_coherent(eth_dma_mask, PAGE_SIZE, &d);
		if (page == NULL)
			goto err;
#endif

		ep->tx_buf[i] = page;
		ep->descs->tdesc[i].buf_addr = d;

		ep->tx_buf[i + 1] = page + PKT_BUF_SIZE;
		ep->descs->tdesc[i + 1].buf_addr = d + PKT_BUF_SIZE;
	}

	return 0;

err:
	ep93xx_free_buffers(ep);
	return 1;
}

static int ep93xx_start_hw(void)
{
	struct ep93xx_priv * ep = &theEp;
	unsigned long addr;
	int i;

	wrl(ep, REG_SELFCTL, REG_SELFCTL_RESET);
	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_SELFCTL) & REG_SELFCTL_RESET) == 0)
			break;
		msleep(1);
	}

	if (i == 10) {
		printk(KERN_CRIT DRV_MODULE_NAME ": hw failed to reset\n");
		return 1;
	}

	wrl(ep, REG_SELFCTL, ((ep->mdc_divisor - 1) << 9));

	/* Does the PHY support preamble suppress?  */
	if ((ep93xx_mdio_read(NULL, ep93xx_phyad, MII_BMSR) & 0x0040) != 0)
		wrl(ep, REG_SELFCTL, ((ep->mdc_divisor - 1) << 9) | (1 << 8));

	/* Receive descriptor ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, rdesc);
	wrl(ep, REG_RXDQBADD, addr);
	wrl(ep, REG_RXDCURADD, addr);
	wrw(ep, REG_RXDQBLEN, RX_QUEUE_ENTRIES * sizeof(struct ep93xx_rdesc));

	/* Receive status ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, rstat);
	wrl(ep, REG_RXSTSQBADD, addr);
	wrl(ep, REG_RXSTSQCURADD, addr);
	wrw(ep, REG_RXSTSQBLEN, RX_QUEUE_ENTRIES * sizeof(struct ep93xx_rstat));

	/* Transmit descriptor ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, tdesc);
	wrl(ep, REG_TXDQBADD, addr);
	wrl(ep, REG_TXDQCURADD, addr);
	wrw(ep, REG_TXDQBLEN, TX_QUEUE_ENTRIES * sizeof(struct ep93xx_tdesc));

	/* Transmit status ring.  */
	addr = ep->descs_dma_addr + offsetof(struct ep93xx_descs, tstat);
	wrl(ep, REG_TXSTSQBADD, addr);
	wrl(ep, REG_TXSTSQCURADD, addr);
	wrw(ep, REG_TXSTSQBLEN, TX_QUEUE_ENTRIES * sizeof(struct ep93xx_tstat));

	wrl(ep, REG_BMCTL, REG_BMCTL_ENABLE_TX | REG_BMCTL_ENABLE_RX);
	wrl(ep, REG_INTEN, REG_INTEN_TX | REG_INTEN_RX);	// enable RX int
	wrl(ep, REG_GIINTMSK, 0);

	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_BMSTS) & REG_BMSTS_RX_ACTIVE) != 0)
			break;
		msleep(1);
	}

	if (i == 10) {
		printk(KERN_CRIT DRV_MODULE_NAME ": hw failed to start\n");
		return 1;
	}

	wrl(ep, REG_RXDENQ, RX_QUEUE_ENTRIES);
	wrl(ep, REG_RXSTSENQ, RX_QUEUE_ENTRIES);

	wrl(ep, REG_AFP, 0);

	wrl(ep, REG_MAXFRMLEN, (MAX_PKT_SIZE << 16) | MAX_PKT_SIZE);

	wrl(ep, REG_RXCTL, REG_RXCTL_DEFAULT);
	wrl(ep, REG_TXCTL, REG_TXCTL_ENABLE);

	return 0;
}

static void ep93xx_stop_hw(void)
{
	struct ep93xx_priv * ep = &theEp;
	int i;

	wrl(ep, REG_SELFCTL, REG_SELFCTL_RESET);
	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_SELFCTL) & REG_SELFCTL_RESET) == 0)
			break;
		msleep(1);
	}

	if (i == 10)
		printk(KERN_CRIT DRV_MODULE_NAME ": hw failed to reset\n");
}

static int ep93xx_open(void)
{
	struct ep93xx_priv * ep = &theEp;
	int err;

	if (ep93xx_alloc_buffers(ep))
		return -ENOMEM;

	if (ep93xx_start_hw()) {
		ep93xx_free_buffers(ep);
		return -EIO;
	}

	ep->rx_pointer = 0;
	ep->tx_clean_pointer = 0;
	ep->tx_pointer = 0;
	ep->tx_pending = 0;

	err = request_irq(IRQ_EP93XX_ETHERNET, ep93xx_irq, 0,
			 NULL, NULL);
	if (err) {
		ep93xx_stop_hw();
		ep93xx_free_buffers(ep);
		return err;
	}

	wrl(ep, REG_GIINTMSK, REG_GIINTMSK_ENABLE);

	return 0;
}

#if 0
static int ep93xx_close(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);

	netif_stop_queue(dev);

	wrl(ep, REG_GIINTMSK, 0);
	free_irq(IRQ_EP93XX_ETHERNET, dev);
	ep93xx_stop_hw();
	ep93xx_free_buffers(ep);

	return 0;
}
#endif

#if 0 // CapROS
static int ep93xx_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	struct mii_ioctl_data *data = if_mii(ifr);

	return generic_mii_ioctl(&ep->mii, data, cmd, NULL);
}
#endif

static int ep93xx_mdio_read(struct net_device *dev, int phy_id, int reg)
{
	struct ep93xx_priv * ep = &theEp;
	int data;
	int i;

	wrl(ep, REG_MIICMD, REG_MIICMD_READ | (ep93xx_phyad << 5) | reg);

	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_MIISTS) & REG_MIISTS_BUSY) == 0)
			break;
		msleep(1);
	}

	if (i == 10) {
		printk(KERN_INFO DRV_MODULE_NAME ": mdio read timed out\n");
		data = 0xffff;
	} else {
		data = rdl(ep, REG_MIIDATA);
	}

	return data;
}

static void ep93xx_mdio_write(struct net_device *dev, int phy_id, int reg, int data)
{
	struct ep93xx_priv * ep = &theEp;
	int i;

	wrl(ep, REG_MIIDATA, data);
	wrl(ep, REG_MIICMD, REG_MIICMD_WRITE | (ep93xx_phyad << 5) | reg);

	for (i = 0; i < 10; i++) {
		if ((rdl(ep, REG_MIISTS) & REG_MIISTS_BUSY) == 0)
			break;
		msleep(1);
	}

	if (i == 10)
		printk(KERN_INFO DRV_MODULE_NAME ": mdio write timed out\n");
}

#if 0
static int ep93xx_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	return mii_ethtool_gset(&ep->mii, cmd);
}

static int ep93xx_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	return mii_ethtool_sset(&ep->mii, cmd);
}

static int ep93xx_nway_reset(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	return mii_nway_restart(&ep->mii);
}

static u32 ep93xx_get_link(struct net_device *dev)
{
	struct ep93xx_priv *ep = netdev_priv(dev);
	return mii_link_ok(&ep->mii);
}

static struct ethtool_ops ep93xx_ethtool_ops = {
	.get_settings		= ep93xx_get_settings,
	.set_settings		= ep93xx_set_settings,
	.nway_reset		= ep93xx_nway_reset,
	.get_link		= ep93xx_get_link,
};
#endif

#if 0
struct net_device *ep93xx_dev_alloc(struct ep93xx_eth_data *data)
{
	struct net_device *dev;

	dev = alloc_etherdev(sizeof(struct ep93xx_priv));
	if (dev == NULL)
		return NULL;

	dev->get_stats = ep93xx_get_stats;
	dev->ethtool_ops = &ep93xx_ethtool_ops;
	dev->poll = ep93xx_poll;
	dev->hard_start_xmit = ep93xx_xmit;
	dev->open = ep93xx_open;
	dev->stop = ep93xx_close;
	dev->do_ioctl = NULL;	// ep93xx_ioctl;

	dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM;
	dev->weight = 64;

	return dev;
}
#endif


#if 0
static int ep93xx_eth_remove(struct platform_device *pdev)
{
	struct net_device *dev;
	struct ep93xx_priv *ep;

	dev = platform_get_drvdata(pdev);
	if (dev == NULL)
		return 0;
	platform_set_drvdata(pdev, NULL);

	ep = netdev_priv(dev);

	/* @@@ Force down.  */
	unregister_netdev(dev);
	ep93xx_free_buffers(ep);

	free_netdev(dev);

	return 0;
}


static struct platform_driver ep93xx_eth_driver = {
	.probe		= ep93xx_eth_probe,
	.remove		= ep93xx_eth_remove,
	.driver		= {
		.name	= "ep93xx-eth",
	},
};
#endif

/* This function should be passed as a parameter to netif_add().
 * It will be called to bring up the interface. */
err_t
ep93xxDevInitF(struct netif * netif)
{
  int ret;
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

#if 0
  platform_device_register(&edb9315a_eth_device);

  // device_add eventually does the following:
  edb9315a_eth_device.dev.driver = &ep93xx_eth_driver.driver;
  ret = ep93xx_eth_driver.driver.probe(&edb9315a_eth_device.dev);
  assert(!ret);

static int ep93xx_eth_probe(struct platform_device *pdev)
{
	struct ep93xx_eth_data *data;
	struct net_device *dev;
	struct ep93xx_priv *ep;
	int err;

	if (pdev == NULL)
		return -ENODEV;
	data = pdev->dev.platform_data;

	dev = ep93xx_dev_alloc(data);
	if (dev == NULL) {
		err = -ENOMEM;
		goto err_out;
	}
	ep = netdev_priv(dev);

	platform_set_drvdata(pdev, dev);
#endif

	ep->mii.phy_id = ep93xx_phyad;
	ep->mii.phy_id_mask = 0x1f;
	ep->mii.reg_num_mask = 0x1f;
	ep->mii.dev = NULL;//dev;
	ep->mii.mdio_read = ep93xx_mdio_read;
	ep->mii.mdio_write = ep93xx_mdio_write;
	ep->mdc_divisor = 40;	/* Max HCLK 100 MHz, min MDIO clk 2.5 MHz.  */

#if 0
	err = register_netdev(dev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register netdev\n");
		goto err_out;
	}

	return 0;

err_out:
	ep93xx_eth_remove(pdev);
	return err;
}
#endif

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
