/* EtherLinkXL.c: A 3Com EtherLink PCI III/XL ethernet driver for linux. */
/*
	Written 1996-1999 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	This driver is for the 3Com "Vortex" and "Boomerang" series ethercards.
	Members of the series include Fast EtherLink 3c590/3c592/3c595/3c597
	and the EtherLink XL 3c900 and 3c905 cards.

	Problem reports and questions should be directed to
	vortex@scyld.com

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Linux Kernel Additions:
	
 	0.99H+lk0.9 - David S. Miller - softnet, PCI DMA updates
 	0.99H+lk1.0 - Jeff Garzik <jgarzik@pobox.com>
		Remove compatibility defines for kernel versions < 2.2.x.
		Update for new 2.3.x module interface

    - See http://www.uow.edu.au/~andrewm/linux/#3c59x-2.3 for more details.
    - Also see Documentation/networking/vortex.txt
*/
/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/endian.h>

#include <idl/capros/key.h>
#include <idl/capros/DevPrivs.h>
#include <idl/capros/Sleep.h>
#include <idl/capros/SpaceBank.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/arch/i386/Process.h>
#include <idl/capros/GPT.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/drivers/PciProbeKey.h>
#include <domain/MemmapKey.h>
#include <domain/drivers/NetKey.h>

#include <string.h>

#include "constituents.h"

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

#define DEBUG_3C905C    if(0)

#define VENDOR_3COM        0x10b7
#define DEVICE_3C905C      0x9200

#define DMA_START       0x80000000u /* 2 GB into address space */
#define BUF_START       0x80001000u
#define DMA_SIZE        0x20000u    /* DMA Size to be dev publish*/

#define ETH_ADDR_LEN     6
#define ETH_FRAME_LEN  1518

#define virt_to_bus(x) (PHYSADDR + (void*)x - (void*)DMA_START)
#define bus_to_virt(x) (DMA_START + (void*)x - (void*)PHYSADDR)

#define le32_to_cpu(x)  x//htons(x)
#define cpu_to_le32(x)  x//ntohs(x)

/* A few values that may be tweaked. */
/* Keep the ring sizes a power of two for efficiency. */
#define TX_RING_SIZE    16
#define RX_RING_SIZE    32
#define PKT_BUF_SZ      1536     /* Size of each temporary Rx buffer.*/

#define IFF_PROMISC     0x100    /* receive all packets */
#define IFF_ALLMULTI    0x200    /* receive all multicast packets */

#define ETH_ALEN        6
#define ETH_HLEN        14

/* Allow setting MTU to a larger size, bypassing the normal ethernet setup. */
static const int mtu = 1500;

typedef struct pci_dev_data DEVICE; /* shorthand!*/

enum {  
  IS_VORTEX=1, IS_BOOMERANG=2, IS_CYCLONE=4, IS_TORNADO=8,
  EEPROM_8BIT=0x10,  /*AKPM: Uses 0x230 as the base bitmaps for EEPROM reads*/
  HAS_PWR_CTRL=0x20, HAS_MII=0x40, HAS_NWAY=0x80, HAS_CB_FNS=0x100,
  INVERT_MII_PWR=0x200, INVERT_LED_PWR=0x400, MAX_COLLISION_RESET=0x800,
  EEPROM_OFFSET=0x1000, HAS_HWCKSM=0x2000, WNO_XCVR_PWR=0x4000 
};


/* Operational definitions.
 * These are not used by other compilation units and thus are not
 * exported in a ".h" file.
 * First the windows.  There are eight register windows, with the command
 * and status registers available in each.
 */
#define EL3WINDOW(win_num) outw(SelectWindow + \
                           (win_num),NETDEV.base_address[0] + EL3_CMD)
#define EL3_CMD 0x0e
#define EL3_STATUS 0x0e

/* The top five bits written to EL3_CMD are a command, the lower
 * 11 bits are the parameter, if applicable.
 * Note that 11 parameters bits was fine for ethernet, but the new chip
 * can handle FDDI length frames (~4500 octets) and now parameters count
 * 32-bit 'Dwords' rather than octets. */
enum vortex_cmd  {
  TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
  RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11,
  UpStall = 6<<11, UpUnstall = (6<<11)+1,
  DownStall = (6<<11)+2, DownUnstall = (6<<11)+3,
  RxDiscard = 8<<11, TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
  FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrEnb = 14<<11,
  SetStatusEnb = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
  SetTxThreshold = 18<<11, SetTxStart = 19<<11,
  StartDMAUp = 20<<11, StartDMADown = (20<<11)+1, StatsEnable = 21<<11,
  StatsDisable = 22<<11, StopCoax = 23<<11, SetFilterBit = 25<<11,
};

/* The SetRxFilter command accepts the following classes: */
enum RxFilter {
  RxStation = 1, RxMulticast = 2, RxBroadcast = 4, RxProm = 8 
};

/* Bits in the general status register. */
enum vortex_status {
  IntLatch = 0x0001, HostError = 0x0002, TxComplete = 0x0004,
  TxAvailable = 0x0008, RxComplete = 0x0010, RxEarly = 0x0020,
  IntReq = 0x0040, StatsFull = 0x0080,
  DMADone = 1<<8, DownComplete = 1<<9, UpComplete = 1<<10,
  DMAInProgress = 1<<11,                  /* DMA controller is still busy.*/
  CmdInProgress = 1<<12,                  /* EL3_CMD is still busy.*/
};

/* Register window 1 offsets, the window used in normal operation.
 * On the Vortex this window is always mapped at offsets 0x10-0x1f. */
enum Window1 {
  TX_FIFO = 0x10,  RX_FIFO = 0x10,  RxErrors = 0x14,
  RxStatus = 0x18,  Timer=0x1A, TxStatus = 0x1B,
  TxFree = 0x1C, /* Remaining free bytes in Tx buffer. */
};

enum Window0 {
  Wn0EepromCmd = 10,              /* Window 0: EEPROM command register. */
  Wn0EepromData = 12,             /* Window 0: EEPROM results register. */
  IntrStatus=0x0E,                /* Valid in all windows. */
};

enum Win0_EEPROM_bits {
  EEPROM_Read = 0x80, EEPROM_WRITE = 0x40, EEPROM_ERASE = 0xC0,
  EEPROM_EWENB = 0x30,            /* Enable erasing/writing for 10 msec. */
  EEPROM_EWDIS = 0x00,            /* Disable EWENB before 10 msec timeout. */
};

/* EEPROM locations. */
enum eeprom_offset {
  PhysAddr01=0, PhysAddr23=1, PhysAddr45=2, ModelID=3,
  EtherLink3ID=7, IFXcvrIO=8, IRQLine=9,
  NodeAddr01=10, NodeAddr23=11, NodeAddr45=12,
  DriverTune=13, Checksum=15
};

enum Window2 {                  /* Window 2. */
  Wn2_ResetOptions=12,
};

enum Window3 {                  /* Window 3: MAC/config bits. */
  Wn3_Config=0, Wn3_MAC_Ctrl=6, Wn3_Options=8,
};

#define BFEXT(value, offset, bitcount)  \
    ((((unsigned long)(value)) >> (offset)) & ((1 << (bitcount)) - 1))

#define BFINS(lhs, rhs, offset, bitcount) \
        (((lhs) & ~((((1 << (bitcount)) - 1)) << (offset))) |   \
        (((rhs) & ((1 << (bitcount)) - 1)) << (offset)))

#define RAM_SIZE(v)             BFEXT(v, 0, 3)
#define RAM_WIDTH(v)    BFEXT(v, 3, 1)
#define RAM_SPEED(v)    BFEXT(v, 4, 2)
#define ROM_SIZE(v)             BFEXT(v, 6, 2)
#define RAM_SPLIT(v)    BFEXT(v, 16, 2)
#define XCVR(v)                 BFEXT(v, 20, 4)
#define AUTOSELECT(v)   BFEXT(v, 24, 1)

enum Window4 {          /* Window 4: Xcvr/media bits. */
  Wn4_FIFODiag = 4, Wn4_NetDiag = 6, Wn4_PhysicalMgmt=8, Wn4_Media = 10,
};

enum Win4_Media_bits {
  Media_SQE = 0x0008,     /* Enable SQE error counting for AUI. */
  Media_10TP = 0x00C0,    /* Enable link beat and jabber for 10baseT. */
  Media_Lnk = 0x0080,     /* Enable just link beat for 100TX/100FX. */
  Media_LnkBeat = 0x0800, 
};

enum Window7 {            /* Window 7: Bus Master control. */
  Wn7_MasterAddr = 0, Wn7_MasterLen = 6, Wn7_MasterStatus = 12,
};

/* Boomerang bus master control registers. */
enum MasterCtrl {
  PktStatus = 0x20, DownListPtr = 0x24, FragAddr = 0x28, FragLen = 0x2c,
  TxFreeThreshold = 0x2f, UpPktStatus = 0x30, UpListPtr = 0x38,
};

/* The Rx and Tx descriptor lists.
 * Caution Alpha hackers: these types are 32 bits!  Note also the 8 byte
 * alignment contraint on tx_ring[] and rx_ring[]. */
#define LAST_FRAG       0x80000000   /* Last Addr/Len pair in descriptor. */
#define DN_COMPLETE     0x00010000   /* This packet has been downloaded */
struct boom_rx_desc {
  uint32_t next;                     /* Last entry points to 0. */
  int32_t status;
  uint32_t addr;                     /* Up to 63 addr/len pairs possible. */
  int32_t length;                    /* Set LAST_FRAG to indicate last pair */
};


/* Values for the Rx status entry. */
enum rx_desc_status {
  RxDComplete=0x00008000, RxDError=0x4000,
  /* See boomerang_rx() for actual error bits */
  IPChksumErr=1<<25, TCPChksumErr=1<<26, UDPChksumErr=1<<27,
  IPChksumValid=1<<29, TCPChksumValid=1<<30, UDPChksumValid=1<<31,
};

struct boom_tx_desc {
  uint32_t next;                    /* Last entry points to 0 */
  int32_t status;                   /* bits 0:12 length, others see below */
  uint32_t addr;
  int32_t length;
};

/* Values for the Tx status entry. */
enum tx_desc_status {
  CRCDisable=0x2000, TxDComplete=0x8000,
  AddIPChksum=0x02000000, AddTCPChksum=0x04000000, AddUDPChksum=0x08000000,
  TxIntrUploaded=0x80000000,   /* IRQ when in FIFO, but maybe not sent. */
};


/* Chip features we care about in vp->capabilities, read from the EEPROM. */
enum ChipCaps { CapBusMaster=0x20, CapPwrMgmt=0x2000 };

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

struct vortex_private {
  /* The Rx and Tx rings should be quad-word-aligned. */
  struct boom_rx_desc rx_ring[RX_RING_SIZE];
  struct boom_tx_desc tx_ring[RX_RING_SIZE];
  uint32_t rx_ring_dma;
  uint32_t tx_ring_dma;
  /* The addresses of transmit- and receive-in-place skbuffs. */
  struct sk_buff* rx_skbuff[RX_RING_SIZE];
  struct sk_buff* tx_skbuff[TX_RING_SIZE];
  unsigned char tbuf[ETH_FRAME_LEN + 40];
  unsigned int cur_rx, cur_tx;      /* The next free ring entry */
  unsigned int dirty_rx, dirty_tx;  /* The ring entries to be free()ed. */
  struct enet_statistics stats;     /* Statistics of service */
  struct sk_buff *tx_skb;           /* Packet being eaten by bus master ctrl */
  uint32_t tx_skb_dma;              /* Allocated DMA address for bus 
				     * master ctrl DMA.   */
  
  /* Some values here only for performance evaluation and path-coverage */
  int rx_nocopy, rx_copy, queued_packet, rx_csumhits;
  int card_idx;
  
  /* The remainder are related to chip state, mostly media selection. */
  //struct timer_list timer;           /* Media selection timer. */
  //struct timer_list rx_oom_timer;    /* Rx skb allocation retry timer */
  int options;                       /* User-settable misc. driver options. */
  unsigned int media_override:4,     /* Passed-in media type. */
    default_media:4,                 /* Read from the EEPROM/Wn3_Config. */
    full_duplex:1, force_fd:1, autoselect:1,
    bus_master:1,                    /* Vortex can only do a fragment bus-m */
    full_bus_master_tx:1, 
    full_bus_master_rx:2,            /* Boomerang  */
    flow_ctrl:1,                     /* Use 802.3x flow control (PAUSE only) */
    partner_flow_ctrl:1,             /* Partner supports flow control */
    has_nway:1,enable_wol:1,         /* Wake-on-LAN is enabled */
    pm_state_valid:1,                /* power_state[] has sane contents */
    open:1,
    medialock:1,
    must_free_region:1;        /* Flag: if zero, Cardbus owns the I/O region */
  int drv_flags;
  uint16_t status_enable;
  uint16_t intr_enable;
  uint16_t available_media;            /* From Wn3_Options. */
  uint16_t capabilities, info1, info2; /* Various, from EEPROM. */
  uint16_t advertising;                /* NWay media advertisement */
  unsigned char phys[2];               /* MII device addresses. */
  uint16_t deferred;                   /* Resend these interrupts when we
					* bale from the ISR */
  uint16_t io_size;                    /* Size of PCI region (for 
					* release_region) */
  uint32_t power_state[16];
};



/* The action to take with a media selection timer tick.
 * Note that we deviate from the 3Com order by checking 10base2 
 * before AUI. */
enum xcvr_types {
  XCVR_10baseT=0, XCVR_AUI, XCVR_10baseTOnly, XCVR_10base2, XCVR_100baseTx,
  XCVR_100baseFx, XCVR_MII=6, XCVR_NWAY=8, XCVR_ExtMII=9, XCVR_Default=10,
};


#define HZ 1

struct media_table {
  char *name;
  unsigned int media_bits:16,  /* Bits to set in Wn4_Media register. */
    mask:8,                    /* The transceiver-present bit in Wn3_Config.*/
	 next:8;                    /* The media type to try next. */
  int wait;                    /* Time before we check media status. */
} media_tbl[] = {
  {   "10baseT", Media_10TP,0x08, XCVR_10base2, (14*HZ)/10},
  { "10Mbs AUI", Media_SQE, 0x20, XCVR_Default, (1*HZ)/10},
  { "undefined", 0,                     0x80, XCVR_10baseT, 10000},
  { "10base2",   0,                     0x10, XCVR_AUI,         (1*HZ)/10},
  { "100baseTX", Media_Lnk, 0x02, XCVR_100baseFx, (14*HZ)/10},
  { "100baseFX", Media_Lnk, 0x04, XCVR_MII,             (14*HZ)/10},
  { "MII",               0,                     0x41, XCVR_10baseT, 3*HZ },
  { "undefined", 0,                     0x01, XCVR_10baseT, 10000},
  { "Autonegotiate", 0,         0x41, XCVR_10baseT, 3*HZ},
  { "MII-External",      0,             0x41, XCVR_10baseT, 3*HZ },
  { "Default",   0,                     0xFF, XCVR_10baseT, 10000},
};

/* Globals */
static DEVICE NETDEV;                        /* We use a single net adapter */
static char rcv_buffer[1600];                /* Max Ethernet Packet */
static struct vortex_private *VORTEX_PRIVATE; /* The singular network card */

/* Set iff a MII transceiver on any interface requires mdio preamble.
 * This only set with the original DP83840 on older 3c905 boards, so the extra
 * code size of a per-interface flag is not worthwhile. */
static char mii_preamble_required;

/* FIX:: This should all go into a device structure 
 * as it is specific for a specific device */
static int DEV_if_port;            /* type of interface port */
static int DEV_flags;              /* flags of operating mode */
static unsigned char DEV_eaddr[6]; /* Station mac address */
static uint8_t DEV_irq;            /* The irq of the device */
static int DEV_mtu;                /* max transmission unit */

/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 4;

uint16_t dma_lss;          /* The lss of our dma region */
uint32_t *dma_abszero;     /* Start address of the dma buffer */
uint32_t PHYSADDR;

/* Function protoypes */
static void update_stats(long ioaddr);

static unsigned int
BlssToL2v(unsigned int blss)
{
  // assert(blss > 0);
  return (blss -1 - EROS_PAGE_BLSS) * EROS_NODE_LGSIZE + EROS_PAGE_ADDR_BITS;
}

/* Convenience routine for buying a new node for use in expanding the
 * address space. */
static uint32_t
make_new_addrspace(uint16_t lss, fixreg_t key)
{
  uint32_t result = capros_SpaceBank_alloc1(KR_BANK, capros_Range_otGPT, key);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM,"Error: make_new_addrspace: buying GPT "
            "returned error code: %u.\n", result);
    return result;
  }
  
  result = capros_GPT_setL2v(key, BlssToL2v(lss));
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "Error: make_new_addrspace: setL2v "
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
  /* Stash the current ProcAddrSpace capability */
  capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH);
  
  /* Make a node with max lss */
  make_new_addrspace(EROS_ADDRESS_LSS, KR_ADDRSPC);
  
  /* Patch up KR_ADDRSPC as follows:
   * slot 0 = capability for original ProcAddrSpace
   * slots 1-15 = local window keys for ProcAddrSpace
   * slot 16 = capability for FIFO
   * slot 16 - ?? = local window keys for FIFO, as needed
   * remaining slot(s) = capability for FRAMEBUF and any needed window keys */
  capros_GPT_setSlot(KR_ADDRSPC, 0, KR_SCRATCH);
  
  uint32_t next_slot = 0;
  for (next_slot = 1; next_slot < 16; next_slot++) {
    /* insert the window key at the appropriate slot */
    capros_GPT_setWindow(KR_ADDRSPC, next_slot, 0, 0,
        ((uint64_t)next_slot) << BlssToL2v(EROS_ADDRESS_LSS-1));
  }
  
  next_slot = 16;
  capros_GPT_setSlot(KR_ADDRSPC, next_slot, KR_DMA);
  if (dma_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR:no room for local window keys for DMA!");
  next_slot++;
  
  /* Finally, patch up the ProcAddrSpace register */
  capros_Process_swapAddrSpace(KR_SELF, KR_ADDRSPC, KR_VOID);
}

/* Generate address faults in the entire mapped region in order to
 * ensure entire address subspace is fabricated and populated with
 *  correct page keys. */
static void
init_mapped_memory(uint32_t *base, uint32_t size)
{
  uint32_t u;
  
  //kprintf(KR_OSTREAM,"initing mapped memory at 0x%08x",(uint32_t)base);
  for (u=0; u < (size / (sizeof(uint32_t))); u=u+EROS_PAGE_SIZE)
    base[u] &= 0xffffffffu;
  
  //kprintf(KR_OSTREAM,"init mapped memory complete.");
}

/* MII transceiver control section.
 * Read and write the MII registers using software-generated serial
 * MDIO protocol.  See the MII specifications or DP83840A data sheet
 * for details. */

/* The maximum data clock rate is 2.5 Mhz.  The minimum timing is usually
 * met by back-to-back PCI I/O cycles, but we insert a delay to avoid
 * "overclocking" issues. */
#define mdio_delay() inl(mdio_addr)

#define MDIO_SHIFT_CLK    0x01
#define MDIO_DIR_WRITE    0x04
#define MDIO_DATA_WRITE0 (0x00 | MDIO_DIR_WRITE)
#define MDIO_DATA_WRITE1 (0x02 | MDIO_DIR_WRITE)
#define MDIO_DATA_READ    0x02
#define MDIO_ENB_IN       0x00

/* Generate the preamble required for initial synchronization and
 * a few older transceivers. */
static void 
mdio_sync(long ioaddr, int bits)
{
  long mdio_addr = ioaddr + Wn4_PhysicalMgmt;

  /* Establish sync by sending at least 32 logic ones. */
  while (-- bits >= 0) {
    outw(MDIO_DATA_WRITE1, mdio_addr);
    mdio_delay();
    outw(MDIO_DATA_WRITE1 | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay();
  }
}

static int 
mdio_read(int phy_id, int location)
{
  int i;
  long ioaddr = NETDEV.base_address[0];
  int read_cmd = (0xf6 << 10) | (phy_id << 5) | location;
  unsigned int retval = 0;
  long mdio_addr = ioaddr + Wn4_PhysicalMgmt;

  if (mii_preamble_required)    mdio_sync(ioaddr, 32);
  
  /* Shift the read command bits out. */
  for (i = 14; i >= 0; i--) {
    int dataval = (read_cmd&(1<<i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
    outw(dataval, mdio_addr);
    mdio_delay();
    outw(dataval | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay();
  }
  /* Read the two transition, 16 data, and wire-idle bits. */
  for (i = 19; i > 0; i--) {
    outw(MDIO_ENB_IN, mdio_addr);
    mdio_delay();
    retval = (retval << 1) | ((inw(mdio_addr) & MDIO_DATA_READ) ? 1 : 0);
    outw(MDIO_ENB_IN | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay();
  }
  return retval & 0x20000 ? 0xffff : retval>>1 & 0xffff;
}

static void 
mdio_write(int phy_id, int location, int value)
{
  long ioaddr = NETDEV.base_address[0];
  int write_cmd = 0x50020000 | (phy_id << 23) | (location << 18) | value;
  long mdio_addr = ioaddr + Wn4_PhysicalMgmt;
  int i;
  
  if (mii_preamble_required)    mdio_sync(ioaddr, 32);
  
  /* Shift the command bits out. */
  for (i = 31; i >= 0; i--) {
    int dataval = (write_cmd&(1<<i)) ? MDIO_DATA_WRITE1 : MDIO_DATA_WRITE0;
    outw(dataval, mdio_addr);
    mdio_delay();
    outw(dataval | MDIO_SHIFT_CLK, mdio_addr);
    mdio_delay();
  }
  /* Leave the interface idle. */
  mdio_sync(ioaddr,32);
  
  return;
}

/* Issue command to the tornado. We may have to wait for the 
 * command to complete*/
static void
issue_and_wait(int cmd)
{
  int i;
  uint32_t ioaddr = NETDEV.base_address[0];
  
  outw(cmd,ioaddr + EL3_CMD);
  for (i = 0; i < 2000; i++) {
    if (!(inw(ioaddr + EL3_STATUS) & CmdInProgress))
      return;
  }
  
  /* OK, that didn't work.  Do it the slow way.  One second */
  for (i = 0; i < 100000; i++) {
    if (!(inw(ioaddr + EL3_STATUS) & CmdInProgress)) {
      kprintf(KR_OSTREAM,"command 0x%x took %d usecs\n",cmd,i * 10);
      return;
    }
    capros_Sleep_sleep(KR_SLEEP,0.1);
  }
  //kprintf(KR_OSTREAM,"command 0x%x did not complete! Status=0x %x\n",
  //cmd, inw(ioaddr + EL3_STATUS));
}

/* Pre-Cyclone chips have no documented multicast filter, so the only
 * multicast setting is to receive all multicast frames.  At least
 * the chip has a very clean way to set the mode, unlike many others. */
static void 
set_rx_mode()
{
  uint32_t ioaddr = NETDEV.base_address[0];
  int new_mode;
  
  if (DEV_flags & IFF_PROMISC) {
    kprintf(KR_OSTREAM,"Setting promiscuous mode.");
    new_mode = SetRxFilter|RxStation|RxMulticast|RxBroadcast|RxProm;
  } else  if ((DEV_flags & IFF_ALLMULTI)) {
    new_mode = SetRxFilter|RxStation|RxMulticast|RxBroadcast;
  } else
    new_mode = SetRxFilter | RxStation | RxBroadcast;
  
  outw(new_mode, ioaddr + EL3_CMD);
}

/* Initialize the link. Also the various interrupts and status bits 
 * are "interesting"  */
static void
vortex_up()
{
  long ioaddr = NETDEV.base_address[0];
  struct vortex_private *vp = VORTEX_PRIVATE;
  unsigned int config;
  int i;
  
  /* Before initializing select the active media port. */
  EL3WINDOW(3);
  config = inl(ioaddr + Wn3_Config);
  
  if (vp->autoselect) {
    if (vp->has_nway) {
      DEBUG_3C905C kprintf(KR_OSTREAM, "using NWAY device table");
      DEV_if_port = XCVR_NWAY;
    } else {
      /* Find first available media type, starting with 100baseTx. */
      DEV_if_port = XCVR_100baseTx;
      while (!(vp->available_media & media_tbl[DEV_if_port].mask))
	DEV_if_port = media_tbl[DEV_if_port].next;
      DEBUG_3C905C kprintf(KR_OSTREAM,"first available media type: %s\n",
			   media_tbl[DEV_if_port].name);
    }
  } else {
    DEV_if_port = vp->default_media;
    DEBUG_3C905C 
      kprintf(KR_OSTREAM,"using default media %s",media_tbl[DEV_if_port].name);
  }

  /* FIX:: What does a timer do here?? */
#if 0  
  init_timer(&vp->timer);
  vp->timer.expires = RUN_AT(media_tbl[dev->if_port].wait);
  vp->timer.data = (unsigned long)dev;
  vp->timer.function = vortex_timer;              /* timer handler */
  add_timer(&vp->timer);

  init_timer(&vp->rx_oom_timer);
  vp->rx_oom_timer.data = (unsigned long)dev;
  vp->rx_oom_timer.function = rx_oom_timer;
#endif

  //kprintf(KR_OSTREAM,"Using media type %s",media_tbl[DEV_if_port].name);

  config = BFINS(config, DEV_if_port, 20, 4);
  //kprintf(KR_OSTREAM,"vortex_up(): writing 0x%x to InternalConfig", config);
  outl(config, ioaddr + Wn3_Config);
  
  if (DEV_if_port == XCVR_MII || DEV_if_port == XCVR_NWAY) {
    int mii_reg1, mii_reg5;
    EL3WINDOW(4);
    /* Read BMSR (reg1) only to clear old status. */
    mii_reg1 = mdio_read(vp->phys[0], 1);
    mii_reg5 = mdio_read(vp->phys[0], 5);
    if (mii_reg5 == 0xffff  ||  mii_reg5 == 0x0000){
      /* No MII device or no link partner report */
      kprintf(KR_OSTREAM,"No link partner report");
    } else {
      if ((mii_reg5 & 0x0100) != 0    /* 100baseTx-FD */
	  || (mii_reg5 & 0x00C0) == 0x0040) /* 10T-FD, but not 100-HD */
	vp->full_duplex = 1;
      //kprintf(KR_OSTREAM,"Net Carrier on ... [SUCCESS]");
    }
    vp->partner_flow_ctrl = ((mii_reg5 & 0x0400) != 0);
    kprintf(KR_OSTREAM,"MII #%d status %x, link partner capability %x,"
	    " info1 %x, setting %s-duplex.\n",
	    vp->phys[0],
	    mii_reg1, mii_reg5,
	    vp->info1, ((vp->info1 & 0x8000) || vp->full_duplex)
	    ? "full" : "half");
    EL3WINDOW(3);
  }

  /* Set the full-duplex bit. */
  outw(((vp->info1 & 0x8000) || vp->full_duplex ? 0x20 : 0) |
       (DEV_mtu > 1500 ? 0x40 : 0) |
       ((vp->full_duplex && vp->flow_ctrl && vp->partner_flow_ctrl)?0x100:0),
       ioaddr + Wn3_MAC_Ctrl);
  
  DEBUG_3C905C kprintf(KR_OSTREAM, "vortex_up() InternalConfig %x",config);
  
  issue_and_wait(TxReset);
  
  /* Don't reset the PHY - It upsets autonegotiation during DHCP operations */
  issue_and_wait(RxReset|0x04);

  outw(SetStatusEnb | 0x00, ioaddr + EL3_CMD);

  EL3WINDOW(4);
  DEBUG_3C905C kprintf(KR_OSTREAM,"vortex_up() irq %d media status %x.\n",
		       DEV_irq, inw(ioaddr + Wn4_Media));
  
  /* Set the station address and mask in window 2 each time opened. */
  EL3WINDOW(2);
  for (i = 0; i < 6; i++)   outb(DEV_eaddr[i], ioaddr + i);
  for (; i < 12; i+=2)      outw(0, ioaddr + i);


#if 0 /* We usually don't have these types of transceivers */
  if (DEV_if_port == XCVR_10base2)
    /* Start the thinnet transceiver. We should really wait 50ms...*/
    outw(StartCoax, ioaddr + EL3_CMD);
  if (DEV_if_port != XCVR_NWAY) {
    EL3WINDOW(4);
    outw((inw(ioaddr + Wn4_Media) & ~(Media_10TP|Media_SQE)) |
	 media_tbl[DEV_if_port].media_bits, ioaddr + Wn4_Media);
  }
#endif

  /* Switch to the stats window, and clear all stats by reading. */
  outw(StatsDisable, ioaddr + EL3_CMD);
  EL3WINDOW(6);
  for (i = 0; i < 10; i++)   inb(ioaddr + i);
  inw(ioaddr + 10);
  inw(ioaddr + 12);
  /* New: On the Vortex we must also clear the BadSSD counter. */
  EL3WINDOW(4);
  inb(ioaddr + 12);
  /* ..and on the Boomerang we enable the extra statistics bits. */
  outw(0x0040, ioaddr + Wn4_NetDiag);
  
  /* Switch to register set 7 for normal use. */
  EL3WINDOW(7);

  if (vp->full_bus_master_rx) { /* Boomerang bus master. */
    vp->cur_rx = vp->dirty_rx = 0;
    /* Initialize the RxEarly register as recommended. */
    outw(SetRxThreshold + (1536>>2), ioaddr + EL3_CMD);
    outl(0x0020, ioaddr + PktStatus);
    outl(virt_to_bus(vp->rx_ring_dma), ioaddr + UpListPtr);
  }
  if (vp->full_bus_master_tx) {           /* Boomerang bus master Tx. */
    vp->cur_tx = vp->dirty_tx = 0;
    if (vp->drv_flags & IS_BOOMERANG)
      outb(PKT_BUF_SZ>>8, ioaddr + TxFreeThreshold); /* Room for a packet. */
    /* Clear the Rx, Tx rings. */
    for (i = 0; i < RX_RING_SIZE; i++)    
      /* AKPM: this is done in vortex_open, too */
      vp->rx_ring[i].status = 0;
    for (i = 0; i < TX_RING_SIZE; i++)
      vp->tx_skbuff[i] = 0;
    outl(0, ioaddr + DownListPtr);
  }
  /* Set receiver mode: presumably accept b-case and phys addr only. */
  set_rx_mode();
  
  outw(StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics. */

  //issue_and_wait(SetTxStart|0x07ff);
  outw(RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
  outw(TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */
  /* Allow status bits to be seen. */
  vp->status_enable = SetStatusEnb | HostError|IntReq|StatsFull|TxComplete|
    (vp->full_bus_master_tx ? DownComplete : TxAvailable) |
    (vp->full_bus_master_rx ? UpComplete : RxComplete) |
    (vp->bus_master ? DMADone : 0);
  vp->intr_enable = SetIntrEnb | IntLatch | TxAvailable |RxComplete|
    StatsFull | HostError | TxComplete | IntReq
    | (vp->bus_master ? DMADone : 0) | UpComplete | DownComplete;
  
  outw(vp->status_enable, ioaddr + EL3_CMD);
  /* Ack all pending events, and set active indicator mask. */
  outw(AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
       ioaddr + EL3_CMD);
  outw(vp->intr_enable, ioaddr + EL3_CMD);
  
  kprintf(KR_OSTREAM,"vortex_up Done");
  return;
}


static void
vortex_down()
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  long ioaddr = NETDEV.base_address[0];
  
  /* Turn off statistics ASAP.  We update vp stats below. */
  outw(StatsDisable, ioaddr + EL3_CMD);
  
  /* Disable the receiver and transmitter. */
  outw(RxDisable, ioaddr + EL3_CMD);
  outw(TxDisable, ioaddr + EL3_CMD);
  
  if (DEV_if_port == XCVR_10base2)
    /* Turn off thinnet power.  Green! */
    outw(StopCoax, ioaddr + EL3_CMD);
  
  outw(SetIntrEnb | 0x0000, ioaddr + EL3_CMD);
  
  update_stats(ioaddr);
  if (vp->full_bus_master_rx)
    outl(0, ioaddr + UpListPtr);
  if (vp->full_bus_master_tx)
    outl(0, ioaddr + DownListPtr);
  
  return;
}

static int
vortex_open() 
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  int i;
  
  if (vp->full_bus_master_rx) { /* Boomerang bus master. */
    DEBUG_3C905C kprintf(KR_OSTREAM,"Filling in the Rx ring.");
    for (i = 0; i < RX_RING_SIZE; i++) {
      vp->rx_ring[i].next = virt_to_bus(vp->rx_ring_dma + 
					sizeof(struct boom_rx_desc)*(i+1) );
      vp->rx_ring[i].status = 0;      /* Clear complete bit. */
      vp->rx_ring[i].length = PKT_BUF_SZ | LAST_FRAG;
      vp->rx_ring[i].addr = virt_to_bus(BUF_START + i*PKT_BUF_SZ);
    }
    
    /* Wrap the ring. */
    vp->rx_ring[i-1].next = virt_to_bus(vp->rx_ring_dma);
  }
  vortex_up();
  
  return 0;
}

/* Talk to the tornado chip, get mac address etc.*/
int
vortex_probe1() 
{
  unsigned int eeprom[0x40],checksum = 0;
  struct vortex_private *vp;
  int i,step;
  
  
  VORTEX_PRIVATE = (void *)DMA_START; /* 2 GB into the address space */
  vp = VORTEX_PRIVATE;
  
  DEV_irq = NETDEV.irq;
  DEV_mtu = mtu;
  vp->rx_ring_dma = DMA_START;
  vp->tx_ring_dma = vp->rx_ring_dma + sizeof(struct boom_rx_desc)*RX_RING_SIZE;
  vp->drv_flags = IS_TORNADO|HAS_NWAY|HAS_HWCKSM;
  vp->io_size = 128;
  vp->has_nway = (vp->drv_flags & HAS_NWAY) ? 1 : 0;
    
  /* Read the station address from the EEPROM. */
  EL3WINDOW(0);
  {
    int base = EEPROM_Read;
    
    for (i = 0; i < 0x40; i++) {
      int timer;
      outw(base + i,NETDEV.base_address[0] + Wn0EepromCmd);
      /* Pause for at least 162 us. for the read to take place. */
      for (timer = 10; timer >= 0; timer--) {
	capros_Sleep_sleep(KR_SLEEP,2);
	if ((inw(NETDEV.base_address[0] + Wn0EepromCmd) & 0x8000) == 0)
	  break;
      }
      eeprom[i] = inw(NETDEV.base_address[0] + Wn0EepromData);
    }
  }
  
  for (i = 0; i < 0x18; i++)
    checksum ^= eeprom[i];
  checksum = (checksum ^ (checksum >> 8)) & 0xff;
  if (checksum != 0x00) {      /* Grrr, needless incompatible change 3Com */
    while (i < 0x21) 
      checksum ^= eeprom[i++]; 
    checksum = (checksum ^ (checksum >> 8)) & 0xff;
  }
     
  /* Print the MAC ADDress */
  DEV_eaddr[0] = eeprom[10 + 0]>>8;
  DEV_eaddr[1] = eeprom[10 + 0]&0xFF;
  DEV_eaddr[2] = eeprom[10 + 1]>>8;
  DEV_eaddr[3] = eeprom[10 + 1]&0xFF;
  DEV_eaddr[4] = eeprom[10 + 2]>>8;
  DEV_eaddr[5] = eeprom[10 + 2]&0xFF;
  
  kprintf(KR_OSTREAM,"%x:%x:%x:%x:%x:%x",
	  DEV_eaddr[0],DEV_eaddr[1],DEV_eaddr[2],
	  DEV_eaddr[3],DEV_eaddr[4],DEV_eaddr[5]);

  EL3WINDOW(2); 
  for (i = 0; i < 6; i++)
    outb(htons(eeprom[i+10]),NETDEV.base_address[0] + i);
   
  EL3WINDOW(4);
  step = (inb(NETDEV.base_address[0] + Wn4_NetDiag) & 0x1e) >> 1;
  kprintf(KR_OSTREAM,"  product code %02x%02x rev %02x.%d date %02d-"
	  "%02d-%02d\n", eeprom[6]&0xff, eeprom[6]>>8, eeprom[0x14],
	  step, (eeprom[4]>>5) & 15, eeprom[4] & 31, eeprom[4]>>9);
   
  /* Extract our information from the EEPROM data. */
  vp->info1 = eeprom[13];
  vp->info2 = eeprom[15];
  vp->capabilities = eeprom[16];
   
  if (vp->info1 & 0x8000) {
    vp->full_duplex = 1;
    kprintf(KR_OSTREAM,"Full duplex capable");
  }
   
  {
    static const char * ram_split[] = {"5:3", "3:1", "1:1", "3:5"};
    unsigned int config;
     
    EL3WINDOW(3);
    vp->available_media = inw(NETDEV.base_address[0] + Wn3_Options);
    if ((vp->available_media & 0xff) == 0)         /* Broken 3c916 */
      vp->available_media = 0x40;
    config = inl(NETDEV.base_address[0] + Wn3_Config);
    DEBUG_3C905C kprintf(KR_OSTREAM,"Internal config register is %x, "
			 "transceivers %x", config,
			 inw(NETDEV.base_address[0] + Wn3_Options));
    DEBUG_3C905C
      kprintf(KR_OSTREAM,"%dK %s-wide RAM %s Rx:Tx split, %s%s interface.\n",
	      8 << RAM_SIZE(config),
	      RAM_WIDTH(config) ? "word" : "byte",
	      ram_split[RAM_SPLIT(config)],
	      AUTOSELECT(config) ? "autoselect/" : "",
	      XCVR(config) > XCVR_ExtMII ? "<invalid transceiver>" :
	      media_tbl[XCVR(config)].name);
    vp->default_media = XCVR(config);
    if (vp->default_media == XCVR_NWAY)
      vp->has_nway = 1;
    vp->autoselect = AUTOSELECT(config);
  }
  
  DEV_if_port = XCVR_NWAY;
  
  if (DEV_if_port == XCVR_MII || DEV_if_port == XCVR_NWAY) {
    int phy, phy_idx = 0;
    EL3WINDOW(4);
    mii_preamble_required++;
    mii_preamble_required++;
    mdio_read(24, 1);
    for (phy = 0; phy < 32 && phy_idx < 1; phy++) {
      int mii_status, phyx;
      
      /* For the 3c905CX we look at index 24 first, because it bogusly
       * reports an external PHY at all indices */
      if (phy == 0)
	phyx = 24;
      else if (phy <= 24)
	phyx = phy - 1;
      else
	phyx = phy;
      mii_status = mdio_read(phyx, 1);
      if (mii_status  &&  mii_status != 0xffff) {
	vp->phys[phy_idx++] = phyx;
	kprintf(KR_OSTREAM,"MII transceiver found at address %d, status %x",
		phyx, mii_status);
	if ((mii_status & 0x0040) == 0)
	  mii_preamble_required++;
      }
    }
    mii_preamble_required--;
    if (phy_idx == 0) {
      kprintf(KR_OSTREAM,"***WARNING*** No MII transceivers found!\n");
      vp->phys[0] = 24;
    } else {
      vp->advertising = mdio_read(vp->phys[0], 4);
      if (vp->full_duplex) {
	/* Only advertise the FD media types. */
	vp->advertising &= ~0x02A0;
	mdio_write(vp->phys[0], 4, vp->advertising);
      }
    }
  }
  
  /* Activate xcvr */
  {
    int reset_opts;
    int ioaddr = NETDEV.base_address[0];
    
    EL3WINDOW(2);
    reset_opts = inw(ioaddr + Wn2_ResetOptions);
    outw(reset_opts, ioaddr + Wn2_ResetOptions);
  }
  
  if (vp->capabilities & CapBusMaster) {
    vp->full_bus_master_tx = 1;
    DEBUG_3C905C 
      kprintf(KR_OSTREAM,"  Enabling bus-master transmits and %s receives.\n",
	      (vp->info2 & 1) ? "early" : "whole-frame" );
    vp->full_bus_master_rx = (vp->info2 & 1) ? 1 : 2;
    vp->bus_master = 0;             /* AKPM: vortex only */
  }

  DEBUG_3C905C
    kprintf(KR_OSTREAM,"busmaster_tx = %d, rx=%d ,mas = %d",
	    vp->full_bus_master_tx,vp->full_bus_master_rx,vp->bus_master);

  return RC_OK;
}

static void
dump_tx_ring()
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  long ioaddr = NETDEV.base_address[0];
  
  if (vp->full_bus_master_tx) {
    int i;
    int stalled = inl(ioaddr + PktStatus) & 0x04;   /* Possible racy. 
						     * But it's only debug stuff */
    
    kprintf(KR_OSTREAM,"Flags; bus-master %d, dirty %d(%d) current %d(%d)\n",
	    vp->full_bus_master_tx,
	    vp->dirty_tx, vp->dirty_tx % TX_RING_SIZE,
	    vp->cur_tx, vp->cur_tx % TX_RING_SIZE);
    kprintf(KR_OSTREAM,"Transmit list %x vs. %p.\n",inl(ioaddr + DownListPtr),
	    &vp->tx_ring[vp->dirty_tx % TX_RING_SIZE]);
    issue_and_wait(DownStall);
    for (i = 0; i < TX_RING_SIZE; i++) {
      kprintf(KR_OSTREAM,"%d: @%p  length %x status %x", i,&vp->tx_ring[i],
#if DO_ZEROCOPY
	      le32_to_cpu(vp->tx_ring[i].frag[0].length),
#else
	      le32_to_cpu(vp->tx_ring[i].length),
#endif
	      le32_to_cpu(vp->tx_ring[i].status));
    }
    if (!stalled)
      outw(DownUnstall, ioaddr + EL3_CMD);
  }
}

 
/* Update statistics.
 * Unlike with the EL3 we need not worry about interrupts changing
 * the window setting from underneath us, but we must still guard
 * against a race condition with a StatsUpdate interrupt updating the
 * table.  This is done by checking that the ASM (!) code generated uses
 * atomic updates with '+='. */
static void 
update_stats(long ioaddr)
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  int old_window = inw(ioaddr + EL3_CMD);
  
  if (old_window == 0xffff)       /* Chip suspended or ejected. */
    return;
  /* Unlike the 3c5x9 we need not turn off stats updates while reading. */
  /* Switch to the stats window, and read everything. */
  EL3WINDOW(6);
  vp->stats.tx_carrier_errors             += inb(ioaddr + 0);
  vp->stats.tx_heartbeat_errors   += inb(ioaddr + 1);
  /* Multiple collisions. */              inb(ioaddr + 2);
  vp->stats.collisions                    += inb(ioaddr + 3);
  vp->stats.tx_window_errors              += inb(ioaddr + 4);
  vp->stats.rx_fifo_errors                += inb(ioaddr + 5);
  vp->stats.tx_packets                    += inb(ioaddr + 6);
  vp->stats.tx_packets                    += (inb(ioaddr + 9)&0x30) << 4;
  /* Rx packets   */                              
  inb(ioaddr + 7);   /* Must read to clear */
  /* Tx deferrals */           
  inb(ioaddr + 8);
  /* Don't bother with register 9, an extension of registers 6&7.
   * If we do use the 6&7 values the atomic update assumption above
   * is invalid. */
  vp->stats.rx_bytes += inw(ioaddr + 10);
  vp->stats.tx_bytes += inw(ioaddr + 12);
  /* New: On the Vortex we must also clear the BadSSD counter. */
  EL3WINDOW(4);
  inb(ioaddr + 12);
  {
    uint8_t up = inb(ioaddr + 13);
    vp->stats.rx_bytes += (up & 0x0f) << 16;
    vp->stats.tx_bytes += (up & 0xf0) << 12;
  }
  
  EL3WINDOW(old_window >> 13);
  return;
}

int
boomerang_start_xmit(uint8_t *dest,int type,int size, char *pkt)
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  long ioaddr = NETDEV.base_address[0];
  int i;
  
  /* Calculate the next Tx descriptor entry. */
  int entry = vp->cur_tx % TX_RING_SIZE;
  struct boom_tx_desc *prev_entry = &vp->tx_ring[(vp->cur_tx-1)%TX_RING_SIZE];
  
  size = 1500;

  //DEBUG_3C905C
  kprintf(KR_OSTREAM,"txing a packet - %d, Tx index %d,%d",
	  size,vp->cur_tx,vp->dirty_tx);
  
  if (vp->cur_tx - vp->dirty_tx >= TX_RING_SIZE) {
    kprintf(KR_OSTREAM,"BUG! Tx Ring full, refusing to send buffer.\n");
    return 1;
  }
  
  /* Form the ethernet header */
  for(i=0;i<ETH_ADDR_LEN;i++) {
    vp->tbuf[i] = dest[i];
    vp->tbuf[i+ETH_ADDR_LEN] = DEV_eaddr[i];
  }
  vp->tbuf[ETH_ADDR_LEN*2] = type >> 8;
  vp->tbuf[ETH_ADDR_LEN*2+1] = type;

  /* Copy over the data */
  for(i=0;i<size;i++) vp->tbuf[i+ETH_ADDR_LEN*2+2] = pkt[i];

  vp->tx_ring[entry].next = 0;
  vp->tx_ring[entry].addr = cpu_to_le32(virt_to_bus(vp->tbuf));
  vp->tx_ring[entry].length = cpu_to_le32((size + 14) | LAST_FRAG);
  vp->tx_ring[entry].status = cpu_to_le32((size + 14)| TxIntrUploaded);
  
  /* Wait for the stall to complete. */
  issue_and_wait(DownStall);
  prev_entry->next = cpu_to_le32(vp->tx_ring_dma + 
				 entry * sizeof(struct boom_tx_desc));
  if (inl(ioaddr + DownListPtr) == 0) {
    outl(virt_to_bus(vp->tx_ring_dma + entry * sizeof(struct boom_tx_desc)), 
	 ioaddr + DownListPtr);
    vp->queued_packet++;
  }
  
  vp->cur_tx++;
  if (vp->cur_tx - vp->dirty_tx > TX_RING_SIZE - 1) {
    kprintf(KR_OSTREAM,"Error::Need to stop out Queue,"
	    "cur_tx = %d, dirty_tx = %d",vp->cur_tx,vp->dirty_tx);
    return 0;
  } else {/* Clear previous interrupt enable. */
    /* Dubious. If in boomeang_interrupt "faster" cyclone ifdef
     * were selected, this would corrupt DN_COMPLETE. No? */
    prev_entry->status &= cpu_to_le32(~TxIntrUploaded);
  }
  outw(DownUnstall, ioaddr + EL3_CMD);
  kprintf(KR_OSTREAM,"Sending packet");
  return 0;
}


/* Receive a packet. The ISR calls this when a upload dma is complete */
int
boomerang_rx()
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  int entry = vp->cur_rx % RX_RING_SIZE;
  long ioaddr = NETDEV.base_address[0];
  int rx_status;
  int rx_work_limit = vp->dirty_rx + RX_RING_SIZE - vp->cur_rx;
  uint32_t dma = virt_to_bus(BUF_START);

  DEBUG_3C905C
    kprintf(KR_OSTREAM,"boomerang_rx(): status %x", inw(ioaddr+EL3_STATUS));
  
  while ((rx_status = le32_to_cpu(vp->rx_ring[entry].status)) & RxDComplete){
    if (--rx_work_limit < 0)
      break;
    if (rx_status & RxDError) { /* Error, update stats. */
      unsigned char rx_error = rx_status >> 16;
      kprintf(KR_OSTREAM, " Rx error: status %x.\n", rx_error);
      vp->stats.rx_errors++;
      if (rx_error & 0x01)  vp->stats.rx_over_errors++;
      if (rx_error & 0x02)  vp->stats.rx_length_errors++;
      if (rx_error & 0x04)  vp->stats.rx_frame_errors++;
      if (rx_error & 0x08)  vp->stats.rx_crc_errors++;
      if (rx_error & 0x10)  vp->stats.rx_length_errors++;
    } else {
      /* The packet length: up to 4.5K!. */
      int pkt_len = rx_status & 0x1fff;
      dma = le32_to_cpu(vp->rx_ring[entry].addr);
      
      DEBUG_3C905C
	kprintf(KR_OSTREAM,"Receiving packet size %d status %x at dma = %x",
		pkt_len,rx_status,dma);
      
#if 0
      /* Print the packet */
      {
	int i;
	char *s = (char *)bus_to_virt(vp->rx_ring[entry].addr);
	
	for(i=14;i<pkt_len;i++) {
	  kprintf(KR_OSTREAM,"p[%d]=%x=%c",i,s[i],s[i]);
	}
      }
#endif
      /* Use hardware checksum info. */
      {                               
	int csum_bits = rx_status & 0xee000000;
	if (csum_bits &&
	    (csum_bits == (IPChksumValid | TCPChksumValid) ||
	     csum_bits == (IPChksumValid | UDPChksumValid))) {
	  //skb->ip_summed = CHECKSUM_UNNECESSARY;
	  vp->rx_csumhits++;
	}
      }
      vp->stats.rx_packets++;
    }
    entry = (++vp->cur_rx) % RX_RING_SIZE;
  }
  /* Refill the Rx ring buffers. */
  for (; vp->cur_rx - vp->dirty_rx > 0; vp->dirty_rx++) {
    entry = vp->dirty_rx % RX_RING_SIZE;
    if (vp->rx_skbuff[entry] == NULL) {
      vp->rx_ring[entry].addr = dma;
    }
    vp->rx_ring[entry].status = 0;  /* Clear complete bit. */
    outw(UpUnstall, ioaddr + EL3_CMD);
  }
  return 0;
}


/* Handle uncommon interrupt sources.  This is a separate routine to minimize
 * the cache impact. */
static void
vortex_error(int status)
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  long ioaddr = NETDEV.base_address[0];
  int do_tx_reset = 0, reset_mask = 0;
  unsigned char tx_status = 0;

  kprintf(KR_OSTREAM,"vortex_error(), status=0x%x\n",status);
  if (status & TxComplete) {          /* Really "TxError" for us. */
    tx_status = inb(ioaddr + TxStatus);
    /* Presumably a tx-timeout. We must merely re-enable. */
    if(tx_status != 0x88 ) {
      kprintf(KR_OSTREAM, "Transmit error, Tx status register %x",tx_status);
      if (tx_status == 0x82) {
	kprintf(KR_OSTREAM,"Probably a duplex mismatch.  See "
		"Documentation/networking/vortex.txt\n");
      }
      dump_tx_ring();
    }
    if (tx_status & 0x14)  vp->stats.tx_fifo_errors++;
    if (tx_status & 0x38)  vp->stats.tx_aborted_errors++;
    outb(0, ioaddr + TxStatus);
    if (tx_status & 0x30) {                 /* txJabber or txUnderrun */
      do_tx_reset = 1;
    } else if ((tx_status & 0x08) && (vp->drv_flags & MAX_COLLISION_RESET)) {
      /* maxCollisions */
      do_tx_reset = 1;
      reset_mask = 0x0108;            /* Reset interface logic, but
				       * not download logic */
    } else {                          /* Merely re-
				       * enable the transmitter. */
      outw(TxEnable, ioaddr + EL3_CMD);
    }
  }
  
  if (status & RxEarly) {                         /* Rx early is unused. */
    boomerang_rx();
    outw(AckIntr | RxEarly, ioaddr + EL3_CMD);
  }
  if (status & StatsFull) {                       /* Empty statistics. */
    static int DoneDidThat;
    kprintf(KR_OSTREAM,"Updating stats.\n");
    update_stats(ioaddr);
    /* HACK: Disable statistics as an interrupt source. */
    /* This occurs when we have the wrong media type! */
    if (DoneDidThat == 0  &&
	inw(ioaddr + EL3_STATUS) & StatsFull) {
      kprintf(KR_OSTREAM,"Updating statistics failed, disabling "
	      "stats as an interrupt source.");
      EL3WINDOW(5);
      outw(SetIntrEnb | (inw(ioaddr + 10) & ~StatsFull), ioaddr + EL3_CMD);
      vp->intr_enable &= ~StatsFull;
      EL3WINDOW(7);
      DoneDidThat++;
    }
  }
  if (status & IntReq) {          /* Restore all interrupt sources.  */
    outw(vp->status_enable, ioaddr + EL3_CMD);
    outw(vp->intr_enable, ioaddr + EL3_CMD);
  }
  if (status & HostError) {
    uint16_t fifo_diag;
    EL3WINDOW(4);
    fifo_diag = inw(ioaddr + Wn4_FIFODiag);
    kprintf(KR_OSTREAM,"Host error, FIFO diagnostic register %x",fifo_diag);
    /* Adapter failure requires Tx/Rx reset and reinit. */
    if (vp->full_bus_master_tx) {
      int bus_status = inl(ioaddr + PktStatus);
      /* 0x80000000 PCI master abort. */
      /* 0x40000000 PCI target abort. */
      kprintf(KR_OSTREAM,"PCI bus error, bus status %x", bus_status);

      /* In this case, blow the card away */
      vortex_down();
      issue_and_wait(TotalReset | 0xff);
      vortex_up();          /* AKPM: bug.  vortex_up() assumes th
			     * at the rx ring is full. It may not be. */
    } else if (fifo_diag & 0x0400)
      do_tx_reset = 1;
    if (fifo_diag & 0x3000) {
      /* Reset Rx fifo and upload logic */
      issue_and_wait(RxReset|0x07);
      /* Set the Rx filter to the current state. */
      set_rx_mode();
      outw(RxEnable, ioaddr + EL3_CMD); /* Re-enable the receiver */
      outw(AckIntr | HostError, ioaddr + EL3_CMD);
    }
  }
  if (do_tx_reset) {
    issue_and_wait(TxReset|reset_mask);
    outw(TxEnable, ioaddr + EL3_CMD);
    //if (!vp->full_bus_master_tx)
    //netif_wake_queue(dev);
  }
}

/* This is the ISR for the boomerang series chips.
 * full_bus_master_tx == 1 && full_bus_master_rx == 1 */
static void 
boomerang_interrupt()
{
  struct vortex_private *vp = VORTEX_PRIVATE;
  long ioaddr;
  int status;
  int work_done = max_interrupt_work;
  
  ioaddr = NETDEV.base_address[0];
  status = inw(ioaddr + EL3_STATUS);
  DEBUG_3C905C 
    kprintf(KR_OSTREAM,"boomerang_interrupt. status=0x%x\n", status);
  
  if ((status & IntLatch) == 0)
    goto handler_exit;         /* No interrupt: shared IRQs can cause this */
  
  if (status == 0xffff) {         /* h/w no longer present (hotplug)? */
    kprintf(KR_OSTREAM, "boomerang_interrupt(1): status = 0xffff\n");
    goto handler_exit;
  }

  if (status & IntReq) {
    status |= vp->deferred;
    vp->deferred = 0;
  }

  kprintf(KR_OSTREAM,"int. status %x,latency %d ticks",
	  status,inb(ioaddr + Timer));
  do {
    kprintf(KR_OSTREAM, "In interrupt loop, status %x",status);
    if (status & UpComplete) {
      outw(AckIntr | UpComplete, ioaddr + EL3_CMD);
      kprintf(KR_OSTREAM, "boomerang_interrupt->boomerang_rx");
      boomerang_rx();
    }
    
    if (status & DownComplete) {
      unsigned int dirty_tx = vp->dirty_tx;
      kprintf(KR_OSTREAM,"Boomerang TX=-----");
      outw(AckIntr | DownComplete, ioaddr + EL3_CMD);
      while (vp->cur_tx - dirty_tx > 0) {
	int entry = dirty_tx % TX_RING_SIZE;
	/* AKPM: the latter is faster, but cyclone-only */
	if (inl(ioaddr + DownListPtr) ==
	    vp->tx_ring_dma + entry * sizeof(struct boom_tx_desc)) {
	  kprintf(KR_OSTREAM,"It still has not been processed");
	  break;                  /* It still hasn't be
				     en processed. */
	}
	if (vp->tx_skbuff[entry]) {
	  //struct sk_buff *skb = vp->tx_skbuff[entry];
#if DO_ZEROCOPY                                 
	  int i;
	  for (i=0; i<=skb_shinfo(skb)->nr_frags; i++)
	    pci_unmap_single(vp->pdev,
			     le32_to_cpu(vp->tx_ring[entry].frag[i].addr),
			     le32_to_cpu(vp->tx_ring[entry].frag[i].length)&
			     0xFFF,
			     PCI_DMA_TODEVICE);
#else
	  //pci_unmap_single(vp->pdev,
	  //le32_to_cpu(vp->tx_ring[entry].addr),
	  //skb->len, PCI_DMA_TODEVICE);
#endif
	  //dev_kfree_skb_irq(skb);
	  vp->tx_skbuff[entry] = 0;
	} else {
	  kprintf(KR_OSTREAM, "boomerang_interrupt: no skb!\n");
	}
	/* vp->stats.tx_packets++;  Counted below. */
	dirty_tx++;
      }
      vp->dirty_tx = dirty_tx;
      if (vp->cur_tx - dirty_tx <= TX_RING_SIZE - 1) {
	kprintf(KR_OSTREAM, "boomerang_interrupt: wake queue cur,dir=%d,%d",
		vp->cur_tx,vp->dirty_tx);
	//netif_wake_queue (dev);
      }
    }

    /* Check for all uncommon interrupts at once. */
    if (status & (HostError | RxEarly | StatsFull | TxComplete | IntReq)) {
      kprintf(KR_OSTREAM,"vortex error ");
      vortex_error(status);
    }

    if (--work_done < 0) {
      kprintf(KR_OSTREAM,"Too much work in interrupt, status %x\n",status);
      /* Disable all pending interrupts. */
      do {
	vp->deferred |= status;
	outw(SetStatusEnb | (~vp->deferred & vp->status_enable),ioaddr + EL3_CMD);
	outw(AckIntr | (vp->deferred & 0x7ff), ioaddr + EL3_CMD);
      } while ((status = inw(ioaddr + EL3_CMD)) & IntLatch);
      /* The timer will reenable interrupts. */
      //mod_timer(&vp->timer, jiffies + 1*HZ);
      break;
    }
    /* Acknowledge the IRQ. */
    outw(AckIntr | IntReq | IntLatch , ioaddr + EL3_CMD);
  } while ((status = inw(ioaddr + EL3_STATUS)) & IntLatch);
  kprintf(KR_OSTREAM,"exiting interrupt, status %x\n",status);
 handler_exit:
  
  return;
}


/* Start the helper thread to wait for interrupts */
int
StartHelper() 
{
  result_t result;
  Message msg;
  
  result = constructor_request(KR_HELPER_C, KR_BANK, KR_SCHED, 
			       KR_START,KR_HELPER_S);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "lance:Constructing helper ... [FAILED]");
    return RC_HELPER_START_FAILED;
  }else kprintf(KR_OSTREAM, "lance:Constructing helper ... [SUCCESS]");
  
  /* Pass on which irq to wait on */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_HELPER_S;
  msg.snd_w1 = NETDEV.irq;
  CALL(&msg);
  kprintf(KR_OSTREAM,"3com:sending IRQ no ... [SUCCESS]");
  
  return RC_OK;
}

/* Initialize the pci_probe domain, do a probe and search for 3c905c card
 * using vendor and device ids */
int
_3c905c_probe()
{
  result_t result;
  uint32_t total;
  unsigned short vendor = VENDOR_3COM;
  int i;
  
  /* Construct an instance of the pci probe domain */
  result = constructor_request(KR_PCI_PROBE_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_PCI_PROBE_C);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  /* Initialize the probe */
  result = pciprobe_initialize(KR_PCI_PROBE_C);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  DEBUG_3C905C kprintf(KR_OSTREAM,"PciProbe init ...  %s.\n",
		       (result == RC_OK) ? "SUCCESS" : "FAILED");
  
  /* Find all the devices of the vendor 3COM */
  result = pciprobe_vendor_total(KR_PCI_PROBE_C, vendor, &total);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  DEBUG_3C905C  kprintf(KR_OSTREAM,"No of 3COM Devices found = %d",total);
  
  /* Get the Device ID to check if it is 3COM 3c905c
   * From pci_ids.h the 3com card had an ID 0x9200*/
  result = pciprobe_vendor_next(KR_PCI_PROBE_C, vendor,0,&NETDEV);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  DEBUG_3C905C {
    kprintf(KR_OSTREAM,"Pci_Find_VendorID ... %s",
	    (result == RC_OK) ? "SUCCESS" : "FAILED");
    kprintf(KR_OSTREAM,"RCV:00:%02x [%04x/%04x] BR[0] %x IRQ %d,MASTER=%s\n", 
	    NETDEV.devfn, NETDEV.vendor,
	    NETDEV.device,NETDEV.base_address[0],NETDEV.irq,
	    NETDEV.master?"YES":"NO");
  }
  
  /* FIX:: Anshumal Do u know the reason for this - even byte alignment??*/
  NETDEV.base_address[0]--;
  
  /* FIX:: Should use the range allocation manager. */
  /* FIX: ask Shap about this -- Merge Bug??? at 0x5000000u */
  for(i=0x1000000u;i>0;i+=DMA_SIZE) {
    /* Hopefully we can do DMA onto this RAM area */
    result = capros_DevPrivs_publishMem(KR_DEVPRIVS,i,i+DMA_SIZE, 0);
    if(result==RC_OK) {
      kprintf(KR_OSTREAM,"Published mem at(%x)",i);
      PHYSADDR = i;
      break;
    }
  }
  
  /* Now we have to reflect this in our address space. As we need to
   * access this. So ask memmap to create our addressable tree for us.*/    
  result = constructor_request(KR_MEMMAP_C,KR_BANK,KR_SCHED,KR_VOID,KR_DMA);
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
  
  /* Do a probe for the tornado card. the 3com 3c59 series is generically
   * named as vortex. */
  result = vortex_probe1();   /* Do a vortex specific - probe */
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Vortex Probe ... [FAILED = %d]",result);
    return result;
  }
  
  /* Allocate the IRQ in the pci device structure */
  result = capros_DevPrivs_allocIRQ(KR_DEVPRIVS,NETDEV.irq,0);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"IRQ %d not allocated",NETDEV.irq);
    return RC_IRQ_ALLOC_FAILED;
  }else   
    kprintf(KR_OSTREAM,"Allocating IRQ %d ... [SUCCESS]",NETDEV.irq);
  
  /* Start the helper process which waits on the pci irq for us and 
   * signals us of irq arrival */
  if((result=StartHelper())!= RC_OK) {
    kprintf(KR_OSTREAM,"Starting Helper ... [FAILED %d]",result);
    return result;
  }
  
  /* get the vortex up and running */
  if((result=vortex_open())!= RC_OK) {
    kprintf(KR_OSTREAM,"Vortex Open ... [FAILED %d]",result);
    return result;
  }
  
  DEBUG_3C905C kprintf(KR_OSTREAM,"TORNADO Init ... [SUCCESS - stat = %x]",
		       inw(NETDEV.base_address[0]+EL3_STATUS));
  
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
   
  case OC_irq_arrived:
    {
      /* helper process has called us, as an irq has arrived 
       * Call the isr */
      DEBUG_3C905C kprintf(KR_OSTREAM,"irq arrived");
      boomerang_interrupt();
      return 1;
    }
  case OC_netif_xmit:
    {
      uint8_t dest[ETH_ADDR_LEN];
      int type;
      
      type = msg->rcv_w1;
      
      dest[3] = msg->rcv_w2 & 0xff ;       dest[0] =  msg->rcv_w3 & 0xff;
      dest[4] = (msg->rcv_w2>>8) & 0xff;   dest[1] = (msg->rcv_w3>>8) & 0xff;
      dest[5] = (msg->rcv_w2>>16) & 0xff;  dest[2] = (msg->rcv_w3>>16) & 0xff;
      
      msg->snd_code = boomerang_start_xmit(dest,type,msg->rcv_sent,rcv_buffer);
      return 1;
    }
  default:
    break;
  }
  
  msg->snd_code = RC_capros_key_UnknownRequest;
  return 1;
}


int 
main(void)
{
  Message msg;
  
  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM,   KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_PCI_PROBE_C,  KR_PCI_PROBE_C);
  capros_Node_getSlot(KR_CONSTIT, KC_DEVPRIVS,  KR_DEVPRIVS);
  capros_Node_getSlot(KR_CONSTIT, KC_PHYSRANGE,  KR_PHYSRANGE);
  capros_Node_getSlot(KR_CONSTIT, KC_MEMMAP_C, KR_MEMMAP_C);
  capros_Node_getSlot(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  capros_Node_getSlot(KR_CONSTIT, KC_HELPER_C, KR_HELPER_C);

  /* Move the DEVPRIVS key to the ProcIoSpace slot so we can do io calls */
  capros_arch_i386_Process_setIoSpace(KR_SELF, KR_DEVPRIVS);

  capros_Process_makeStartKey(KR_SELF, 0, KR_START);
  
  /* Probe for a tornado type network card, if it exists initialize it
   * and return a start key . */
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START;
  msg.snd_key1   = KR_VOID;
  msg.snd_key2   = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_data = 0;
  msg.snd_len  = 0;
  msg.snd_code = _3c905c_probe();
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
    msg.snd_data = 0;
    msg.snd_len = 0;
    msg.snd_invKey = KR_RETURN;
    msg.snd_rsmkey = KR_RETURN;
    RETURN(&msg);
  } while (ProcessRequest(&msg));
  
  return 0;
}
