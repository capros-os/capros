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
 * Copyright (C) 2008-2010, Strawberry Development Group
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
#include <eros/Invoke.h>
#include <idl/capros/IPInt.h>

#include <lwipCap.h>
#include "../../cap.h"

#define dbg_tx     0x1
#define dbg_rx     0x2
#define dbg_errors 0x4

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u | dbg_errors )

#define DEBUG(x) if (dbg_##x & dbg_flags)

extern void __iomem * hwBaseAddr;
extern unsigned int ep93xx_phyad;	// PHY address

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

	unsigned int		rx_pointer;

	/* tx_clean_pointer is index of the beginning of pending entries
	   in tx_buf/descs.t*.
           tx_pointer is the index of the end of pending entries
	   in tx_buf/descs.t*.
	   tx_pending is the number of pending entries.
	   0 <= tx_pending <= TX_QUEUE_ENTRIES
	   "Pending entries" are ones being sent by the descriptor processor.
	 */
	unsigned int		tx_clean_pointer;
	unsigned int		tx_pointer;
	// spinlock_t		tx_pending_lock;
	unsigned int		tx_pending;

	struct net_device	*dev;
	//struct napi_struct	napi;

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

int ep93xx_mdio_read(struct net_device *dev, int phy_id, int reg);

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

		if (!(rstat0 & RSTAT0_RFP) || !(rstat1 & RSTAT1_RFP))
			break;

		DEBUG(rx) printk("ep93xx_rx rcvd entry %d st0=%#x st1=%#x len=%d iplen=%d\n",
			entry, rstat0, rstat1, length,
			((uint8_t *)ep->rx_buf[entry])[17]);

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
			DEBUG(errors) kdprintf(KR_OSTREAM,
			  "rstat0 & RSTAT0_RWE\n");
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
#if 0 // CapROS
		skb = dev_alloc_skb(length + 2);
		if (likely(skb != NULL)) {
			skb_reserve(skb, 2);
			dma_sync_single(NULL, ep->descs->rdesc[entry].buf_addr,
						length, DMA_FROM_DEVICE);
			skb_copy_to_linear_data(skb, ep->rx_buf[entry], length);
			skb_put(skb, length);
			skb->protocol = eth_type_trans(skb, dev);

			netif_receive_skb(skb);

			ep->stats.rx_packets++;
			ep->stats.rx_bytes += length;
		} else {
			ep->stats.rx_dropped++;
		}
#else
		ethInput(ep->rx_buf[entry], length);
#endif // CapROS

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
#if 0 // CapROS
	rx = ep93xx_rx(dev, rx, budget);
	if (rx < budget) {
		int more = 0;

		spin_lock_irq(&ep->rx_lock);
		__napi_complete(napi);
		wrl(ep, REG_INTEN, REG_INTEN_TX | REG_INTEN_RX);
		if (ep93xx_have_more_rx(ep)) {
			wrl(ep, REG_INTEN, REG_INTEN_TX);
			wrl(ep, REG_INTSTSP, REG_INTSTS_RX);
			more = 1;
		}
		spin_unlock_irq(&ep->rx_lock);

		if (more && napi_reschedule(napi))
			goto poll_some_more;
	}

	return rx;
#else
	ep93xx_rx();

	wrl(ep, REG_INTEN, REG_INTEN_TX | REG_INTEN_RX);	// enable RX int
	if (ep93xx_have_more_rx(ep)) {
		wrl(ep, REG_INTEN, REG_INTEN_TX);	// disable RX int
		wrl(ep, REG_INTSTSP, REG_INTSTS_RX);	// clear RX int

		goto poll_some_more;
	}
#endif // CapROS
}

/* low_level_output starts the transmission of a packet in a (possibly chained)
 * pbuf. */
err_t
low_level_output(struct netif *netif, struct pbuf *p)
{
  assert(netif->mtu + sizeof(struct eth_hdr) <= MAX_PKT_SIZE);

  if (ep->tx_pending >= TX_QUEUE_ENTRIES)
    // There are no buffers available.
    return ERR_MEM;

  struct ep93xx_priv * ep = &theEp;
  int entry = ep->tx_pointer;

  ethOutput(netif, p, ep->tx_buf[entry]);

  ep->descs->tdesc[entry].tdesc1 = TDESC1_EOF
    	| (entry << 16) | (p->tot_len & 0xfff);

  ep->tx_pointer = (ep->tx_pointer + 1) & (TX_QUEUE_ENTRIES - 1);
  ep->tx_pending++;

  wrl(ep, REG_TXDENQ, 1);	// add 1 to number of descrs in queue

  return ERR_OK;
}

static void ep93xx_tx_complete(void)
{
	struct ep93xx_priv * ep = &theEp;

	while (1) {
		int entry;
		struct ep93xx_tstat *tstat;
		u32 tstat0;

		entry = ep->tx_clean_pointer;
		tstat = ep->descs->tstat + entry;

		tstat0 = tstat->tstat0;
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

static uint32_t
ep93xx_do_irq(u32 status)
{
	DEBUG(tx) printk("ep93xx_do_irq: status=%#x\n", status);

	if (status & REG_INTSTS_RX)
		ep93xx_poll();

	if (status & REG_INTSTS_TX)
		ep93xx_tx_complete();

	return IRQ_HANDLED;
}

static irqreturn_t ep93xx_irq(int irq, void *dev_id)
{
	struct ep93xx_priv * ep = &theEp;
	u32 status;

	status = rdl(ep, REG_INTSTSC);	// read int status and clear
	if (status == 0)
		return IRQ_NONE;

#if 0 // CapROS
	if (status & REG_INTSTS_RX) {
		spin_lock(&ep->rx_lock);
		if (likely(napi_schedule_prep(&ep->napi))) {
			wrl(ep, REG_INTEN, REG_INTEN_TX);
			__napi_schedule(&ep->napi);
		}
		spin_unlock(&ep->rx_lock);
	}

	if (status & REG_INTSTS_TX)
		ep93xx_tx_complete(dev);

	return IRQ_HANDLED;
#else
	/* For concurrency control, do interrupt work in the main thread. */
	/* An alternative design would be to use a semaphore. */
	uint32_t ret;
	capros_IPInt_processInterrupt(KR_DeviceEntry,
				(uint32_t)&ep93xx_do_irq, status, &ret);

	return ret;
#endif // CapROS
}

static void ep93xx_free_buffers(struct ep93xx_priv *ep)
{
	int i;

	for (i = 0; i < RX_QUEUE_ENTRIES; i += 2) {
#if 0 // CapROS
		dma_addr_t d;

		d = ep->descs->rdesc[i].buf_addr;
		if (d)
			dma_unmap_single(NULL, d, PAGE_SIZE, DMA_FROM_DEVICE);

#else
		if (ep->rx_buf[i] != NULL)
			dma_free_coherent(NULL, PAGE_SIZE, ep->rx_buf[i],
					ep->descs->rdesc[i].buf_addr);
#endif // CapROS
	}

	for (i = 0; i < TX_QUEUE_ENTRIES; i += 2) {
#if 0 // CapROS
		dma_addr_t d;

		d = ep->descs->tdesc[i].buf_addr;
		if (d)
			dma_unmap_single(NULL, d, PAGE_SIZE, DMA_TO_DEVICE);

#else
		if (ep->tx_buf[i] != NULL)
			dma_free_coherent(NULL, PAGE_SIZE, ep->tx_buf[i],
					ep->descs->tdesc[i].buf_addr);
#endif // CapROS
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

	ep->descs = dma_alloc_coherent(NULL,
				sizeof(struct ep93xx_descs),
				&ep->descs_dma_addr, 0);
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
		if (dma_mapping_error(NULL, d)) {
			free_page((unsigned long)page);
			goto err;
		}
#else
		page = dma_alloc_coherent(NULL, PAGE_SIZE, &d, 0);
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
		if (dma_mapping_error(NULL, d)) {
			free_page((unsigned long)page);
			goto err;
		}
#else
		page = dma_alloc_coherent(NULL, PAGE_SIZE, &d, 0);
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

#if 0 // CapROS
	wrb(ep, REG_INDAD0, dev->dev_addr[0]);
	wrb(ep, REG_INDAD1, dev->dev_addr[1]);
	wrb(ep, REG_INDAD2, dev->dev_addr[2]);
	wrb(ep, REG_INDAD3, dev->dev_addr[3]);
	wrb(ep, REG_INDAD4, dev->dev_addr[4]);
	wrb(ep, REG_INDAD5, dev->dev_addr[5]);
#endif // CapROS
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

int ep93xx_open(void)
{
	struct ep93xx_priv * ep = &theEp;
	int err;

	if (ep93xx_alloc_buffers(ep))
		return -ENOMEM;

#if 0 // CapROS
	if (is_zero_ether_addr(dev->dev_addr)) {
		random_ether_addr(dev->dev_addr);
		printk(KERN_INFO "%s: generated random MAC address "
			"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x.\n", dev->name,
			dev->dev_addr[0], dev->dev_addr[1],
			dev->dev_addr[2], dev->dev_addr[3],
			dev->dev_addr[4], dev->dev_addr[5]);
	}

	napi_enable(&ep->napi);
#endif // CapROS

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

	napi_disable(&ep->napi);
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

int ep93xx_mdio_read(struct net_device *dev, int phy_id, int reg)
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

void ep93xx_mdio_write(struct net_device *dev, int phy_id, int reg, int data)
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
static void ep93xx_get_drvinfo(struct net_device *dev, struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_MODULE_NAME);
	strcpy(info->version, DRV_MODULE_VERSION);
}

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

static const struct net_device_ops ep93xx_netdev_ops = {
	.ndo_open		= ep93xx_open,
	.ndo_stop		= ep93xx_close,
	.ndo_start_xmit		= ep93xx_xmit,
	.ndo_get_stats		= ep93xx_get_stats,
	.ndo_do_ioctl		= ep93xx_ioctl,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_change_mtu		= eth_change_mtu,
	.ndo_set_mac_address	= eth_mac_addr,
};
#endif

#if 0
static struct net_device *ep93xx_dev_alloc(struct ep93xx_eth_data *data)
{
	struct net_device *dev;

	dev = alloc_etherdev(sizeof(struct ep93xx_priv));
	if (dev == NULL)
		return NULL;

	memcpy(dev->dev_addr, data->dev_addr, ETH_ALEN);

	dev->ethtool_ops = &ep93xx_ethtool_ops;
	dev->netdev_ops = &ep93xx_netdev_ops;

	dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM;

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

	if (ep->base_addr != NULL)
		iounmap(ep->base_addr);

	if (ep->res != NULL) {
		release_resource(ep->res);
		kfree(ep->res);
	}

	free_netdev(dev);

	return 0;
}
#endif // CapROS

void init_mii(void)
{
  theEp.mii.phy_id = ep93xx_phyad;
  theEp.mii.phy_id_mask = 0x1f;
  theEp.mii.reg_num_mask = 0x1f;
  theEp.mii.dev = NULL;//dev;
  theEp.mii.mdio_read = ep93xx_mdio_read;
  theEp.mii.mdio_write = ep93xx_mdio_write;
  theEp.mdc_divisor = 40;	/* Max HCLK 100 MHz, min MDIO clk 2.5 MHz.  */
}

#if 0 // CapROS
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
	ep->dev = dev;
	netif_napi_add(dev, &ep->napi, ep93xx_poll, 64);

	platform_set_drvdata(pdev, dev);

	ep->res = request_mem_region(pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start + 1,
			dev_name(&pdev->dev));
	if (ep->res == NULL) {
		dev_err(&pdev->dev, "Could not reserve memory region\n");
		err = -ENOMEM;
		goto err_out;
	}

	ep->base_addr = ioremap(pdev->resource[0].start,
			pdev->resource[0].end - pdev->resource[0].start);
	if (ep->base_addr == NULL) {
		dev_err(&pdev->dev, "Failed to ioremap ethernet registers\n");
		err = -EIO;
		goto err_out;
	}
	ep->irq = pdev->resource[1].start;

	init_mii();	// ep == &theEp

	err = register_netdev(dev);
	if (err) {
		dev_err(&pdev->dev, "Failed to register netdev\n");
		goto err_out;
	}

	printk(KERN_INFO "%s: ep93xx on-chip ethernet, IRQ %d, "
			 "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x.\n", dev->name,
			ep->irq, data->dev_addr[0], data->dev_addr[1],
			data->dev_addr[2], data->dev_addr[3],
			data->dev_addr[4], data->dev_addr[5]);

	return 0;

err_out:
	ep93xx_eth_remove(pdev);
	return err;
}

static struct platform_driver ep93xx_eth_driver = {
	.probe		= ep93xx_eth_probe,
	.remove		= ep93xx_eth_remove,
	.driver		= {
		.name	= "ep93xx-eth",
		.owner	= THIS_MODULE,
	},
};

static int __init ep93xx_eth_init_module(void)
{
	printk(KERN_INFO DRV_MODULE_NAME " version " DRV_MODULE_VERSION " loading\n");
	return platform_driver_register(&ep93xx_eth_driver);
}

static void __exit ep93xx_eth_cleanup_module(void)
{
	platform_driver_unregister(&ep93xx_eth_driver);
}

module_init(ep93xx_eth_init_module);
module_exit(ep93xx_eth_cleanup_module);
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:ep93xx-eth");
#endif
