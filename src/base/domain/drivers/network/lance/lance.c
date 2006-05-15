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

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>
#include <idl/eros/Sleep.h>
#include <idl/eros/Number.h>

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

#define DEBUG 1

#define KR_OSTREAM      KR_APP(0)
#define KR_PCI_PROBE_C  KR_APP(1)
#define KR_MEMMAP_C     KR_APP(2)
#define KR_PCI_PROBE    KR_APP(3)
#define KR_DEVPRIVS     KR_APP(4)
#define KR_PHYSRANGE    KR_APP(5)
#define KR_ADDRSPC      KR_APP(6)
#define KR_DMA          KR_APP(7)
#define KR_SLEEP        KR_APP(8)
#define KR_START        KR_APP(9)
#define KR_HELPER_C     KR_APP(10)
#define KR_HELPER_S     KR_APP(11)

#define KR_SCRATCH      KR_APP(15)

/* From VMWare Specs - The virtualized ethernet card is an AMD PCnet II
 * based on 79C970A chip. Id = 0x2621. */
#define VENDOR_AMD      0x1022
#define AMD79C970A      0x2621

#define ETH_ADDR_LEN     6
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
  // The saved address of a sent-in-place packet/buffer, for skfree()
  //struct sk_buff *tx_skbuff[TX_RING_SIZE];
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
static uint8_t eaddr[ETH_ADDR_LEN];   /* Store MAC Address */
static char rcv_buffer[ETH_FRAME_LEN];     /* Max Ethernet Packet */

/* Function Prototypes */
int  lance_read(int off,int size);
int  lance_write(int off,int size,int val);
int  lance_readcsr(int reg, int size);
int  lance_readbcr(int reg, int size);
void lance_writecsr(int reg,int size, int val);
void lance_writebcr(int reg,int size, int val);

int lance_interrupt();
int lance_init();
int lance_open();
int lance_close();
int lance_init_ring();
int lance_start_xmit(uint8_t*,int,int,char *);
int lance_rx();
int lance_probe();
void lance_restart(unsigned int);

int ProcessRequest(Message *);
void dbgCSR();
static void patch_addrspace(void);
static void init_mapped_memory(uint32_t *, uint32_t);
int StartHelper();

/* Following is used to compute 32 ^ lss for patching together 
 * address space */
#define LWK_FACTOR(lss) (mult(EROS_NODE_SIZE, lss) * EROS_PAGE_SIZE)
uint32_t 
mult(uint32_t base, uint32_t exponent)
{
  uint32_t u;
  int32_t result = 1u;

  if (exponent == 0)  return result;

  for (u = 0; u < exponent; u++)
    result = result * base;
  
  return result;
}

/* Convenience routine for buying a new node for use in expanding the
 * address space. */
static uint32_t
make_new_addrspace(uint16_t lss, fixreg_t key)
{
  uint32_t result = spcbank_buy_nodes(KR_BANK, 1, key, KR_VOID, KR_VOID);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM,"Error: make_new_addrspace: buying node "
	    "returned error code: %u.\n", result);
    return result;
  }

  result = node_make_node_key(key, lss, 0, key);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Error: make_new_addrspace: making node key "
	    "returned error code: %u.\n", result);
    return result;
  }
  return RC_OK;
}

/* Place the newly constructed "mapped memory" tree into the process's
 * address space. */
static void
patch_addrspace(void)
{
  eros_Number_value window_key;
  uint32_t next_slot = 0;
  
  /* Stash the current ProcAddrSpace capability */
  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  
  /* Make a node with max lss */
  make_new_addrspace(EROS_ADDRESS_LSS, KR_ADDRSPC);
  
  /* Patch up KR_ADDRSPC as follows:
     slot 0 = capability for original ProcAddrSpace
     slots 1-15 = local window keys for ProcAddrSpace
     slot 16 = capability for FIFO
     slot 16 - ?? = local window keys for FIFO, as needed
     remaining slot(s) = capability for FRAMEBUF and any needed window keys
  */
  node_swap(KR_ADDRSPC, 0, KR_SCRATCH, KR_VOID);

  for (next_slot = 1; next_slot < 16; next_slot++) {
    window_key.value[2] = 0;    /* slot 0 of local node */
    window_key.value[1] = 0;    /* high order 32 bits of address
                                   offset */

    /* low order 32 bits: multiple of EROS_NODE_SIZE ^ (LSS-1) pages */
    window_key.value[0] = next_slot * LWK_FACTOR(EROS_ADDRESS_LSS-1); 

    /* insert the window key at the appropriate slot */
    node_write_number(KR_ADDRSPC, next_slot, &window_key); 
  }

  next_slot = 16;
  
  node_swap(KR_ADDRSPC, next_slot, KR_DMA, KR_VOID);
  if (dma_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: lance(): no room for local window "
             "keys for DMA!");
  next_slot++;

  /* Finally, patch up the ProcAddrSpace register */
  process_swap(KR_SELF, ProcAddrSpace, KR_ADDRSPC, KR_VOID);
}

/* Generate address faults in the entire mapped region in order to
 * ensure entire address subspace is fabricated and populated with
 *  correct page keys. */
static void
init_mapped_memory(uint32_t *base, uint32_t size)
{
  uint32_t u;

  kprintf(KR_OSTREAM,"lance: initing mapped memory at 0x%08x",(uint32_t)base);

  for (u=0; u < (size / (sizeof(uint32_t))); u=u+EROS_PAGE_SIZE)
    base[u] &= 0xffffffffu;

  kprintf(KR_OSTREAM, "lance: init mapped memory complete.");
}


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
int 
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
  
  /* Get the MAC Address */
  for(i = 0; i < 6; i++)  eaddr[i] = inb(ioaddr + i );
  kprintf(KR_OSTREAM, "MAC Addr %02x:%02x:%02x:%02x:%02x:%02x\n",
  	  eaddr[0],eaddr[1],eaddr[2],eaddr[3],eaddr[4],eaddr[5]);
  
    
  /* Set up CSR0 & CSR1. For this we need to initialize our data structures */
  /* Make certain the data structures used by the LANCE are 16byte aligned */
  lp = (void *)DMA_START; /* 2 GB into the address space */
  
  //lp->rx_buffs = 0x80010000u;
  lp->rx_buffs = PHYSADDR + 0x10000u;
  lp->init_block.mode = 0x0003; /* Disable Rx & Tx for now */
  lp->init_block.tlen_rlen = TX_RING_LEN_BITS | RX_RING_LEN_BITS;
  lp->init_block.filter[0] = 0x00000000;
  lp->init_block.filter[1] = 0x00000000;
  lp->init_block.rx_ring = virt_to_bus(&lp->rx_ring);
  lp->init_block.tx_ring = virt_to_bus(&lp->tx_ring);
  
  kprintf(KR_OSTREAM,"sizeof IB = %d",sizeof(lp->init_block));
  kprintf(KR_OSTREAM,"rx_ring = %x",lp->init_block.rx_ring);
  kprintf(KR_OSTREAM,"tx_ring = %x",lp->init_block.tx_ring);
  
  /* The MAC  address of the card */
  for(i=0;i<6;i++) {
    lp->init_block.phys_addr[i] = eaddr[i];
  }
   
  /* Switch lance to 32-bit mode */
  lance_writebcr(20,2,0x200);
    
  RAM_Addr = PHYSADDR;  
  lance_writecsr(IblockAddr,2,RAM_Addr&0xffffu);
  lance_writecsr(IblockAddr+1,2,RAM_Addr>>16);
  
  lance_writebcr(20,2,lance_readbcr(20,2)|BIT(1));
  
  lance_writecsr(Imask,2,IDON);
  lance_writecsr(CSR0,2,INIT);
  
  while(!(lance_readcsr(CSR0,2)&IDON)) {
    eros_Sleep_sleep(KR_SLEEP,10);
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

int 
lance_open() 
{
  int ioaddr = NETDEV.base_address[0];
  int i;
  uint32_t RAM_Addr;
  
  /* Reset the LANCE */
  inw(ioaddr+LANCE_RESET);
  
  /* Allow some time to wake up */
  eros_Sleep_sleep(KR_SLEEP,200);

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
  kprintf(KR_OSTREAM,"LANCE open after %d ticks, csr0 %x",i,
	  inw(ioaddr+LANCE_DATA));

  outw(0x0,ioaddr+LANCE_ADDR);
  outw(0x42,ioaddr+LANCE_DATA);
  
  return 0;
}


int
lance_rx() 
{
  int entry = lp->cur_rx & RX_RING_MOD_MASK;
  static long pkts;

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

#if 0
      int i;
      char *s = (char *)bus_to_virt(lp->rx_ring[entry].base);
      for(i=0;i<pkt_len;i++) kprintf(KR_OSTREAM,"p[%d]=%d,%c",i,s[i],s[i]);
#endif
      pkts++;
      kprintf(KR_OSTREAM,"pkt_len=%d,pkts=%x",pkt_len,pkts);
      if(pkt_len<60) {
	kprintf(KR_OSTREAM,"Error::pkt_len < 60");
	lp->stats.rx_errors++;
      }else {
	lp->stats.rx_packets++;
      }
    }
    
    lp->rx_ring[entry].buf_length = -PKT_BUF_SZ;
    lp->rx_ring[entry].status |= 0x8000;
    entry = (++lp->cur_rx) & RX_RING_MOD_MASK;
  }
  
  return 0;
}

/* Handles the ethernet pkt format into this */
int 
lance_start_xmit(uint8_t *dest,int type,int size,char *packet) 
{
  int ioaddr = NETDEV.base_address[0];
  int entry;
  int i;
    
#ifdef DEBUG
  outw(0x0000, ioaddr+LANCE_ADDR);
  kprintf(KR_OSTREAM,"lance_start_xmit()-csr0 %x",inw(ioaddr+LANCE_DATA));
#endif  
  
  /* Fill in a Tx ring entry */

  /* Mask to ring buffer boundary*/
  entry = lp->cur_tx & TX_RING_MOD_MASK;
      
  lp->tx_ring[entry].misc = 0x00000000;
  lp->tx_ring[entry].length = -size;//sizeof(xmitbuf);
  
  /* memcpy */
  for(i=0;i<ETH_ADDR_LEN;i++) {
    lp->tbuf[i] = dest[i];
    lp->tbuf[i+ETH_ADDR_LEN] = eaddr[i];
  }
  
  lp->tbuf[ETH_ADDR_LEN*2] = type >> 8;
  lp->tbuf[ETH_ADDR_LEN*2+1] = type;
				
  for(i=0;i<size;i++) lp->tbuf[i+ETH_ADDR_LEN*2+2] = packet[i];
    
  lp->tx_ring[entry].base = virt_to_bus(&lp->tbuf);;
  lp->tx_ring[entry].status = 0x8300;
    
  /* Caution: the write order is important here, set the base address
   * with the "ownership" bits last. */
  //lp->tx_ring[entry].length = -skb->len;
  //lp->tx_skbuff[entry] = skb;
  //lp->tx_ring[entry].base = (u32)virt_to_bus(skb->data);
  
  lp->cur_tx++;

  /* Trigger an immediate send poll */
  outw(0x0000, ioaddr+LANCE_ADDR);
  outw(0x0048, ioaddr+LANCE_DATA);
  
  return 0;
}


/* Interrupt handler */
int 
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
#ifdef DEBUG
      kprintf(KR_OSTREAM,"******Rx Interrupt*******");
#endif
      lance_rx();
    }
    
    if(csr0 & 0x0200) {       /* Tx interrupt */
#ifdef DEBUG
      kprintf(KR_OSTREAM,"******Tx Interrupt*******");
#endif
      while(lp->dirty_tx < lp->cur_tx) {
	int entry = lp->dirty_tx & TX_RING_MOD_MASK;
	int status = lp->tx_ring[entry].status;
	
	if(status < 0) 	  break; /* It still hasn't been Txed*/
		
	lp->tx_ring[entry].base = 0;
	
	if(status & 0x4000) {
	  int err_status = lp->tx_ring[entry].misc;
#ifdef DEBUG
	  kprintf(KR_OSTREAM,"Investigate Tx Error");
#endif
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
#ifdef DEBUG
	kprintf(KR_OSTREAM,"out-of-sync dirty pointer, %d vs. %d, full=%d",
		lp->dirty_tx, lp->cur_tx, lp->tx_full);
#endif
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


int
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

int 
lance_close() 
{
  int ioaddr = NETDEV.base_address[0];
  int result;
  
  outw(112, ioaddr+LANCE_ADDR);
  lp->stats.rx_missed_errors  = inw(ioaddr + LANCE_DATA);
  
#ifdef DEBUG
  kprintf(KR_OSTREAM,"Missed Errors = %d",lp->stats.rx_missed_errors);
#endif
  
  outw(0, ioaddr+LANCE_ADDR);
  
  kprintf(KR_OSTREAM,
	  "Shutting down ethercard, status was %x",inw(ioaddr+LANCE_DATA));
  
  /* We stop the LANCE here -- it occasionally polls
   * memory if we don't. */
  outw(0x0004, ioaddr+LANCE_DATA);
  
  /* Release IRQ */
  result = eros_DevPrivs_releaseIRQ(KR_DEVPRIVS,NETDEV.irq);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Lance_close::Error Releasing IRQ\n");
    return RC_IRQ_RELEASE_FAILED;
  }
  
  return 0;
}

int 
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
  
#ifdef DEBUG
  kprintf(KR_OSTREAM,"PciProbe init ...  %s.\n",
	  (result == RC_OK) ? "SUCCESS" : "FAILED");
#endif
  
  /* Find all the AMD devices */
  result = pciprobe_vendor_total(KR_PCI_PROBE_C, vendor, &total);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
#ifdef DEBUG
  kprintf(KR_OSTREAM,"Searching for AMD Devices ... %s",
	  (result ==RC_OK) ? "SUCCESS" : "FAILED");
  kprintf(KR_OSTREAM,"No. of AMD Devices = %d",total);
#endif
  
  /* Get the Device ID to check if it is AMD_LANCE (PCNetPCI - II) 
   * From pci_ids.h the lance card had an ID 0x2000 */
  result = pciprobe_vendor_next(KR_PCI_PROBE_C, vendor,0,&NETDEV);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
#ifdef DEBUG
  kprintf(KR_OSTREAM,"Pci_Find_VendorID ... %s",
	  (result == RC_OK) ? "SUCCESS" : "FAILED");
  kprintf(KR_OSTREAM,"RCV: 00:%02x [%04x/%04x] BR[0] %08x IRQ %d,MASTER=%s\n", 
	  NETDEV.devfn, NETDEV.vendor,
	  NETDEV.device,NETDEV.base_address[0],NETDEV.irq,
	  NETDEV.master?"YES":"NO");
#endif

  /* Anshumal - The base ioaddr starts from base_address[0]-1. why?? */
  NETDEV.base_address[0] --;
  
  /* FIX: ask Shap about this -- Merge Bug??? at 0x5000000u */
  for(i=0x1000000u;i>0;i+=DMA_SIZE) {
    /* Hopefully we can do DMA onto this RAM area */
    result = eros_DevPrivs_publishMem(KR_DEVPRIVS,i,i+DMA_SIZE, 0);
    if(result==RC_OK) {
      kprintf(KR_OSTREAM,"Published mem at(%x)",i);
      PHYSADDR = i;
      break;
    }
  }
  
  /* Now we have to reflect this in our address space. As we need to
   * access this. So ask memmap to create our addressable tree for us.*/    
  result = constructor_request(KR_MEMMAP_C,KR_BANK,KR_SCHED,KR_VOID,
			       KR_DMA);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Constructing Memmap --- FAILED(%d)",result);
    return result;
  }
  
  result = memmap_map(KR_DMA,KR_PHYSRANGE,PHYSADDR,DMA_SIZE,&dma_lss);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Memmaping --- FAILED(%d)",result);
    return result;
  }
  dma_abszero = (uint32_t *)DMA_START; 
  
  /* Patch up our addresspace to reflect the new mapping and 
   * touch all pages to avoid page faulting later */
  patch_addrspace();
  init_mapped_memory(dma_abszero,DMA_SIZE);
  
  if((result=StartHelper())!= RC_OK) {
    kprintf(KR_OSTREAM,"lance_probe():Starting Helper ... [FAILED]");
    return result;
  }
  
  /* Initialize the lance card */
  result = lance_init();
  if(result!=RC_OK)  {
    kprintf(KR_OSTREAM,"LANCE initialisation failed\n");
    return RC_NETIF_INIT_FAILED;
  }
  else 
    kprintf(KR_OSTREAM,"Initializing Lance ... [SUCCESS]");
  return RC_OK;
}


int 
main(void)
{
  Message msg;
    
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,   KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_PCI_PROBE_C,  KR_PCI_PROBE_C);
  node_extended_copy(KR_CONSTIT, KC_DEVPRIVS,  KR_DEVPRIVS);
  node_extended_copy(KR_CONSTIT, KC_PHYSRANGE,  KR_PHYSRANGE);
  node_extended_copy(KR_CONSTIT, KC_MEMMAP_C, KR_MEMMAP_C);
  node_extended_copy(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  node_extended_copy(KR_CONSTIT, KC_HELPER_C, KR_HELPER_C);
    
  /* Move the DEVPRIVS key to the ProcIoSpace slot so we can do io calls */
  process_swap(KR_SELF, ProcIoSpace, KR_DEVPRIVS, KR_VOID);
  process_make_start_key(KR_SELF, 0, KR_START);
  
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = lance_probe();
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0   = KR_VOID;
  msg.rcv_key1   = KR_VOID;
  msg.rcv_key2   = KR_VOID;
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = rcv_buffer;
  msg.rcv_limit  = sizeof(rcv_buffer);
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  do {
    msg.snd_invKey = KR_RETURN;
    msg.snd_rsmkey = KR_RETURN;
    RETURN(&msg);
  } while (ProcessRequest(&msg));

  return 0;
}

/* Start the helper thread & then pass it our start key so that
 * the helper can notify us of interrupts */
int 
StartHelper() {
  uint32_t result;
  Message msg;
  
  result = process_copy(KR_SELF,ProcSched, KR_SCHED);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"lance(start helper)::Process copy ...[FAILED]");
    return RC_HELPER_START_FAILED;
  }
  
  result = constructor_request(KR_HELPER_C, KR_BANK, KR_SCHED, KR_VOID,
                                 KR_HELPER_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "lance:Constructing helper ... [FAILED]");
    return RC_HELPER_START_FAILED;
  }else {
    kprintf(KR_OSTREAM, "lance:Constructing helper ... [SUCCESS]");
  }
  
  process_make_start_key(KR_SELF, 0, KR_START);
  
  /* Pass our start key to the helper process */
  msg.snd_invKey = KR_HELPER_S;
  msg.snd_key0   = KR_VOID;
  msg.snd_key1   = KR_START;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_VOID;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = OC_netdriver_key;
  msg.snd_w1 = NETDEV.irq;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_key0 = KR_VOID;
  msg.rcv_key1 = KR_VOID;
  msg.rcv_key2 = KR_VOID;
  msg.rcv_rsmkey = KR_VOID;
  msg.rcv_data = 0;
  msg.rcv_limit = 0;
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;
  
  CALL(&msg);
  kprintf(KR_OSTREAM, "lance:sendingkey+IRQ ... [SUCCESS]");
    
  return RC_OK;
}

/* Process Request logic */
int 
ProcessRequest(Message *msg) 
{
  msg->snd_len  = 0;
  msg->snd_key0 = KR_VOID;
  msg->snd_key1 = KR_VOID;
  msg->snd_key2 = KR_VOID;
  msg->snd_rsmkey = KR_RETURN;
  msg->snd_code   = RC_OK;
  msg->snd_w1 = 0;
  msg->snd_w2 = 0;
  msg->snd_w3 = 0;
  msg->snd_invKey = KR_RETURN;

  switch (msg->rcv_code) {
    /* open the lance card for use */
  case OC_netif_open:
    {
      int result;
      
      result = lance_open();
      if(result!=RC_OK) { 
	msg->snd_code = RC_NETIF_OPEN_FAILED;
	return 1;
      }
      
      /* Allocate the IRQ in the pci device structure */
      result = eros_DevPrivs_allocIRQ(KR_DEVPRIVS,NETDEV.irq, 0);
      if(result != RC_OK) {
	msg->snd_code = RC_IRQ_ALLOC_FAILED;
	kprintf(KR_OSTREAM,"IRQ %d not allocated",NETDEV.irq);
      }
      else   
	kprintf(KR_OSTREAM,"Allocating IRQ %d ... [SUCCESS]",NETDEV.irq);
      
      msg->snd_len = 6; /* Ethernet Address is 6-byte long */
      msg->snd_data = &eaddr[0];
      return 1;
    }
    
    /* Acknowledge interrupt */
  case OC_irq_arrived:
    {
      lance_interrupt(); /* Service interrupt */
      return 1;
    }
    
    /* Transmit packet */
  case OC_netif_xmit:
    {
      uint8_t dest[ETH_ADDR_LEN];
      int type;
      
      type = msg->rcv_w1;
      
      dest[3] = msg->rcv_w2 & 0xff ;       dest[0] =  msg->rcv_w3 & 0xff;
      dest[4] = (msg->rcv_w2>>8) & 0xff;   dest[1] = (msg->rcv_w3>>8) & 0xff;
      dest[5] = (msg->rcv_w2>>16) & 0xff;  dest[2] = (msg->rcv_w3>>16) & 0xff;
      
      msg->snd_code = lance_start_xmit(dest,type,msg->rcv_sent,rcv_buffer);
      return 1;
    }
    
    /* Change the settings of the lance chip */
  case OC_netif_mode:
    {
      lp->init_block.mode = msg->rcv_w1; /* Set the mode in the initblock */
      
      /* Stop the lance & restart */
      lance_writecsr(CSR0,2,STOP);
      lance_restart(0x0042);
      return 1;
    }
    
    /* Close & exit */
  case OC_netif_close:
    {
      msg->snd_code = lance_close();
      return 1;
    }
    
  default:
    break;
  }
  
  msg->snd_code = RC_eros_key_UnknownRequest;
  return 1;
}
