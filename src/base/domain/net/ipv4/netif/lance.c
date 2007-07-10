/* lance.c: An AMD LANCE/PCnet ethernet driver for Linux. */
/*
	Written/copyright 1993-1998 by Donald Becker.

	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.
	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	This driver is for the Allied Telesis AT1500 and HP J2405A, and should work
	with most other LANCE-based bus-master (NE2100/NE2500) ethercards.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Andrey V. Savochkin:
	- alignment problem with 1.3.* kernel and some minor changes.
	Thomas Bogendoerfer (tsbogend@bigbug.franken.de):
	- added support for Linux/Alpha, but removed most of it, because
        it worked only for the PCI chip. 
      - added hook for the 32bit lance driver
      - added PCnetPCI II (79C970A) to chip table
	Paul Gortmaker (gpg109@rsphy1.anu.edu.au):
	- hopefully fix above so Linux/Alpha can use ISA cards too.
    8/20/96 Fixed 7990 autoIRQ failure and reversed unneeded alignment -djb
    v1.12 10/27/97 Module support -djb
    v1.14  2/3/98 Module support modified, made PCI support optional -djb
    v1.15 5/27/99 Fixed bug in the cleanup_module(). dev->priv was freed
                  before unregister_netdev() which caused NULL pointer
                  reference later in the chain (in rtnetlink_fill_ifinfo())
                  -- Mika Kuoppala <miku@iki.fi>
    
    Forward ported v1.14 to 2.1.129, merged the PCI and misc changes from
    the 2.1 version of the old driver - Alan Cox

    Get rid of check_region, check kmalloc return in lance_probe1
    Arnaldo Carvalho de Melo <acme@conectiva.com.br> - 11/01/2001
*/
/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System distribution.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/* Network card driver for lance - AMD 79C970A (VMWare compatible). */

#include <stddef.h>

#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>

#include <idl/capros/key.h>
#include <idl/capros/DevPrivs.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/Number.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/PciProbeKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/MemmapKey.h>
#include <domain/drivers/NetKey.h>

#include <string.h>

#include "constituents.h"
#include "lance.h"
#include "netif.h"
#include "netutils.h"
#include "../keyring.h"

#include "../include/pbuf.h"
#include "../include/ethernet.h"
#include "../include/ip.h"
#include "../include/etharp.h"

#define DEBUGF if(0)

/* From VMWare Specs - The virtualized ethernet card is an AMD PCnet II
 * based on 79C970A chip. Id = 0x2621. */
#define VENDOR_AMD      0x1022
#define AMD79C970A      0x2621

#define ETH_FRAME_LEN    1518

/* Set the number of Tx and Rx buffers, using Log_2(# buffers).
 * Reasonable default values are 4 Tx buffers, and 16 Rx buffers.
 * That translates to 2 (4 == 2^^2) and 4 (16 == 2^^4).*/
#define LANCE_LOG_TX_BUFFERS 4
#define LANCE_LOG_RX_BUFFERS 4

#define TX_RING_SIZE         (1 << (LANCE_LOG_TX_BUFFERS))
#define TX_RING_MOD_MASK     (TX_RING_SIZE - 1)
#define TX_RING_LEN_BITS     ((LANCE_LOG_TX_BUFFERS) << 12)

#define RX_RING_SIZE         (1 << (LANCE_LOG_RX_BUFFERS))
#define RX_RING_MOD_MASK     (RX_RING_SIZE - 1)
#define RX_RING_LEN_BITS     ((LANCE_LOG_RX_BUFFERS) << 4)

#define PKT_BUF_SZ           ETH_FRAME_LEN

/* Offsets from base I/O address. */
#define LANCE_DATA       0x10
#define LANCE_ADDR       0x12
#define LANCE_RESET      0x14
#define LANCE_BUS_IF     0x16
#define LANCE_TOTAL_SIZE 0x18

#define DMA_START        0x80000000u /* 2 GB into address space */
#define DMA_SIZE         0x20000u    /* DMA Size to be dev publish*/

/* The LANCE Rx & Tx ring Descriptors */
struct lance_rx_head {
  uint32_t base;
  short buf_length;
  short status;
  uint32_t msg_length;
  uint32_t reserved;
};

struct lance_tx_head {
  uint32_t base;
  short  length;
  short status;
  uint32_t misc;
  uint32_t reserved;
};

struct enet_statistics{
  unsigned long rx_packets;       /* total packets received       */
  unsigned long tx_packets;       /* total packets transmitted    */
  unsigned long rx_bytes;         /* total bytes received         */
  unsigned long tx_bytes;         /* total bytes transmitted      */
  unsigned long rx_errors;        /* bad packets received         */
  unsigned long tx_errors;        /* packet transmit problems     */
  unsigned long rx_dropped;       /* no space in linux buffers    */
  unsigned long tx_dropped;       /* no space available in linux  */
  unsigned long multicast;        /* multicast packets received   */
  unsigned long collisions;

  /* detailed rx_errors: */
  unsigned long rx_length_errors;
  unsigned long rx_over_errors;    /* receiver ring buff overflow  */
  unsigned long rx_crc_errors;     /* recved pkt with crc error    */
  unsigned long rx_frame_errors;   /* recv'd frame alignment error */
  unsigned long rx_fifo_errors;    /* recv'r fifo overrun          */
  unsigned long rx_missed_errors;  /* receiver missed packet       */
  
  /* detailed tx_errors */
  unsigned long tx_aborted_errors;
  unsigned long tx_carrier_errors;
  unsigned long tx_fifo_errors;
  unsigned long tx_heartbeat_errors;
  unsigned long tx_window_errors;

};

/* The LANCE 32-bit initialization block from AMD docs */
struct lance_init_block {
  uint16_t mode;
  uint16_t tlen_rlen;    /* TLEN|RESERV|RLEN|RESERV all pieces are 4-bit */
  uint8_t  phys_addr[6];
  uint16_t reserved;
  uint32_t filter[2];
  uint32_t rx_ring;       /* Receive & transmit ring base */
  uint32_t tx_ring;  
};

struct lance_private{
  struct lance_init_block  init_block;
  /* The Tx & Rx ring entries must be aligned on 16-byte boundaries in 
   * 32-bit mode */
  struct lance_rx_head rx_ring[RX_RING_SIZE];
  struct lance_tx_head tx_ring[TX_RING_SIZE];
  unsigned long rx_buffs;    /* Address of Rx & Tx Buffers */
  unsigned char tbuf[ETH_FRAME_LEN];
  int cur_rx,cur_tx;         /* The next free ring entry */
  int dirty_rx,dirty_tx;     /* The ring entries to be free()ed*/
  struct enet_statistics stats;
  char tx_full;
  unsigned long lock;
};

#define virt_to_bus(x) (PHYSADDR + (void*)x - (void*)DMA_START)
#define bus_to_virt(x) (DMA_START + (void*)x - (void*)PHYSADDR)

typedef struct pci_dev_data DEVICE; /* shorthand!*/

/* Globals */
uint16_t fb_lss;           /* Needed for patching up ProcAddrSpace */
uint16_t dma_lss;          /* The lss of our dma region */
uint32_t *dma_abszero;     /* Start address of the dma buffer */
uint32_t PHYSADDR;
struct lance_private *lp;  /* The lance_private is to stay here */
static DEVICE NETDEV;      /* Our singular network adapter */
static struct eth_addr ethbroadcast = {{0xff,0xff,0xff,0xff,0xff,0xff}};

/* Function Prototypes */
void dbgCSR();

int
lance_read(int off, int size)
{
  switch(size) {
  case 2:
    return inw(LANCEADDR(NETDEV.base_address[0], off, size));
    break;
  case 4:
    return inl(LANCEADDR(NETDEV.base_address[0], off, size));
    break;
  default:
    kdprintf(KR_OSTREAM,"LANCEREAD::default size in/out??");
    return 0;
  }
}

int
lance_write(int off, int size,int val)
{
  switch(size) {
  case 2:
    outw(val,LANCEADDR(NETDEV.base_address[0], off, size));
    break;
  case 4:
    outw(val,LANCEADDR(NETDEV.base_address[0], off, size));
    break;
  default:
    kdprintf(KR_OSTREAM,"LANCEWRITE::default size in/out??");
    return 0;
  }
  return 0;
}


/* Read from CSR: Write CSR value into RAP & read from RDP */
int
lance_readcsr(int reg, int size)
{
  lance_write(RAP, size, reg);
  return lance_read(RDP, size);
}


/* Write to CSR: Write CSR value into RAP & read from RDP */
void
lance_writecsr(int reg, int size, int val)
{
  lance_write(RAP, size, reg);
  lance_write(RDP, size, val);
}

/* Read from BCR: Write BCR value into RAP & read instead from BDP */
int
lance_readbcr(int reg, int size)
{
  lance_write(RAP, size, reg);
  return lance_read(BDP, size);
}

/* Write to BCR: Write BCR value into RAP & write instead to BDP*/
void  
lance_writebcr(int reg, int size, int val)
{
  lance_write(RAP, size, reg);
  lance_write(BDP, size, val);
}

/* Debugging info */
void dbgCSR() 
{
  kprintf(KR_OSTREAM,"CSR0=%4x",lance_readcsr(CSR0,2));
  kprintf(KR_OSTREAM,"CSR1=%4x",lance_readcsr(IblockAddr,2));
  kprintf(KR_OSTREAM,"CSR2=%4x",lance_readcsr(IblockAddr+1,2));
}

/* Initialize the lance card
 * 1. Get the Chip ID. Check if it is the AMD LANCE 79C970A */
uint32_t 
lance_init()
{
  int ioaddr = NETDEV.base_address[0];
  int chip_version;
  int i;
  uint32_t RAM_Addr;

  /* check out the chip id. We are still in 16-bit mode. So read out
   * the ChipID CSR & the next CSR */
  chip_version = lance_readcsr(ChipId,2);
  chip_version |= lance_readcsr(ChipId+1,2) << 16;
  
  chip_version = (chip_version >> 12) & 0xffff;
  if(chip_version!=AMD79C970A) {
    kprintf(KR_OSTREAM,"Unsupported Chip %x",chip_version);
    return -1;
  }else {
    kprintf(KR_OSTREAM,"AMD chip %x ... [SUCCESS]",chip_version);
  }
  
  /* Abbrev for lance */
  NETIF.name[0] = 'l'; NETIF.name[1] = 'a'; NETIF.name[2] = '\0';
  
  /* Get the MAC Address */
  NETIF.hwaddr_len = 6; /* ethernet has 6 byte long h/w length */
  for(i = 0; i < 6; i++)  NETIF.hwaddr[i] = inb(ioaddr + i );
  kprintf(KR_OSTREAM, "MAC Addr %02x:%02x:%02x:%02x:%02x:%02x\n",
  	  NETIF.hwaddr[0],NETIF.hwaddr[1],NETIF.hwaddr[2],
	  NETIF.hwaddr[3],NETIF.hwaddr[4],NETIF.hwaddr[5]);
  
  /* Set the MTU */
  NETIF.mtu = 1500; /* Maximum MTU on ethernet */
  
  /* Set up CSR0 & CSR1. For this we need to initialize our data structures */
  /* Make certain the data structures used by the LANCE are 16bit aligned */
  lp = (void *)DMA_START; /* 2 GB into the address space */
  
  lp->rx_buffs = PHYSADDR + 0x10000u;
  lp->init_block.mode = 0x0003; /* Disable Rx & Tx for now */
  lp->init_block.tlen_rlen = TX_RING_LEN_BITS | RX_RING_LEN_BITS;
  lp->init_block.filter[0] = 0x00000000;
  lp->init_block.filter[1] = 0x00000000;
  lp->init_block.rx_ring = virt_to_bus(&lp->rx_ring);
  lp->init_block.tx_ring = virt_to_bus(&lp->tx_ring);
  
  DEBUGF {
    kprintf(KR_OSTREAM,"sizeof IB = %d",sizeof(lp->init_block));
    kprintf(KR_OSTREAM,"rx_ring = %x",lp->init_block.rx_ring);
    kprintf(KR_OSTREAM,"tx_ring = %x",lp->init_block.tx_ring);
  }
  
  /* The MAC  address of the card */
  for(i=0;i<6;i++) lp->init_block.phys_addr[i] = NETIF.hwaddr[i];
   
  /* Switch lance to 32-bit mode */
  lance_writebcr(20,2,0x200);
    
  RAM_Addr = PHYSADDR;  
  lance_writecsr(IblockAddr,2,RAM_Addr&0xffffu);
  lance_writecsr(IblockAddr+1,2,RAM_Addr>>16);
  
  lance_writebcr(20,2,lance_readbcr(20,2)|BIT(1));
  
  lance_writecsr(Imask,2,IDON);
  lance_writecsr(CSR0,2,INIT);
  
  while(!(lance_readcsr(CSR0,2)&IDON)) {
    capros_Sleep_sleep(KR_SLEEP,10);
    dbgCSR();
  }
  lance_writecsr(CSR0,2,IENA|STRT);
  
  return 0;
}


void 
lance_restart(unsigned int csr0_bits) 
{
  int i;
  int ioaddr = NETDEV.base_address[0];
  
  //lance_purge_tx_ring();
  lance_init_ring();
  
  outw(0x0000, ioaddr + LANCE_ADDR);
  
  /* ReInit Ring */
  outw(0x0001, ioaddr + LANCE_DATA);
  i = 0;
  while (i++ < 100)
    if (inw(ioaddr+LANCE_DATA) & 0x0100)
      break;
  
  outw(csr0_bits, ioaddr + LANCE_DATA);
}

uint32_t
lance_open() 
{
  int ioaddr = NETDEV.base_address[0];
  int i;
  uint32_t RAM_Addr;
  
  /* Reset the LANCE */
  inw(ioaddr+LANCE_RESET);
  
  /* Allow some time to wake up */
  capros_Sleep_sleep(KR_SLEEP,200);

  /* Switch LANCE to 32-bit mode */
  outw(0x0014,ioaddr+LANCE_ADDR);
  outw(0x0002,ioaddr+LANCE_BUS_IF);
    
  /* Turn on auto-select of media (AUI, BNC). */
  lance_writebcr(20,2,lance_readbcr(20,2)|BIT(1));
    
  /* Must be 0x0000.promiscuous mode for testing =0x8000*/
  lp->init_block.mode = 0x0000; 
  lp->init_block.filter[0] = 0x00000000;
  lp->init_block.filter[1] = 0x00000000;
  lance_init_ring();
  
  /* Re-initialize the LANCE, and start it when done. */
  RAM_Addr = PHYSADDR;
  lance_writecsr(IblockAddr,2, RAM_Addr & 0xffff);
  lance_writecsr(IblockAddr+1,2, (RAM_Addr >> 16) & 0xffff);

  outw(0x0004, ioaddr+LANCE_ADDR);
  outw(0x0915, ioaddr+LANCE_DATA);
  
  /* Set INIT bit in CSR0 */
  lance_writecsr(CSR0,2,INIT);
  
  i = 0;
  while (i++ < 100)
    if (inw(ioaddr+LANCE_DATA) & 0x0100)
      break;
  
  outw(0x0042, ioaddr+LANCE_DATA);
  
  DEBUGF 
    kprintf(KR_OSTREAM,"LANCE open after %d ticks, csr0 %x",i,
		 inw(ioaddr+LANCE_DATA));

  outw(0x0,ioaddr+LANCE_ADDR);
  outw(0x42,ioaddr+LANCE_DATA);
  
  return 0;
}


/* When a recieve interrupt is raised, this function handler is called.
 * we check for validity of packet, drop it if it is not valid. If valid
 * check if it is for us ( strip ethernet header ) and pass it on to the 
 * appropriate layer above for processing */
inline uint32_t
lance_rx() 
{
  int entry = lp->cur_rx & RX_RING_MOD_MASK;
  int i=0;
  
  /* Keep Check ownership & accept pkt. */
  while(lp->rx_ring[entry].status >= 0) {
    int status = lp->rx_ring[entry].status >> 8;
    
    if(status!=0x03) {
      kprintf(KR_OSTREAM,"Investigate RX Error");
      if (status & 0x01)      /* Only count a general error at the */
	lp->stats.rx_errors++; /* end of a packet.*/
      if (status & 0x20) lp->stats.rx_frame_errors++;
      if (status & 0x10) lp->stats.rx_over_errors++;
      if (status & 0x08) lp->stats.rx_crc_errors++;
      if (status & 0x04) lp->stats.rx_fifo_errors++;
      lp->rx_ring[entry].status &= 0x03ff;
    }else {
      short pkt_len = (lp->rx_ring[entry].msg_length & 0xfff) - 4;
      
      DEBUGF {
	static long pkts;
	pkts++;
	kprintf(KR_OSTREAM,"pkt_len=%d,pkts=%x",pkt_len,pkts);
      }
      if(pkt_len<60) {
	kprintf(KR_OSTREAM,"Error::pkt_len < 60");
	lp->stats.rx_errors++;
	/* Drop packet: Do nothing */
      }else {
	struct pbuf *p,*q;
	uint16_t totlen = 0; 
	char *s;
	struct eth_hdr *ethhdr = NULL;
	
	p = pbuf_alloc(PBUF_RAW,pkt_len,PBUF_POOL);
	if(p!=NULL) {
	  DEBUGF kprintf(KR_OSTREAM,"POOL Allocated ");
	  
	  s = (char *)bus_to_virt(lp->rx_ring[entry].base);
	  /* We iterate over the pbuf chain until we have read the entire
	   * packet into the pbuf. */
	  for(q = p; q != NULL; q = q->next) {
	    /* Read enough bytes to fill this pbuf in the chain. The
	       available data in the pbuf is given by the q->len
	       variable. */
	    for(i=0;i<q->len;i++) {
	      ((char *)q->payload)[i] = s[totlen++];
	    }
	  }
	  
	  /* Now we have the whole packet. Call the appropriate layer 
	   * peeking into the packet (for  protocol no.) */
	  ethhdr = p -> payload;
	  
#if 0 
	  kprintf(KR_OSTREAM,"totlen = %d,Packet type = %x",
		  totlen,htons(ethhdr->type));
#endif
	  switch(htons(ethhdr->type)) {
	  case ETHERTYPE_IP:
	    /* Add to arp table here */
	    etharp_ip_input(&NETIF,p);
	    DEBUGF kprintf(KR_OSTREAM,"lance_rx ip pkt");
	    pbuf_header(p,-14);
	    ip_input(p);
	    break;
	  case ETHERTYPE_ARP:
	    p = etharp_arp_input(&NETIF,(struct eth_addr *)&(NETIF.hwaddr),p);
	    if(p != NULL) {
	      pbuf_free(p); 
	      p = NULL;
	    }
	    break;
	  default:
	    kprintf(KR_OSTREAM,"lance_rx default pkt");
	    /* free pbuf drop packet*/
	    pbuf_free(p);
	    p = NULL;
	    break;
	  }
	  lp->stats.rx_packets++;
	}
	else {
	  kprintf(KR_OSTREAM,"No pufs left???");
	  //break;   /* No pool pbufs left drop packet */
	}
      }
    }
    
    lp->rx_ring[entry].buf_length = -PKT_BUF_SZ;
    lp->rx_ring[entry].status |= 0x8000;
    entry = (++lp->cur_rx) & RX_RING_MOD_MASK;
  }
  
  return 0;
}


/* Handles the ethernet pkt format into this. 
 * If type = -1, send packet as it is received */
uint32_t
lance_start_xmit(struct pbuf *p,int type,struct ip_addr *ipaddr)
{
  int ioaddr = NETDEV.base_address[0];
  int entry = 0;
  int i = 0;
  int totlen = 0;
  struct pbuf *q;
  struct eth_addr mcastaddr,*dest=NULL;
  struct ip_addr *queryaddr;

  
  DEBUGF {
    outw(0x0000, ioaddr+LANCE_ADDR);
    kprintf(KR_OSTREAM,"lance_start_xmit()-csr0 %x",inw(ioaddr+LANCE_DATA));
  }
  
  if(type != -1) {
    /* Construct Ethernet header. Start with looking up deciding which
     * MAC address to use as a destination address. Broadcasts and
     * multicasts are special, all other addresses are looked up in the
     * ARP table. */
    if (ip_addr_isany(ipaddr) ||
	ip_addr_isbroadcast(ipaddr, &(NETIF.netmask))) {
      dest = (struct eth_addr *)&ethbroadcast;
    } else if (ip_addr_ismulticast(ipaddr)) {
      /* Hash IP multicast address to MAC address. */
      mcastaddr.addr[0] = 0x01;
      mcastaddr.addr[1] = 0x0;
      mcastaddr.addr[2] = 0x5e;
      mcastaddr.addr[3] = ip4_addr2(ipaddr) & 0x7f;
      mcastaddr.addr[4] = ip4_addr3(ipaddr);
      mcastaddr.addr[5] = ip4_addr4(ipaddr);
      dest = &mcastaddr;
    }else {
      if (ip_addr_maskcmp(ipaddr, &(NETIF.ip_addr), &(NETIF.netmask))) {
	/* Use destination IP address if the destination is on the same
	 * subnet as we are. */
	queryaddr = ipaddr;
      } else {
	/* Otherwise we use the default router as the address to send
	 * the Ethernet frame to. */
	queryaddr = &(NETIF.gw);
      }
      dest = etharp_lookup(&NETIF,queryaddr,p);
    }
    
    if(dest == NULL) {
      pbuf_free(p);
      return 0;
    }
    
    /* Fill in a Tx ring entry */
    
    /* Mask to ring buffer boundary*/
    entry = lp->cur_tx & TX_RING_MOD_MASK;
    lp->tx_ring[entry].misc = 0x00000000;
    lp->tx_ring[entry].length = -(p->tot_len + ETHER_HDR_LEN);
    for(i=0;i<ETHER_ADDR_LEN;i++) {
      lp->tbuf[i] = dest->addr[i];
      lp->tbuf[i+ETHER_ADDR_LEN] = NETIF.hwaddr[i];
    }
    lp->tbuf[ETHER_ADDR_LEN*2] = type >> 8;
    lp->tbuf[ETHER_ADDR_LEN*2+1] = type;
    totlen = ETHER_HDR_LEN;
  }else {
    entry = lp->cur_tx & TX_RING_MOD_MASK;
    lp->tx_ring[entry].misc = 0x00000000;
    lp->tx_ring[entry].length = -p->tot_len;;
  }
  
  for(q = p; q != NULL; q = q->next) {
    for(i=0;i<q->len;i++) {
      ((char *)lp->tbuf)[totlen++] = ((char *)q->payload)[i];
    }
  }
  
  lp->tx_ring[entry].base = virt_to_bus(&lp->tbuf);
  lp->tx_ring[entry].status = 0x8300;
  
  /* Trigger an immediate send poll */
  outw(0x0000, ioaddr+LANCE_ADDR);
  outw(0x0048, ioaddr+LANCE_DATA);
  
  lp->cur_tx++;
  pbuf_free(p);

  DEBUGF kprintf(KR_OSTREAM,"Sending Lance Packet");

  return 0;
}


/* Interrupt handler */
uint32_t
lance_interrupt() 
{
  int ioaddr = NETDEV.base_address[0];
  int csr0,boguscnt = 10;
  int must_restart;
  
  outw(0x00,ioaddr+LANCE_ADDR);
  while((csr0=inw(ioaddr+LANCE_DATA))&0x8600 && --boguscnt >=0) {
    //kprintf(KR_OSTREAM,"IntH:csr0=%x newcsr=%x",csr0,inw(ioaddr+LANCE_DATA));
    
    /* Acknowledge all of the current interrupt sources ASAP */
    outw(csr0 & ~0x004f,ioaddr + LANCE_DATA);
    
    must_restart = 0;
    
    if (csr0 & 0x0400) {       /* Rx interrupt */
      DEBUGF kprintf(KR_OSTREAM,"******Rx Interrupt*******");
      lance_rx();
    }
    
    if(csr0 & 0x0200) {       /* Tx interrupt */
      DEBUGF kprintf(KR_OSTREAM,"******Tx Interrupt*******");
      
      while(lp->dirty_tx < lp->cur_tx) {
	int entry = lp->dirty_tx & TX_RING_MOD_MASK;
	int status = lp->tx_ring[entry].status;
	
	if(status < 0) 	  break; /* It still hasn't been Txed*/
		
	lp->tx_ring[entry].base = 0;
	
	if(status & 0x4000) {
	  int err_status = lp->tx_ring[entry].misc;
	  DEBUGF kprintf(KR_OSTREAM,"Investigate Tx Error");

	  lp->stats.tx_errors++;
	  if (err_status & 0x04000000) lp->stats.tx_aborted_errors++;
	  if (err_status & 0x08000000) lp->stats.tx_carrier_errors++;
	  if (err_status & 0x10000000) lp->stats.tx_window_errors++;
	  if (err_status & 0x40000000) {
	    /* Ackk!  On FIFO errors the Tx unit is turned off! */
	    lp->stats.tx_fifo_errors++;
	    kprintf(KR_OSTREAM,"Tx FIFO error! Status %4x",csr0);
	    
	    /* Restart the chip. */
	    must_restart = 1;
	  }
	} else {
	  if (status & 0x1800)
	    lp->stats.collisions++;
	  lp->stats.tx_packets++;
	}
	lp->dirty_tx++;
      }
      if (lp->cur_tx - lp->dirty_tx >= TX_RING_SIZE) {
	DEBUGF 
	  kprintf(KR_OSTREAM,"out-of-sync dirty pointer, %d vs. %d, full=%d",
		  lp->dirty_tx, lp->cur_tx, lp->tx_full);

	lp->dirty_tx += TX_RING_SIZE;
      }
      
    }/*End of Tx interrupt handler*/
      
    /* Other Errors */
    if(csr0 & 0x4000) lp->stats.tx_errors++; /* Tx BABL*/
    if(csr0 & 0x1000) lp->stats.rx_errors++; /* Receiver missed a pkt */ 
    
    if(must_restart){
      /* stop the chip & restart */
      outw(0x0000,ioaddr + LANCE_ADDR);
      outw(0x0004,ioaddr + LANCE_DATA);
      lance_restart(0x0002);
    }
  }
  
  /* Clear any other interrupt, and set interrupt enable. */
  outw(0x0000,ioaddr+LANCE_ADDR);
  outw(0x7970,ioaddr+LANCE_DATA);
  
  return 0;
}


uint32_t
lance_init_ring() 
{
  int i;
  
  lp->lock = 0, lp->tx_full = 0;
  lp->cur_rx = lp->cur_tx = 0;
  lp->dirty_rx = lp->dirty_tx = 0;
   
  for (i = 0; i < RX_RING_SIZE; i++) {
    //lp->rx_ring[i].base = virt_to_bus(&lp->rx_ring[i]);
    lp->rx_ring[i].base = lp->rx_buffs + i*PKT_BUF_SZ;
    lp->rx_ring[i].buf_length = -PKT_BUF_SZ;
    lp->rx_ring[i].status = 0x8000;     
  }
  
  /* The Tx buffer address is filled in as needed, but we do need 
   * to clear the upper ownership bit. */
  for (i = 0; i < TX_RING_SIZE; i++) {
    lp->tx_ring[i].base = 0;
    lp->tx_ring[i].status = 0;
  }
  
  lp->init_block.tlen_rlen = TX_RING_LEN_BITS | RX_RING_LEN_BITS;
  //for (i = 0; i < 6; i++)
  // lp->init_block.phys_addr[i] = NETDEV.dev_addr[i];
  lp->init_block.rx_ring = (uint32_t)virt_to_bus(&lp->rx_ring);
  lp->init_block.tx_ring = (uint32_t)virt_to_bus(&lp->tx_ring);
    
  return 0;
}

uint32_t
lance_close() 
{
  int ioaddr = NETDEV.base_address[0];
  int result;
  
  outw(112, ioaddr+LANCE_ADDR);
  lp->stats.rx_missed_errors  = inw(ioaddr + LANCE_DATA);
  
  DEBUGF kprintf(KR_OSTREAM,"Missed Errors = %d",lp->stats.rx_missed_errors);
  
  outw(0, ioaddr+LANCE_ADDR);
  
  kprintf(KR_OSTREAM,
	  "Shutting down ethercard, status was %x",inw(ioaddr+LANCE_DATA));
  
  /* We stop the LANCE here -- it occasionally polls
   * memory if we don't. */
  outw(0x0004, ioaddr+LANCE_DATA);
  
  /* Release IRQ */
  result = capros_DevPrivs_releaseIRQ(KR_DEVPRIVS,NETDEV.irq);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Lance_close::Error Releasing IRQ\n");
    return RC_IRQ_RELEASE_FAILED;
  }
  
  return 0;
}

uint32_t
lance_probe() 
{
  uint32_t result,total;
  unsigned short  vendor = VENDOR_AMD;
  int i;
  
  result = constructor_request(KR_PCI_PROBE_C,KR_BANK,KR_SCHED,
                               KR_VOID,KR_PCI_PROBE_C);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  result = pciprobe_initialize(KR_PCI_PROBE_C);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  DEBUGF kprintf(KR_OSTREAM,"PciProbe init ...  %s.\n",
		 (result == RC_OK) ? "SUCCESS" : "FAILED");
  
  /* Find all the AMD devices */
  result = pciprobe_vendor_total(KR_PCI_PROBE_C, vendor, &total);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  DEBUGF {
    kprintf(KR_OSTREAM,"Searching for AMD Devices ... %s",
	    (result ==RC_OK) ? "SUCCESS" : "FAILED");
    kprintf(KR_OSTREAM,"No. of AMD Devices = %d",total);
  }
  
  /* Get the Device ID to check if it is AMD_LANCE (PCNetPCI - II) 
   * From pci_ids.h the lance card had an ID 0x2000 */
  result = pciprobe_vendor_next(KR_PCI_PROBE_C, vendor,0,&NETDEV);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  DEBUGF{
    kprintf(KR_OSTREAM,"Pci_Find_VendorID ... %s",
	    (result == RC_OK) ? "SUCCESS" : "FAILED");
    kprintf(KR_OSTREAM,"RCV: 00:%02x [%04x/%04x] BR[0] %08x IRQ %d,MASTER=%s", 
	    NETDEV.devfn, NETDEV.vendor,
	    NETDEV.device,NETDEV.base_address[0],NETDEV.irq,
	    NETDEV.master?"YES":"NO");
  }
  
  /* Anshumal - The base ioaddr starts from base_address[0]-1. why?? */
  NETDEV.base_address[0] --;
  
  /* FIX: ask Shap about this -- Merge Bug??? at 0x5000000u */
  for(i=0x1000000u;i>0;i+=DMA_SIZE) {
    /* Hopefully we can do DMA onto this RAM area */
    result = capros_DevPrivs_publishMem(KR_DEVPRIVS,i,i+DMA_SIZE, 0);
    if(result==RC_OK) {
      DEBUGF kprintf(KR_OSTREAM,"Published mem at(%x)",i);
      PHYSADDR = i;
      break;
    }
  }
  
  /* Now we have to reflect this in our address space. As we need to
   * access this. So ask memmap to create our addressable tree for us.*/    
  result = constructor_request(KR_MEMMAP_C,KR_BANK,KR_SCHED,KR_VOID,
			       KR_MEMMAP_C);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Constructing Memmap --- FAILED(%d)",result);
    return result;
  }
  
  result = memmap_map(KR_MEMMAP_C,KR_PHYSRANGE,PHYSADDR,DMA_SIZE,&dma_lss);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Memmaping --- FAILED(%d)",result);
    return result;
  }
  dma_abszero = (uint32_t *)DMA_START; 
  
  /* Patch up our addresspace to reflect the new mapping and 
   * touch all pages to avoid page faulting later */
  patch_addrspace(dma_lss);
  init_mapped_memory(dma_abszero,DMA_SIZE);
  
  if((result=StartHelper(NETDEV.irq))!= RC_OK) {
    kprintf(KR_OSTREAM,"lance_probe():Starting Helper ... [FAILED]");
    return result;
  }
  
  /* Initialize the lance card */
  result = lance_init();
  if(result!=RC_OK)  {
    kprintf(KR_OSTREAM,"LANCE initialisation failed\n");
    return RC_NETIF_INIT_FAILED;
  }
  else  kprintf(KR_OSTREAM,"Initializing Lance ... [SUCCESS]");
  
  /* Open the lance card for use */
  result = lance_open();
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys: Lance Open (%d)... [ FAILED ]",result);
    return result;
  }

  /* Allocate irq for interrupts */
  result = lance_alloc_irq();
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"NetSys: Lance alloc irq (%d)... [ FAILED ]",result);
    return result;
  }
  return RC_OK;
}

uint32_t 
lance_alloc_irq() 
{
  uint32_t result;
  
  /* Allocate the IRQ in the pci device structure */
  result = capros_DevPrivs_allocIRQ(KR_DEVPRIVS,NETDEV.irq);
  if(result != RC_OK)  {
    kprintf(KR_OSTREAM,"IRQ %d not allocated",NETDEV.irq);
    return RC_IRQ_ALLOC_FAILED;
  }
  else  kprintf(KR_OSTREAM,"Allocating IRQ %d ... [SUCCESS]",NETDEV.irq);
  
  return RC_OK;
}
