/*
 * tg3.c: Broadcom Tigon3 ethernet driver.
 *
 * Copyright (C) 2001, 2002 David S. Miller (davem@redhat.com)
 * Copyright (C) 2001, 2002 Jeff Garzik (jgarzik@pobox.com)
 */
/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 * Copyright (C) 2007, 2008, Strawberry Development Group.
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

#include <unistd.h>
#include <string.h>

#include "tg3.h"
#include "ethtool.h"
#include "mii.h"
#include "error_codes.h"
#include "pci.h"
#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_PCI_PROBE_C  KR_APP(1)
#define KR_MEMMAP_C     KR_APP(2)
#define KR_PCI_PROBE    KR_APP(3)
#define KR_DEVPRIVS     KR_APP(4)
#define KR_PHYSRANGE    KR_APP(5)
#define KR_ADDRSPC      KR_APP(6)
#define KR_DMA          KR_APP(7)
#define KR_REGS         KR_APP(8)
#define KR_SLEEP        KR_APP(9)
#define KR_START        KR_APP(10)
#define KR_HELPER_C     KR_APP(11)
#define KR_HELPER_S     KR_APP(12)

#define KR_SCRATCH      KR_APP(19)

#define DEBUG_ALTIMA    if(0)
#define MAX_WAIT_CNT    1000

#define DEVICE_ALTIMA_AC9100     0x03EA
#define VENDOR_ALTIMA_AC9100     0x173B

#define DMA_START       0x80000000u /* 2 GB into address space */
#define REGS_START      0x88000000u /* 2.125 GB into the address space */
#define DMA_SIZE        0x00300000u /* DMA Size to be dev publish*/

#define ETH_ADDR_LEN     6
#define ETH_FRAME_LEN  1518
#define ETH_DATA_LEN   1518
#define ETH_HLEN        14

#define MAX_SKB_FRAGS   6

#define virt_to_bus(x) (PHYSADDR + x - DMA_START)
#define bus_to_virt(x) (DMA_START + x - PHYSADDR)

#define le32_to_cpu(x)  x//htons(x)
#define cpu_to_le32(x)  x//ntohs(x)

/* For now define writel/readl to be outl & inl */
//#define writel(v,p) outl((v),(unsigned long)p)
//#define readl(p)    inl((unsigned long)p)

#define PCI_FUNC(devfn)             ((devfn) & 0x07)
#define swab32(x) \
({ \
        __uint32_t __x = (x); \
        ((__uint32_t)( \
                (((__uint32_t)(__x) & (__uint32_t)0x000000ffUL) << 24) | \
                (((__uint32_t)(__x) & (__uint32_t)0x0000ff00UL) <<  8) | \
                (((__uint32_t)(__x) & (__uint32_t)0x00ff0000UL) >>  8) | \
                (((__uint32_t)(__x) & (__uint32_t)0xff000000UL) >> 24) )); \
})

#define netif_carrier_on(x)  1
#define netif_carrier_off(x) 0
#define netif_carrier_ok(x)  1

#define CHECKSUM_NONE 0
#define CHECKSUM_HW 1
#define CHECKSUM_UNNECESSARY 2

#define TG3_DEF_MAC_MODE        0
#define TG3_DEF_RX_MODE         0
#define TG3_DEF_TX_MODE         0

/* length of time before we decide the hardware is borked,
 * and dev->tx_timeout() should be called to fix the problem */
#define TG3_TX_TIMEOUT                  (5 * HZ)

/* hardware minimum and maximum for a single frame's data payload */
#define TG3_MIN_MTU                     60
#define TG3_MAX_MTU                     9000

/* These numbers seem to be hard coded in the NIC firmware somehow.
 * You can't change the ring sizes, but you can change where you place
 * them in the NIC onboard memory.
 */
#define TG3_RX_RING_SIZE                512
#define TG3_DEF_RX_RING_PENDING         200
#define TG3_RX_JUMBO_RING_SIZE          256
#define TG3_DEF_RX_JUMBO_RING_PENDING   100
#define TG3_RX_RCB_RING_SIZE            1024
#define TG3_TX_RING_SIZE                512
#define TG3_DEF_TX_RING_PENDING         (TG3_TX_RING_SIZE - 1)

#define TG3_RX_RING_BYTES       (sizeof(struct tg3_rx_buffer_desc) * \
                                 TG3_RX_RING_SIZE)
#define TG3_RX_JUMBO_RING_BYTES (sizeof(struct tg3_rx_buffer_desc) * \
                                 TG3_RX_JUMBO_RING_SIZE)
#define TG3_RX_RCB_RING_BYTES   (sizeof(struct tg3_rx_buffer_desc) * \
                                 TG3_RX_RCB_RING_SIZE)
#define TG3_TX_RING_BYTES       (sizeof(struct tg3_tx_buffer_desc) * \
                                 TG3_TX_RING_SIZE)
#define TX_RING_GAP(TP) \
        (TG3_TX_RING_SIZE - (TP)->tx_pending)
#define TX_BUFFS_AVAIL(TP)                                              \
        (((TP)->tx_cons <= (TP)->tx_prod) ?                             \
          (TP)->tx_cons + (TP)->tx_pending - (TP)->tx_prod :            \
          (TP)->tx_cons - (TP)->tx_prod - TX_RING_GAP(TP))
#define NEXT_TX(N)              (((N) + 1) & (TG3_TX_RING_SIZE - 1))

#define RX_PKT_BUF_SZ           (1536 + tp->rx_offset + 64)
#define RX_JUMBO_PKT_BUF_SZ     (9046 + tp->rx_offset + 64)

/* minimum number of free TX descriptors required to wake up TX process */
#define TG3_TX_WAKEUP_THRESH            (TG3_TX_RING_SIZE / 4)

//static int tg3_debug = -1;      /* -1 == use TG3_DEF_MSG_ENABLE as value */

typedef struct pci_dev_data DEVICE; /* shorthand!*/

/* Globals */
static DEVICE NETDEV;               /* We use a single net adapter */
static uint8_t eaddr[ETH_ADDR_LEN]; /* Store MAC Address */
static char rcv_buffer[1600];       /* Max Ethernet Packet */
struct tg3 TP;                      /* A broadcom chip */

uint16_t dma_lss;          /* The lss of our dma region */
uint16_t regs_lss;         /* The lss of our regs mapped region */
uint16_t tp_regs;          /* The lss of the tp_registers */
uint32_t *dma_abszero;     /* Start address of the dma buffer */
uint32_t desc_start = 0x80005000; /* Start of the ring descriptors*/
uint32_t PHYSADDR;

int
pci_find_capability(struct pci_dev_data *dev, int cap)
{
  uint16_t status;
  uint8_t pos, id;
  int ttl = 48;
  
  pciprobe_read_config_word(KR_PCI_PROBE_C,dev, PCI_STATUS, &status);
  if (!(status & PCI_STATUS_CAP_LIST))
    return 0;
  
  pciprobe_read_config_byte(KR_PCI_PROBE_C,dev, PCI_CAPABILITY_LIST, &pos);
  while (ttl-- && pos >= 0x40) {
    pos &= ~3;
    pciprobe_read_config_byte(KR_PCI_PROBE_C,dev, pos + PCI_CAP_LIST_ID, &id);
    if (id == 0xff)
      break;
    if (id == cap)
      return pos;
    pciprobe_read_config_byte(KR_PCI_PROBE_C,dev, 
			      pos + PCI_CAP_LIST_NEXT, &pos);
  }
  return 0;
}

/**
 * pci_save_state - save the PCI configuration space of a device 
 *                  before suspending
 * @dev: - PCI device that we're dealing with
 * @buffer: - buffer to hold config space context
 *
 * @buffer must be large enough to hold the entire PCI 2.2 config space 
 * (>= 64 bytes).
 */
int
pci_save_state(struct pci_dev_data *dev, uint32_t *buffer)
{
  int i;
  if (buffer) {
    /* XXX: 100% dword access ok here? */
    for (i = 0; i < 16; i++)
      pciprobe_read_config_dword(KR_PCI_PROBE_C,dev, i * 4,&buffer[i]);
  }
  return 0;
}

/** 
 * pci_restore_state - Restore the saved state of a PCI device
 * @dev: - PCI device that we're dealing with
 * @buffer: - saved PCI config space
 *
 */
int 
pci_restore_state(struct pci_dev_data *dev, uint32_t *buffer)
{
  int i;
  if (buffer) {
    for (i = 0; i < 16; i++)
      pciprobe_write_config_dword(KR_PCI_PROBE_C,dev,i * 4, buffer[i]);
  }
  return 0;
}

void 
writel(uint32_t val, uint32_t addr) 
{
  uint32_t *l = (uint32_t *)addr;
  l[0] = val;
}

uint32_t
readl(uint32_t addr) 
{
  uint32_t *l = (uint32_t *)addr;
  return l[0];
}

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
void
patch_addrspace()
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
  
  capros_Node_swapSlot(KR_ADDRSPC, next_slot, KR_REGS, KR_VOID);
  if (regs_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR:no room for local window "
             "keys for tg3 regs!");

  /* Finally, patch up the ProcAddrSpace register */
  capros_Process_swapAddrSpace(KR_SELF, KR_ADDRSPC, KR_VOID);
}

/* Generate address faults in the entire mapped region in order to
 * ensure entire address subspace is fabricated and populated with
 *  correct page keys. */
void
init_mapped_memory(uint32_t *base, uint32_t size)
{
  uint32_t u;
  
  for (u=0; u < (size / (sizeof(uint32_t))); u=u+EROS_PAGE_SIZE) {
    base[u] &= 0xffffffffu;
  }
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
  }else DEBUG_ALTIMA
	  kprintf(KR_OSTREAM, "lance:Constructing helper ... [SUCCESS]");
  
  /* Pass on which irq to wait on */
  memset(&msg,0,sizeof(Message));
  msg.snd_invKey = KR_HELPER_S;
  msg.snd_w1 = NETDEV.irq;
  CALL(&msg);
  DEBUG_ALTIMA kprintf(KR_OSTREAM,"TG3:sending IRQ no ... [SUCCESS]");
  
  return RC_OK;
}

void 
tg3_write_indirect_reg32(struct tg3 *tp, uint32_t off, uint32_t val)
{
  if ((tp->tg3_flags & TG3_FLAG_PCIX_TARGET_HWBUG) != 0) {
    pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
				TG3PCI_REG_BASE_ADDR, off);
    pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
				TG3PCI_REG_DATA, val);
  } else {
    writel(val, tp->regs + off);
    if ((tp->tg3_flags & TG3_FLAG_5701_REG_WRITE_BUG) != 0)
      readl(tp->regs + off);
  }
}

#define tw32(reg,val)           tg3_write_indirect_reg32(tp,(reg),(val))
#define tw32_mailbox(reg, val)  writel(((val) & 0xffffffff), tp->regs + (reg))
#define tw16(reg,val)           writew(((val) & 0xffff), tp->regs + (reg))
#define tw8(reg,val)            writeb(((val) & 0xff), tp->regs + (reg))
#define tr32(reg)               readl(tp->regs + (reg))
#define tr16(reg)               readw(tp->regs + (reg))
#define tr8(reg)                readb(tp->regs + (reg))

/* Chips other than 5700/5701 use the NVRAM for fetching info. */
static void 
tg3_nvram_init(struct tg3 *tp)
{
  tw32(GRC_EEPROM_ADDR,
       (EEPROM_ADDR_FSM_RESET |
	(EEPROM_DEFAULT_CLOCK_PERIOD <<
	 EEPROM_ADDR_CLKPERD_SHIFT)));
  
  /* XXX schedule_timeout() ... */
  capros_Sleep_sleep(KR_SLEEP,0.10);
  
  /* Enable seeprom accesses. */
  tw32(GRC_LOCAL_CTRL,
       tr32(GRC_LOCAL_CTRL) | GRC_LCLCTRL_AUTO_SEEPROM);
  tr32(GRC_LOCAL_CTRL);
  capros_Sleep_sleep(KR_SLEEP,0.100);
  
  if (GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5700 &&
      GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5701) {
    uint32_t nvcfg1 = tr32(NVRAM_CFG1);
    
    tp->tg3_flags |= TG3_FLAG_NVRAM;
    if (nvcfg1 & NVRAM_CFG1_FLASHIF_ENAB) {
      if (nvcfg1 & NVRAM_CFG1_BUFFERED_MODE)
	tp->tg3_flags |= TG3_FLAG_NVRAM_BUFFERED;
    } else {
      nvcfg1 &= ~NVRAM_CFG1_COMPAT_BYPASS;
      tw32(NVRAM_CFG1, nvcfg1);
    }
    
  } else {
    tp->tg3_flags &= ~(TG3_FLAG_NVRAM | TG3_FLAG_NVRAM_BUFFERED);
  }
}

int
tg3_nvram_read_using_eeprom(struct tg3 *tp,
			    uint32_t offset, uint32_t *val)
{
  uint32_t tmp;
  int i;

  if (offset > EEPROM_ADDR_ADDR_MASK ||
      (offset % 4) != 0)
    return -RC_EINVAL;

  tmp = tr32(GRC_EEPROM_ADDR) & ~(EEPROM_ADDR_ADDR_MASK |
				  EEPROM_ADDR_DEVID_MASK |
				  EEPROM_ADDR_READ);
  tw32(GRC_EEPROM_ADDR,
       tmp |
       (0 << EEPROM_ADDR_DEVID_SHIFT) |
       ((offset << EEPROM_ADDR_ADDR_SHIFT) &
	EEPROM_ADDR_ADDR_MASK) |
       EEPROM_ADDR_READ | EEPROM_ADDR_START);
  for (i = 0; i < 10000; i++) {
    tmp = tr32(GRC_EEPROM_ADDR);

    if (tmp & EEPROM_ADDR_COMPLETE)
      break;
    capros_Sleep_sleep(KR_SLEEP,0.100);
  }
  if (!(tmp & EEPROM_ADDR_COMPLETE))
    return -RC_EBUSY;

  *val = tr32(GRC_EEPROM_DATA);
  return 0;
}

int
tg3_nvram_read(struct tg3 *tp,
	       uint32_t offset, uint32_t *val)
{
  int i, saw_done_clear;

  if (!(tp->tg3_flags & TG3_FLAG_NVRAM))
    return tg3_nvram_read_using_eeprom(tp, offset, val);

  if (tp->tg3_flags & TG3_FLAG_NVRAM_BUFFERED)
    offset = ((offset / NVRAM_BUFFERED_PAGE_SIZE) <<
	      NVRAM_BUFFERED_PAGE_POS) +
      (offset % NVRAM_BUFFERED_PAGE_SIZE);

  if (offset > NVRAM_ADDR_MSK)
    return -RC_EINVAL;

  tw32(NVRAM_SWARB, SWARB_REQ_SET1);
  for (i = 0; i < 1000; i++) {
    if (tr32(NVRAM_SWARB) & SWARB_GNT1)
      break;
    capros_Sleep_sleep(KR_SLEEP,.20);
  }
  tw32(NVRAM_ADDR, offset);
  tw32(NVRAM_CMD,
       NVRAM_CMD_RD | NVRAM_CMD_GO |
       NVRAM_CMD_FIRST | NVRAM_CMD_LAST | NVRAM_CMD_DONE);

  /* Wait for done bit to clear then set again. */
  saw_done_clear = 0;
  for (i = 0; i < 1000; i++) {
    capros_Sleep_sleep(KR_SLEEP,.10);
    if (!saw_done_clear &&
	!(tr32(NVRAM_CMD) & NVRAM_CMD_DONE))
      saw_done_clear = 1;
    else if (saw_done_clear &&
	     (tr32(NVRAM_CMD) & NVRAM_CMD_DONE))
      break;
  }
  if (i >= 1000) {
    tw32(NVRAM_SWARB, SWARB_REQ_CLR1);
    return -RC_EBUSY;
  }
  *val = swab32(tr32(NVRAM_RDDATA));
  tw32(NVRAM_SWARB, 0x20);

  return 0;
}

void 
tg3_write_mem(struct tg3 *tp, uint32_t off, uint32_t val)
{
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
			      TG3PCI_MEM_WIN_BASE_ADDR, off);
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
			      TG3PCI_MEM_WIN_DATA, val);
  
  /* Always leave this as zero. */
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
			      TG3PCI_MEM_WIN_BASE_ADDR, 0);
}

void
tg3_read_mem(struct tg3 *tp, uint32_t off, uint32_t *val)
{
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
			      TG3PCI_MEM_WIN_BASE_ADDR, off);
  pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, 
			     TG3PCI_MEM_WIN_DATA, val);

  /* Always leave this as zero. */
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
			      TG3PCI_MEM_WIN_BASE_ADDR, 0);
}

void
tg3_disable_ints(struct tg3 *tp)
{
  tw32(TG3PCI_MISC_HOST_CTRL,
       (tp->misc_host_ctrl | MISC_HOST_CTRL_MASK_PCI_INT));
  tw32_mailbox(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW, 0x00000001);
  tr32(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW);
}

inline void 
tg3_cond_int(struct tg3 *tp)
{
  if (tp->hw_status->status & SD_STATUS_UPDATED)
    tw32(GRC_LOCAL_CTRL, tp->grc_local_ctrl | GRC_LCLCTRL_SETINT);
}

void
tg3_enable_ints(struct tg3 *tp)
{
  tw32(TG3PCI_MISC_HOST_CTRL,
       (tp->misc_host_ctrl & ~MISC_HOST_CTRL_MASK_PCI_INT));
  tw32_mailbox(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW, 0x00000000);
  tr32(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW);

  tg3_cond_int(tp);
}

void 
tg3_switch_clocks(struct tg3 *tp)
{
  if (tr32(TG3PCI_CLOCK_CTRL) & CLOCK_CTRL_44MHZ_CORE) {
    tw32(TG3PCI_CLOCK_CTRL,
	 (CLOCK_CTRL_44MHZ_CORE | CLOCK_CTRL_ALTCLK));
    tr32(TG3PCI_CLOCK_CTRL);
    capros_Sleep_sleep(KR_SLEEP,.40);
    tw32(TG3PCI_CLOCK_CTRL,
	 (CLOCK_CTRL_ALTCLK));
    tr32(TG3PCI_CLOCK_CTRL);
    capros_Sleep_sleep(KR_SLEEP,.40);
  }
  tw32(TG3PCI_CLOCK_CTRL, 0);
  tr32(TG3PCI_CLOCK_CTRL);
  capros_Sleep_sleep(KR_SLEEP,.40);
}


/* To stop a block, clear the enable bit and poll till it
 * clears.  tp->lock is held.
 */
static int 
tg3_stop_block(struct tg3 *tp, unsigned long ofs, uint32_t enable_bit)
{
  unsigned int i;
  uint32_t val;
  
  val = tr32(ofs);
  val &= ~enable_bit;
  tw32(ofs, val);
  tr32(ofs);
  
  for (i = 0; i < MAX_WAIT_CNT; i++) {
    capros_Sleep_sleep(KR_SLEEP,0.100);
    val = tr32(ofs);
    if ((val & enable_bit) == 0)
      break;
  }
  if (i == MAX_WAIT_CNT) {
    kprintf(KR_OSTREAM,"tg3_stop_block timed out, "
	    "ofs=%lx enable_bit=%x\n",
	    ofs, enable_bit);
    return -RC_ENODEV;
  }
  
  return 0;
}

#define PHY_BUSY_LOOPS  500
int
tg3_readphy(struct tg3 *tp, int reg, uint32_t *val)
{
  uint32_t frame_val;
  int loops, ret;

  if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
    tw32(MAC_MI_MODE,
	 (tp->mi_mode & ~MAC_MI_MODE_AUTO_POLL));
    tr32(MAC_MI_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);
  }

  *val = 0xffffffff;

  frame_val  = ((PHY_ADDR << MI_COM_PHY_ADDR_SHIFT) &
		MI_COM_PHY_ADDR_MASK);
  frame_val |= ((reg << MI_COM_REG_ADDR_SHIFT) &
		MI_COM_REG_ADDR_MASK);
  frame_val |= (MI_COM_CMD_READ | MI_COM_START);
        
  tw32(MAC_MI_COM, frame_val);
  tr32(MAC_MI_COM);

  loops = PHY_BUSY_LOOPS;
  while (loops-- > 0) {
    capros_Sleep_sleep(KR_SLEEP,.10);
    frame_val = tr32(MAC_MI_COM);

    if ((frame_val & MI_COM_BUSY) == 0) {
      capros_Sleep_sleep(KR_SLEEP,.05);
      frame_val = tr32(MAC_MI_COM);
      break;
    }
  }
  ret = -RC_EBUSY;

  if (loops > 0) {
    *val = frame_val & MI_COM_DATA_MASK;
    ret = 0;
  }
  
  if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
    tw32(MAC_MI_MODE, tp->mi_mode);
    tr32(MAC_MI_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);
  }

  return ret;
}

int
tg3_writephy(struct tg3 *tp, int reg, uint32_t val)
{
  uint32_t frame_val;
  int loops, ret;

  if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
    tw32(MAC_MI_MODE,
	 (tp->mi_mode & ~MAC_MI_MODE_AUTO_POLL));
    tr32(MAC_MI_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);
  }

  frame_val  = ((PHY_ADDR << MI_COM_PHY_ADDR_SHIFT) &
		MI_COM_PHY_ADDR_MASK);
  frame_val |= ((reg << MI_COM_REG_ADDR_SHIFT) &
		MI_COM_REG_ADDR_MASK);
  frame_val |= (val & MI_COM_DATA_MASK);
  frame_val |= (MI_COM_CMD_WRITE | MI_COM_START);
        
  tw32(MAC_MI_COM, frame_val);
  tr32(MAC_MI_COM);

  loops = PHY_BUSY_LOOPS;
  while (loops-- > 0) {
    capros_Sleep_sleep(KR_SLEEP,.10);
    frame_val = tr32(MAC_MI_COM);
    if ((frame_val & MI_COM_BUSY) == 0) {
      capros_Sleep_sleep(KR_SLEEP,.05);
      frame_val = tr32(MAC_MI_COM);
      break;
    }
  }
  ret = -RC_EBUSY;
  if (loops > 0)
    ret = 0;

  if ((tp->mi_mode & MAC_MI_MODE_AUTO_POLL) != 0) {
    tw32(MAC_MI_MODE, tp->mi_mode);
    tr32(MAC_MI_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);
  }

  return ret;
}

static int 
tg3_init_5401phy_dsp(struct tg3 *tp)
{
  int err;
  
  /* Turn off tap power management. */
  err  = tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c20);
  
  err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x0012);
  err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x1804);
  
  err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x0013);
  err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x1204);
  
  err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x8006);
  err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0132);
  
  err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x8006);
  err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0232);
  
  err |= tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x201f);
  err |= tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x0a20);
  
  capros_Sleep_sleep(KR_SLEEP,.40);
  
  return err;
}

/* tp->lock is held. */
static void 
tg3_set_bdinfo(struct tg3 *tp, uint32_t bdinfo_addr,
	       uint32_t mapping, uint32_t maxlen_flags,
	       uint32_t nic_addr)
{
  tg3_write_mem(tp,
		(bdinfo_addr +
		 TG3_BDINFO_HOST_ADDR +
		 TG3_64BIT_REG_HIGH),
		((uint64_t) mapping >> 32));
  tg3_write_mem(tp,
		(bdinfo_addr +
		 TG3_BDINFO_HOST_ADDR +
		 TG3_64BIT_REG_LOW),
		((uint64_t) mapping & 0xffffffff));
  tg3_write_mem(tp,(bdinfo_addr +
		    TG3_BDINFO_MAXLEN_FLAGS),
		maxlen_flags);
  tg3_write_mem(tp,
		(bdinfo_addr +
		 TG3_BDINFO_NIC_ADDR),
		nic_addr);
}

/* This will reset the tigon3 PHY if there is no valid
 * link unless the FORCE argument is non-zero.
 */
static int 
tg3_phy_reset(struct tg3 *tp, int force)
{
  uint32_t phy_status, phy_control;
  int err, limit;
  
  err  = tg3_readphy(tp, MII_BMSR, &phy_status);
  err |= tg3_readphy(tp, MII_BMSR, &phy_status);
  if (err != 0)
    return -RC_EBUSY;
  
  /* If we have link, and not forcing a reset, then nothing
   * to do. */
  if ((phy_status & BMSR_LSTATUS) != 0 && (force == 0))
    return 0;
  
  /* OK, reset it, and poll the BMCR_RESET bit until it
   * clears or we time out. */
  phy_control = BMCR_RESET;
  err = tg3_writephy(tp, MII_BMCR, phy_control);
  if (err != 0)
    return -RC_EBUSY;
  
  limit = 5000;
  while (limit--) {
    err = tg3_readphy(tp, MII_BMCR, &phy_control);
    if (err != 0)
      return -RC_EBUSY;
    
    if ((phy_control & BMCR_RESET) == 0) {
      capros_Sleep_sleep(KR_SLEEP,0.40);
      return 0;
    }
    capros_Sleep_sleep(KR_SLEEP,0.10);
  }
  
  return -RC_EBUSY;
}

void
tg3_init_link_config(struct tg3 *tp)
{
  tp->link_config.advertising =
    (ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
     ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
     ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full |
     ADVERTISED_Autoneg | ADVERTISED_MII);
  tp->link_config.speed = SPEED_INVALID;
  tp->link_config.duplex = DUPLEX_INVALID;
  tp->link_config.autoneg = AUTONEG_ENABLE;
  //netif_carrier_off(tp->dev);
  tp->link_config.active_speed = SPEED_INVALID;
  tp->link_config.active_duplex = DUPLEX_INVALID;
  tp->link_config.phy_is_low_power = 0;
  tp->link_config.orig_speed = SPEED_INVALID;
  tp->link_config.orig_duplex = DUPLEX_INVALID;
  tp->link_config.orig_autoneg = AUTONEG_INVALID;
}

void
tg3_init_bufmgr_config(struct tg3 *tp)
{
  tp->bufmgr_config.mbuf_read_dma_low_water =
    DEFAULT_MB_RDMA_LOW_WATER;
  tp->bufmgr_config.mbuf_mac_rx_low_water =
    DEFAULT_MB_MACRX_LOW_WATER;
  tp->bufmgr_config.mbuf_high_water =
    DEFAULT_MB_HIGH_WATER;
  
  tp->bufmgr_config.mbuf_read_dma_low_water_jumbo =
    DEFAULT_MB_RDMA_LOW_WATER_JUMBO;
  tp->bufmgr_config.mbuf_mac_rx_low_water_jumbo =
    DEFAULT_MB_MACRX_LOW_WATER_JUMBO;
  tp->bufmgr_config.mbuf_high_water_jumbo =
    DEFAULT_MB_HIGH_WATER_JUMBO;
  
  tp->bufmgr_config.dma_low_water = DEFAULT_DMA_LOW_WATER;
  tp->bufmgr_config.dma_high_water = DEFAULT_DMA_HIGH_WATER;
}

static void 
tg3_link_report(struct tg3 *tp)
{
  if (!netif_carrier_ok(tp->dev)) {
    kprintf(KR_OSTREAM,"Link is down.\n");
  } else {
    kprintf(KR_OSTREAM,"Link is up at %d Mbps, %s duplex.\n",
	    (tp->link_config.active_speed == SPEED_1000 ?
	     1000 :
	     (tp->link_config.active_speed == SPEED_100 ?
	      100 : 10)),
	    (tp->link_config.active_duplex == DUPLEX_FULL ?
	     "full" : "half"));
    
    kprintf(KR_OSTREAM,"Flow control is %s for TX and "
	    "%s for RX.\n",
	    (tp->tg3_flags & TG3_FLAG_TX_PAUSE) ? "on" : "off",
	    (tp->tg3_flags & TG3_FLAG_RX_PAUSE) ? "on" : "off");
  }
}

static void 
tg3_setup_flow_control(struct tg3 *tp, uint32_t local_adv, 
		       uint32_t remote_adv)
{
  uint32_t new_tg3_flags = 0;
  
  if (local_adv & ADVERTISE_PAUSE_CAP) {
    if (local_adv & ADVERTISE_PAUSE_ASYM) {
      if (remote_adv & LPA_PAUSE_CAP)
	new_tg3_flags |=
	  (TG3_FLAG_RX_PAUSE |
	   TG3_FLAG_TX_PAUSE);
      else if (remote_adv & LPA_PAUSE_ASYM)
	new_tg3_flags |=
	  (TG3_FLAG_RX_PAUSE);
    } else {
      if (remote_adv & LPA_PAUSE_CAP)
	new_tg3_flags |=
	  (TG3_FLAG_RX_PAUSE |
	   TG3_FLAG_TX_PAUSE);
    }
  } else if (local_adv & ADVERTISE_PAUSE_ASYM) {
    if ((remote_adv & LPA_PAUSE_CAP) &&
	(remote_adv & LPA_PAUSE_ASYM))
      new_tg3_flags |= TG3_FLAG_TX_PAUSE;
  }
  
  tp->tg3_flags &= ~(TG3_FLAG_RX_PAUSE | TG3_FLAG_TX_PAUSE);
  tp->tg3_flags |= new_tg3_flags;
  
  if (new_tg3_flags & TG3_FLAG_RX_PAUSE)
    tp->rx_mode |= RX_MODE_FLOW_CTRL_ENABLE;
  else
    tp->rx_mode &= ~RX_MODE_FLOW_CTRL_ENABLE;
  
  if (new_tg3_flags & TG3_FLAG_TX_PAUSE)
    tp->tx_mode |= TX_MODE_FLOW_CTRL_ENABLE;
  else
    tp->tx_mode &= ~TX_MODE_FLOW_CTRL_ENABLE;
}

static void 
tg3_aux_stat_to_speed_duplex(struct tg3 *tp, uint32_t val, 
			     uint16_t *speed, uint8_t *duplex)
{
  switch (val & MII_TG3_AUX_STAT_SPDMASK) {
  case MII_TG3_AUX_STAT_10HALF:
    *speed = SPEED_10;
    *duplex = DUPLEX_HALF;
    break;

  case MII_TG3_AUX_STAT_10FULL:
    *speed = SPEED_10;
    *duplex = DUPLEX_FULL;
    break;

  case MII_TG3_AUX_STAT_100HALF:
    *speed = SPEED_100;
    *duplex = DUPLEX_HALF;
    break;

  case MII_TG3_AUX_STAT_100FULL:
    *speed = SPEED_100;
    *duplex = DUPLEX_FULL;
    break;

  case MII_TG3_AUX_STAT_1000HALF:
    *speed = SPEED_1000;
    *duplex = DUPLEX_HALF;
    break;

  case MII_TG3_AUX_STAT_1000FULL:
    *speed = SPEED_1000;
    *duplex = DUPLEX_FULL;
    break;

  default:
    *speed = SPEED_INVALID;
    *duplex = DUPLEX_INVALID;
    break;
  };
}

static int tg3_phy_copper_begin(struct tg3 *tp, int wait_for_link)
{
  uint32_t new_adv;
  int i;

  if (tp->link_config.phy_is_low_power) {
    /* Entering low power mode.  Disable gigabit and
     * 100baseT advertisements.
     */
    tg3_writephy(tp, MII_TG3_CTRL, 0);

    new_adv = (ADVERTISE_10HALF | ADVERTISE_10FULL |
	       ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP);
    if (tp->tg3_flags & TG3_FLAG_WOL_SPEED_100MB)
      new_adv |= (ADVERTISE_100HALF | ADVERTISE_100FULL);

    tg3_writephy(tp, MII_ADVERTISE, new_adv);
  } else if (tp->link_config.speed == SPEED_INVALID) {
    tp->link_config.advertising =
      (ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
       ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full |
       ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full |
       ADVERTISED_Autoneg | ADVERTISED_MII);

    if (tp->tg3_flags & TG3_FLAG_10_100_ONLY)
      tp->link_config.advertising &=
	~(ADVERTISED_1000baseT_Half |
	  ADVERTISED_1000baseT_Full);

    new_adv = (ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP);
    if (tp->link_config.advertising & ADVERTISED_10baseT_Half)
      new_adv |= ADVERTISE_10HALF;
    if (tp->link_config.advertising & ADVERTISED_10baseT_Full)
      new_adv |= ADVERTISE_10FULL;
    if (tp->link_config.advertising & ADVERTISED_100baseT_Half)
      new_adv |= ADVERTISE_100HALF;
    if (tp->link_config.advertising & ADVERTISED_100baseT_Full)
      new_adv |= ADVERTISE_100FULL;
    tg3_writephy(tp, MII_ADVERTISE, new_adv);

    if (tp->link_config.advertising &
	(ADVERTISED_1000baseT_Half | ADVERTISED_1000baseT_Full)) {
      new_adv = 0;
      if (tp->link_config.advertising & ADVERTISED_1000baseT_Half)
	new_adv |= MII_TG3_CTRL_ADV_1000_HALF;
      if (tp->link_config.advertising & ADVERTISED_1000baseT_Full)
	new_adv |= MII_TG3_CTRL_ADV_1000_FULL;
      if (!(tp->tg3_flags & TG3_FLAG_10_100_ONLY) &&
	  (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
	   tp->pci_chip_rev_id == CHIPREV_ID_5701_B0))
	new_adv |= (MII_TG3_CTRL_AS_MASTER |
		    MII_TG3_CTRL_ENABLE_AS_MASTER);
      tg3_writephy(tp, MII_TG3_CTRL, new_adv);
    } else {
      tg3_writephy(tp, MII_TG3_CTRL, 0);
    }
  } else {
    /* Asking for a specific link mode. */
    if (tp->link_config.speed == SPEED_1000) {
      new_adv = ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP;
      tg3_writephy(tp, MII_ADVERTISE, new_adv);

      if (tp->link_config.duplex == DUPLEX_FULL)
	new_adv = MII_TG3_CTRL_ADV_1000_FULL;
      else
	new_adv = MII_TG3_CTRL_ADV_1000_HALF;
      if (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
	  tp->pci_chip_rev_id == CHIPREV_ID_5701_B0)
	new_adv |= (MII_TG3_CTRL_AS_MASTER |
		    MII_TG3_CTRL_ENABLE_AS_MASTER);
      tg3_writephy(tp, MII_TG3_CTRL, new_adv);
    } else {
      tg3_writephy(tp, MII_TG3_CTRL, 0);

      new_adv = ADVERTISE_CSMA | ADVERTISE_PAUSE_CAP;
      if (tp->link_config.speed == SPEED_100) {
	if (tp->link_config.duplex == DUPLEX_FULL)
	  new_adv |= ADVERTISE_100FULL;
	else
	  new_adv |= ADVERTISE_100HALF;
      } else {
	if (tp->link_config.duplex == DUPLEX_FULL)
	  new_adv |= ADVERTISE_10FULL;
	else
	  new_adv |= ADVERTISE_10HALF;
      }
      tg3_writephy(tp, MII_ADVERTISE, new_adv);
    }
  }

  if (tp->link_config.autoneg == AUTONEG_DISABLE &&
      tp->link_config.speed != SPEED_INVALID) {
    uint32_t bmcr, orig_bmcr;

    tp->link_config.active_speed = tp->link_config.speed;
    tp->link_config.active_duplex = tp->link_config.duplex;

    bmcr = 0;
    switch (tp->link_config.speed) {
    default:
    case SPEED_10:
      break;

    case SPEED_100:
      bmcr |= BMCR_SPEED100;
      break;

    case SPEED_1000:
      bmcr |= TG3_BMCR_SPEED1000;
      break;
    };

    if (tp->link_config.duplex == DUPLEX_FULL)
      bmcr |= BMCR_FULLDPLX;

    tg3_readphy(tp, MII_BMCR, &orig_bmcr);
    if (bmcr != orig_bmcr) {
      tg3_writephy(tp, MII_BMCR, BMCR_LOOPBACK);
      for (i = 0; i < 15000; i++) {
	uint32_t tmp;

	capros_Sleep_sleep(KR_SLEEP,.10);
	tg3_readphy(tp, MII_BMSR, &tmp);
	tg3_readphy(tp, MII_BMSR, &tmp);
	if (!(tmp & BMSR_LSTATUS)) {
	  capros_Sleep_sleep(KR_SLEEP,.40);
	  break;
	}
      }
      tg3_writephy(tp, MII_BMCR, bmcr);
      capros_Sleep_sleep(KR_SLEEP,.40);
    }
  } else {
    tg3_writephy(tp, MII_BMCR,
		 BMCR_ANENABLE | BMCR_ANRESTART);
  }

  if (wait_for_link) {
    tp->link_config.active_speed = SPEED_INVALID;
    tp->link_config.active_duplex = DUPLEX_INVALID;
    for (i = 0; i < 300000; i++) {
      uint32_t tmp;

      capros_Sleep_sleep(KR_SLEEP,.10);
      tg3_readphy(tp, MII_BMSR, &tmp);
      tg3_readphy(tp, MII_BMSR, &tmp);
      if (!(tmp & BMSR_LSTATUS))
	continue;

      tg3_readphy(tp, MII_TG3_AUX_STAT, &tmp);
      tg3_aux_stat_to_speed_duplex(tp, tmp,
				   &tp->link_config.active_speed,
				   &tp->link_config.active_duplex);
    }
    if (tp->link_config.active_speed == SPEED_INVALID)
      return -RC_EINVAL;
  }

  return 0;
}

static int 
tg3_setup_copper_phy(struct tg3 *tp)
{
  int current_link_up;
  uint32_t bmsr, dummy;
  uint16_t current_speed;
  uint8_t current_duplex;
  int i, err;

  tw32(MAC_STATUS,
       (MAC_STATUS_SYNC_CHANGED |
	MAC_STATUS_CFG_CHANGED));
  tr32(MAC_STATUS);
  capros_Sleep_sleep(KR_SLEEP,.40);

  tp->mi_mode = MAC_MI_MODE_BASE;
  tw32(MAC_MI_MODE, tp->mi_mode);
  tr32(MAC_MI_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x02);

  /* Some third-party PHYs need to be reset on link going
   * down.
   *
   * XXX 5705 note: This workaround also applies to 5705_a0
   */
  if ((GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
       GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704) &&
      netif_carrier_ok(tp->dev)) {
    tg3_readphy(tp, MII_BMSR, &bmsr);
    tg3_readphy(tp, MII_BMSR, &bmsr);
    if (!(bmsr & BMSR_LSTATUS))
      tg3_phy_reset(tp, 1);
  }

  if ((tp->phy_id & PHY_ID_MASK) == PHY_ID_BCM5401) {
    tg3_readphy(tp, MII_BMSR, &bmsr);
    tg3_readphy(tp, MII_BMSR, &bmsr);

    if (!(tp->tg3_flags & TG3_FLAG_INIT_COMPLETE))
      bmsr = 0;

    if (!(bmsr & BMSR_LSTATUS)) {
      err = tg3_init_5401phy_dsp(tp);
      if (err)
	return err;

      tg3_readphy(tp, MII_BMSR, &bmsr);
      for (i = 0; i < 1000; i++) {
	capros_Sleep_sleep(KR_SLEEP,.10);
	tg3_readphy(tp, MII_BMSR, &bmsr);
	if (bmsr & BMSR_LSTATUS) {
	  capros_Sleep_sleep(KR_SLEEP,.40);
	  break;
	}
      }

      if ((tp->phy_id & PHY_ID_REV_MASK) == PHY_REV_BCM5401_B0 &&
	  !(bmsr & BMSR_LSTATUS) &&
	  tp->link_config.active_speed == SPEED_1000) {
	err = tg3_phy_reset(tp, 1);
	if (!err)
	  err = tg3_init_5401phy_dsp(tp);
	if (err)
	  return err;
      }
    }
  } else if (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
	     tp->pci_chip_rev_id == CHIPREV_ID_5701_B0) {
    /* 5701 {A0,B0} CRC bug workaround */
    tg3_writephy(tp, 0x15, 0x0a75);
    tg3_writephy(tp, 0x1c, 0x8c68);
    tg3_writephy(tp, 0x1c, 0x8d68);
    tg3_writephy(tp, 0x1c, 0x8c68);
  }

  /* Clear pending interrupts... */
  tg3_readphy(tp, MII_TG3_ISTAT, &dummy);
  tg3_readphy(tp, MII_TG3_ISTAT, &dummy);

  if (tp->tg3_flags & TG3_FLAG_USE_MI_INTERRUPT)
    tg3_writephy(tp, MII_TG3_IMASK, ~MII_TG3_INT_LINKCHG);
  else
    tg3_writephy(tp, MII_TG3_IMASK, ~0);

  if (tp->led_mode == led_mode_three_link)
    tg3_writephy(tp, MII_TG3_EXT_CTRL,
		 MII_TG3_EXT_CTRL_LNK3_LED_MODE);
  else
    tg3_writephy(tp, MII_TG3_EXT_CTRL, 0);

  current_link_up = 0;
  current_speed = SPEED_INVALID;
  current_duplex = DUPLEX_INVALID;

  tg3_readphy(tp, MII_BMSR, &bmsr);
  tg3_readphy(tp, MII_BMSR, &bmsr);

  if (bmsr & BMSR_LSTATUS) {
    uint32_t aux_stat, bmcr;

    tg3_readphy(tp, MII_TG3_AUX_STAT, &aux_stat);
    for (i = 0; i < 2000; i++) {
      capros_Sleep_sleep(KR_SLEEP,.10);
      tg3_readphy(tp, MII_TG3_AUX_STAT, &aux_stat);
      if (aux_stat)
	break;
    }

    tg3_aux_stat_to_speed_duplex(tp, aux_stat,
				 &current_speed,
				 &current_duplex);
    tg3_readphy(tp, MII_BMCR, &bmcr);
    tg3_readphy(tp, MII_BMCR, &bmcr);
    if (tp->link_config.autoneg == AUTONEG_ENABLE) {
      if (bmcr & BMCR_ANENABLE) {
	uint32_t gig_ctrl;

	current_link_up = 1;

	/* Force autoneg restart if we are exiting
	 * low power mode.
	 */
	tg3_readphy(tp, MII_TG3_CTRL, &gig_ctrl);
	if (!(gig_ctrl & (MII_TG3_CTRL_ADV_1000_HALF |
			  MII_TG3_CTRL_ADV_1000_FULL))) {
	  current_link_up = 0;
	}
      } else {
	current_link_up = 0;
      }
    } else {
      if (!(bmcr & BMCR_ANENABLE) &&
	  tp->link_config.speed == current_speed &&
	  tp->link_config.duplex == current_duplex) {
	current_link_up = 1;
      } else {
	current_link_up = 0;
      }
    }

    tp->link_config.active_speed = current_speed;
    tp->link_config.active_duplex = current_duplex;
  }

  if (current_link_up == 1 &&
      (tp->link_config.active_duplex == DUPLEX_FULL) &&
      (tp->link_config.autoneg == AUTONEG_ENABLE)) {
    uint32_t local_adv, remote_adv;

    tg3_readphy(tp, MII_ADVERTISE, &local_adv);
    local_adv &= (ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM);

    tg3_readphy(tp, MII_LPA, &remote_adv);
    remote_adv &= (LPA_PAUSE_CAP | LPA_PAUSE_ASYM);

    /* If we are not advertising full pause capability,
     * something is wrong.  Bring the link down and reconfigure.
     */
    if (local_adv != ADVERTISE_PAUSE_CAP) {
      current_link_up = 0;
    } else {
      tg3_setup_flow_control(tp, local_adv, remote_adv);
    }
  }

  if (current_link_up == 0) {
    uint32_t tmp;

    tg3_phy_copper_begin(tp, 0);

    tg3_readphy(tp, MII_BMSR, &tmp);
    tg3_readphy(tp, MII_BMSR, &tmp);
    if (tmp & BMSR_LSTATUS)
      current_link_up = 1;
  }

  tp->mac_mode &= ~MAC_MODE_PORT_MODE_MASK;
  if (current_link_up == 1) {
    if (tp->link_config.active_speed == SPEED_100 ||
	tp->link_config.active_speed == SPEED_10)
      tp->mac_mode |= MAC_MODE_PORT_MODE_MII;
    else
      tp->mac_mode |= MAC_MODE_PORT_MODE_GMII;
  } else
    tp->mac_mode |= MAC_MODE_PORT_MODE_GMII;

  tp->mac_mode &= ~MAC_MODE_HALF_DUPLEX;
  if (tp->link_config.active_duplex == DUPLEX_HALF)
    tp->mac_mode |= MAC_MODE_HALF_DUPLEX;

  tp->mac_mode &= ~MAC_MODE_LINK_POLARITY;
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700) {
    if ((tp->led_mode == led_mode_link10) ||
	(current_link_up == 1 &&
	 tp->link_config.active_speed == SPEED_10))
      tp->mac_mode |= MAC_MODE_LINK_POLARITY;
  } else {
    if (current_link_up == 1)
      tp->mac_mode |= MAC_MODE_LINK_POLARITY;
    tw32(MAC_LED_CTRL, LED_CTRL_PHY_MODE_1);
  }

  /* ??? Without this setting Netgear GA302T PHY does not
   * ??? send/receive packets...
   */
  if ((tp->phy_id & PHY_ID_MASK) == PHY_ID_BCM5411 &&
      tp->pci_chip_rev_id == CHIPREV_ID_5700_ALTIMA) {
    tp->mi_mode |= MAC_MI_MODE_AUTO_POLL;
    tw32(MAC_MI_MODE, tp->mi_mode);
    tr32(MAC_MI_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);
  }

  tw32(MAC_MODE, tp->mac_mode);
  tr32(MAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  if (tp->tg3_flags &
      (TG3_FLAG_USE_LINKCHG_REG |
       TG3_FLAG_POLL_SERDES)) {
    /* Polled via timer. */
    tw32(MAC_EVENT, 0);
  } else {
    tw32(MAC_EVENT, MAC_EVENT_LNKSTATE_CHANGED);
  }
  tr32(MAC_EVENT);
  capros_Sleep_sleep(KR_SLEEP,.40);

  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 &&
      current_link_up == 1 &&
      tp->link_config.active_speed == SPEED_1000 &&
      ((tp->tg3_flags & TG3_FLAG_PCIX_MODE) ||
       (tp->tg3_flags & TG3_FLAG_PCI_HIGH_SPEED))) {
    capros_Sleep_sleep(KR_SLEEP,0.120);
    tw32(MAC_STATUS,
	 (MAC_STATUS_SYNC_CHANGED |
	  MAC_STATUS_CFG_CHANGED));
    tr32(MAC_STATUS);
    capros_Sleep_sleep(KR_SLEEP,.40);
    tg3_write_mem(tp,
		  NIC_SRAM_FIRMWARE_MBOX,
		  NIC_SRAM_FIRMWARE_MBOX_MAGIC2);
  }

#if 0
  if (current_link_up != netif_carrier_ok(tp->dev)) {
    if (current_link_up)
      netif_carrier_on(tp->dev);
    else
      netif_carrier_off(tp->dev);
  }
#endif

  tg3_link_report(tp);

  return 0;
}

struct tg3_fiber_aneginfo {
  int state;
#define ANEG_STATE_UNKNOWN		0
#define ANEG_STATE_AN_ENABLE		1
#define ANEG_STATE_RESTART_INIT		2
#define ANEG_STATE_RESTART		3
#define ANEG_STATE_DISABLE_LINK_OK	4
#define ANEG_STATE_ABILITY_DETECT_INIT	5
#define ANEG_STATE_ABILITY_DETECT	6
#define ANEG_STATE_ACK_DETECT_INIT	7
#define ANEG_STATE_ACK_DETECT		8
#define ANEG_STATE_COMPLETE_ACK_INIT	9
#define ANEG_STATE_COMPLETE_ACK		10
#define ANEG_STATE_IDLE_DETECT_INIT	11
#define ANEG_STATE_IDLE_DETECT		12
#define ANEG_STATE_LINK_OK		13
#define ANEG_STATE_NEXT_PAGE_WAIT_INIT	14
#define ANEG_STATE_NEXT_PAGE_WAIT	15

  uint32_t flags;
#define MR_AN_ENABLE		0x00000001
#define MR_RESTART_AN		0x00000002
#define MR_AN_COMPLETE		0x00000004
#define MR_PAGE_RX		0x00000008
#define MR_NP_LOADED		0x00000010
#define MR_TOGGLE_TX		0x00000020
#define MR_LP_ADV_FULL_DUPLEX	0x00000040
#define MR_LP_ADV_HALF_DUPLEX	0x00000080
#define MR_LP_ADV_SYM_PAUSE	0x00000100
#define MR_LP_ADV_ASYM_PAUSE	0x00000200
#define MR_LP_ADV_REMOTE_FAULT1	0x00000400
#define MR_LP_ADV_REMOTE_FAULT2	0x00000800
#define MR_LP_ADV_NEXT_PAGE	0x00001000
#define MR_TOGGLE_RX		0x00002000
#define MR_NP_RX		0x00004000

#define MR_LINK_OK		0x80000000

  unsigned long link_time, cur_time;

  uint32_t ability_match_cfg;
  int ability_match_count;

  char ability_match, idle_match, ack_match;

  uint32_t txconfig, rxconfig;
#define ANEG_CFG_NP		0x00000080
#define ANEG_CFG_ACK		0x00000040
#define ANEG_CFG_RF2		0x00000020
#define ANEG_CFG_RF1		0x00000010
#define ANEG_CFG_PS2		0x00000001
#define ANEG_CFG_PS1		0x00008000
#define ANEG_CFG_HD		0x00004000
#define ANEG_CFG_FD		0x00002000
#define ANEG_CFG_INVAL		0x00001f06

};
#define ANEG_OK		0
#define ANEG_DONE	1
#define ANEG_TIMER_ENAB	2
#define ANEG_FAILED	-1

#define ANEG_STATE_SETTLE_TIME	10000

static int 
tg3_fiber_aneg_smachine(struct tg3 *tp,
			struct tg3_fiber_aneginfo *ap)
{
  unsigned long delta;
  uint32_t rx_cfg_reg;
  int ret;

  if (ap->state == ANEG_STATE_UNKNOWN) {
    ap->rxconfig = 0;
    ap->link_time = 0;
    ap->cur_time = 0;
    ap->ability_match_cfg = 0;
    ap->ability_match_count = 0;
    ap->ability_match = 0;
    ap->idle_match = 0;
    ap->ack_match = 0;
  }
  ap->cur_time++;

  if (tr32(MAC_STATUS) & MAC_STATUS_RCVD_CFG) {
    rx_cfg_reg = tr32(MAC_RX_AUTO_NEG);

    if (rx_cfg_reg != ap->ability_match_cfg) {
      ap->ability_match_cfg = rx_cfg_reg;
      ap->ability_match = 0;
      ap->ability_match_count = 0;
    } else {
      if (++ap->ability_match_count > 1) {
	ap->ability_match = 1;
	ap->ability_match_cfg = rx_cfg_reg;
      }
    }
    if (rx_cfg_reg & ANEG_CFG_ACK)
      ap->ack_match = 1;
    else
      ap->ack_match = 0;

    ap->idle_match = 0;
  } else {
    ap->idle_match = 1;
    ap->ability_match_cfg = 0;
    ap->ability_match_count = 0;
    ap->ability_match = 0;
    ap->ack_match = 0;

    rx_cfg_reg = 0;
  }

  ap->rxconfig = rx_cfg_reg;
  ret = ANEG_OK;

  switch(ap->state) {
  case ANEG_STATE_UNKNOWN:
    if (ap->flags & (MR_AN_ENABLE | MR_RESTART_AN))
      ap->state = ANEG_STATE_AN_ENABLE;

    /* fallthru */
  case ANEG_STATE_AN_ENABLE:
    ap->flags &= ~(MR_AN_COMPLETE | MR_PAGE_RX);
    if (ap->flags & MR_AN_ENABLE) {
      ap->link_time = 0;
      ap->cur_time = 0;
      ap->ability_match_cfg = 0;
      ap->ability_match_count = 0;
      ap->ability_match = 0;
      ap->idle_match = 0;
      ap->ack_match = 0;

      ap->state = ANEG_STATE_RESTART_INIT;
    } else {
      ap->state = ANEG_STATE_DISABLE_LINK_OK;
    }
    break;

  case ANEG_STATE_RESTART_INIT:
    ap->link_time = ap->cur_time;
    ap->flags &= ~(MR_NP_LOADED);
    ap->txconfig = 0;
    tw32(MAC_TX_AUTO_NEG, 0);
    tp->mac_mode |= MAC_MODE_SEND_CONFIGS;
    tw32(MAC_MODE, tp->mac_mode);
    tr32(MAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);

    ret = ANEG_TIMER_ENAB;
    ap->state = ANEG_STATE_RESTART;

    /* fallthru */
  case ANEG_STATE_RESTART:
    delta = ap->cur_time - ap->link_time;
    if (delta > ANEG_STATE_SETTLE_TIME) {
      ap->state = ANEG_STATE_ABILITY_DETECT_INIT;
    } else {
      ret = ANEG_TIMER_ENAB;
    }
    break;

  case ANEG_STATE_DISABLE_LINK_OK:
    ret = ANEG_DONE;
    break;

  case ANEG_STATE_ABILITY_DETECT_INIT:
    ap->flags &= ~(MR_TOGGLE_TX);
    ap->txconfig = (ANEG_CFG_FD | ANEG_CFG_PS1);
    tw32(MAC_TX_AUTO_NEG, ap->txconfig);
    tp->mac_mode |= MAC_MODE_SEND_CONFIGS;
    tw32(MAC_MODE, tp->mac_mode);
    tr32(MAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);

    ap->state = ANEG_STATE_ABILITY_DETECT;
    break;

  case ANEG_STATE_ABILITY_DETECT:
    if (ap->ability_match != 0 && ap->rxconfig != 0) {
      ap->state = ANEG_STATE_ACK_DETECT_INIT;
    }
    break;

  case ANEG_STATE_ACK_DETECT_INIT:
    ap->txconfig |= ANEG_CFG_ACK;
    tw32(MAC_TX_AUTO_NEG, ap->txconfig);
    tp->mac_mode |= MAC_MODE_SEND_CONFIGS;
    tw32(MAC_MODE, tp->mac_mode);
    tr32(MAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);

    ap->state = ANEG_STATE_ACK_DETECT;

    /* fallthru */
  case ANEG_STATE_ACK_DETECT:
    if (ap->ack_match != 0) {
      if ((ap->rxconfig & ~ANEG_CFG_ACK) ==
	  (ap->ability_match_cfg & ~ANEG_CFG_ACK)) {
	ap->state = ANEG_STATE_COMPLETE_ACK_INIT;
      } else {
	ap->state = ANEG_STATE_AN_ENABLE;
      }
    } else if (ap->ability_match != 0 &&
	       ap->rxconfig == 0) {
      ap->state = ANEG_STATE_AN_ENABLE;
    }
    break;

  case ANEG_STATE_COMPLETE_ACK_INIT:
    if (ap->rxconfig & ANEG_CFG_INVAL) {
      ret = ANEG_FAILED;
      break;
    }
    ap->flags &= ~(MR_LP_ADV_FULL_DUPLEX |
		   MR_LP_ADV_HALF_DUPLEX |
		   MR_LP_ADV_SYM_PAUSE |
		   MR_LP_ADV_ASYM_PAUSE |
		   MR_LP_ADV_REMOTE_FAULT1 |
		   MR_LP_ADV_REMOTE_FAULT2 |
		   MR_LP_ADV_NEXT_PAGE |
		   MR_TOGGLE_RX |
		   MR_NP_RX);
    if (ap->rxconfig & ANEG_CFG_FD)
      ap->flags |= MR_LP_ADV_FULL_DUPLEX;
    if (ap->rxconfig & ANEG_CFG_HD)
      ap->flags |= MR_LP_ADV_HALF_DUPLEX;
    if (ap->rxconfig & ANEG_CFG_PS1)
      ap->flags |= MR_LP_ADV_SYM_PAUSE;
    if (ap->rxconfig & ANEG_CFG_PS2)
      ap->flags |= MR_LP_ADV_ASYM_PAUSE;
    if (ap->rxconfig & ANEG_CFG_RF1)
      ap->flags |= MR_LP_ADV_REMOTE_FAULT1;
    if (ap->rxconfig & ANEG_CFG_RF2)
      ap->flags |= MR_LP_ADV_REMOTE_FAULT2;
    if (ap->rxconfig & ANEG_CFG_NP)
      ap->flags |= MR_LP_ADV_NEXT_PAGE;

    ap->link_time = ap->cur_time;

    ap->flags ^= (MR_TOGGLE_TX);
    if (ap->rxconfig & 0x0008)
      ap->flags |= MR_TOGGLE_RX;
    if (ap->rxconfig & ANEG_CFG_NP)
      ap->flags |= MR_NP_RX;
    ap->flags |= MR_PAGE_RX;

    ap->state = ANEG_STATE_COMPLETE_ACK;
    ret = ANEG_TIMER_ENAB;
    break;

  case ANEG_STATE_COMPLETE_ACK:
    if (ap->ability_match != 0 &&
	ap->rxconfig == 0) {
      ap->state = ANEG_STATE_AN_ENABLE;
      break;
    }
    delta = ap->cur_time - ap->link_time;
    if (delta > ANEG_STATE_SETTLE_TIME) {
      if (!(ap->flags & (MR_LP_ADV_NEXT_PAGE))) {
	ap->state = ANEG_STATE_IDLE_DETECT_INIT;
      } else {
	if ((ap->txconfig & ANEG_CFG_NP) == 0 &&
	    !(ap->flags & MR_NP_RX)) {
	  ap->state = ANEG_STATE_IDLE_DETECT_INIT;
	} else {
	  ret = ANEG_FAILED;
	}
      }
    }
    break;

  case ANEG_STATE_IDLE_DETECT_INIT:
    ap->link_time = ap->cur_time;
    tp->mac_mode &= ~MAC_MODE_SEND_CONFIGS;
    tw32(MAC_MODE, tp->mac_mode);
    tr32(MAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,.40);

    ap->state = ANEG_STATE_IDLE_DETECT;
    ret = ANEG_TIMER_ENAB;
    break;

  case ANEG_STATE_IDLE_DETECT:
    if (ap->ability_match != 0 &&
	ap->rxconfig == 0) {
      ap->state = ANEG_STATE_AN_ENABLE;
      break;
    }
    delta = ap->cur_time - ap->link_time;
    if (delta > ANEG_STATE_SETTLE_TIME) {
      /* XXX another gem from the Broadcom driver :( */
      ap->state = ANEG_STATE_LINK_OK;
    }
    break;

  case ANEG_STATE_LINK_OK:
    ap->flags |= (MR_AN_COMPLETE | MR_LINK_OK);
    ret = ANEG_DONE;
    break;

  case ANEG_STATE_NEXT_PAGE_WAIT_INIT:
    /* ??? unimplemented */
    break;

  case ANEG_STATE_NEXT_PAGE_WAIT:
    /* ??? unimplemented */
    break;

  default:
    ret = ANEG_FAILED;
    break;
  };

  return ret;
}
static int 
tg3_setup_fiber_phy(struct tg3 *tp)
{
  uint32_t orig_pause_cfg;
  uint16_t orig_active_speed;
  uint8_t orig_active_duplex;
  int current_link_up;
  int i;

  orig_pause_cfg =
    (tp->tg3_flags & (TG3_FLAG_RX_PAUSE |
		      TG3_FLAG_TX_PAUSE));
  orig_active_speed = tp->link_config.active_speed;
  orig_active_duplex = tp->link_config.active_duplex;

  tp->mac_mode &= ~(MAC_MODE_PORT_MODE_MASK | MAC_MODE_HALF_DUPLEX);
  tp->mac_mode |= MAC_MODE_PORT_MODE_TBI;
  tw32(MAC_MODE, tp->mac_mode);
  tr32(MAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  /* Reset when initting first time or we have a link. */
  if (!(tp->tg3_flags & TG3_FLAG_INIT_COMPLETE) ||
      (tr32(MAC_STATUS) & MAC_STATUS_PCS_SYNCED)) {
    /* Set PLL lock range. */
    tg3_writephy(tp, 0x16, 0x8007);

    /* SW reset */
    tg3_writephy(tp, MII_BMCR, BMCR_RESET);

    /* Wait for reset to complete. */
    /* XXX schedule_timeout() ... */
    for (i = 0; i < 500; i++)
      capros_Sleep_sleep(KR_SLEEP,.10);

    /* Config mode; select PMA/Ch 1 regs. */
    tg3_writephy(tp, 0x10, 0x8411);

    /* Enable auto-lock and comdet, select txclk for tx. */
    tg3_writephy(tp, 0x11, 0x0a10);

    tg3_writephy(tp, 0x18, 0x00a0);
    tg3_writephy(tp, 0x16, 0x41ff);

    /* Assert and deassert POR. */
    tg3_writephy(tp, 0x13, 0x0400);
    capros_Sleep_sleep(KR_SLEEP,.40);
    tg3_writephy(tp, 0x13, 0x0000);

    tg3_writephy(tp, 0x11, 0x0a50);
    capros_Sleep_sleep(KR_SLEEP,.40);
    tg3_writephy(tp, 0x11, 0x0a10);

    /* Wait for signal to stabilize */
    /* XXX schedule_timeout() ... */
    for (i = 0; i < 15000; i++)
      capros_Sleep_sleep(KR_SLEEP,.10);

    /* Deselect the channel register so we can read the PHYID
     * later.
     */
    tg3_writephy(tp, 0x10, 0x8011);
  }

  /* Enable link change interrupt unless serdes polling.  */
  if (!(tp->tg3_flags & TG3_FLAG_POLL_SERDES))
    tw32(MAC_EVENT, MAC_EVENT_LNKSTATE_CHANGED);
  else
    tw32(MAC_EVENT, 0);
  tr32(MAC_EVENT);
  capros_Sleep_sleep(KR_SLEEP,.40);

  current_link_up = 0;
  if (tr32(MAC_STATUS) & MAC_STATUS_PCS_SYNCED) {
    if (tp->link_config.autoneg == AUTONEG_ENABLE &&
	!(tp->tg3_flags & TG3_FLAG_GOT_SERDES_FLOWCTL)) {
      struct tg3_fiber_aneginfo aninfo;
      int status = ANEG_FAILED;
      unsigned int tick;
      uint32_t tmp;

      memset(&aninfo, 0, sizeof(aninfo));
      aninfo.flags |= (MR_AN_ENABLE);

      tw32(MAC_TX_AUTO_NEG, 0);

      tmp = tp->mac_mode & ~MAC_MODE_PORT_MODE_MASK;
      tw32(MAC_MODE, tmp | MAC_MODE_PORT_MODE_GMII);
      tr32(MAC_MODE);
      capros_Sleep_sleep(KR_SLEEP,.40);

      tw32(MAC_MODE, tp->mac_mode | MAC_MODE_SEND_CONFIGS);
      tr32(MAC_MODE);
      capros_Sleep_sleep(KR_SLEEP,.40);

      aninfo.state = ANEG_STATE_UNKNOWN;
      aninfo.cur_time = 0;
      tick = 0;
      while (++tick < 195000) {
	status = tg3_fiber_aneg_smachine(tp, &aninfo);
	if (status == ANEG_DONE ||
	    status == ANEG_FAILED)
	  break;

	capros_Sleep_sleep(KR_SLEEP,.1);
      }

      tp->mac_mode &= ~MAC_MODE_SEND_CONFIGS;
      tw32(MAC_MODE, tp->mac_mode);
      tr32(MAC_MODE);
      capros_Sleep_sleep(KR_SLEEP,.40);

      if (status == ANEG_DONE &&
	  (aninfo.flags &
	   (MR_AN_COMPLETE | MR_LINK_OK |
	    MR_LP_ADV_FULL_DUPLEX))) {
	uint32_t local_adv, remote_adv;

	local_adv = ADVERTISE_PAUSE_CAP;
	remote_adv = 0;
	if (aninfo.flags & MR_LP_ADV_SYM_PAUSE)
	  remote_adv |= LPA_PAUSE_CAP;
	if (aninfo.flags & MR_LP_ADV_ASYM_PAUSE)
	  remote_adv |= LPA_PAUSE_ASYM;

	tg3_setup_flow_control(tp, local_adv, remote_adv);

	tp->tg3_flags |=
	  TG3_FLAG_GOT_SERDES_FLOWCTL;
	current_link_up = 1;
      }
      for (i = 0; i < 60; i++) {
	capros_Sleep_sleep(KR_SLEEP,.20);
	tw32(MAC_STATUS,
	     (MAC_STATUS_SYNC_CHANGED |
	      MAC_STATUS_CFG_CHANGED));
	tr32(MAC_STATUS);
	capros_Sleep_sleep(KR_SLEEP,.40);
	if ((tr32(MAC_STATUS) &
	     (MAC_STATUS_SYNC_CHANGED |
	      MAC_STATUS_CFG_CHANGED)) == 0)
	  break;
      }
      if (current_link_up == 0 &&
	  (tr32(MAC_STATUS) & MAC_STATUS_PCS_SYNCED)) {
	current_link_up = 1;
      }
    } else {
      /* Forcing 1000FD link up. */
      current_link_up = 1;
    }
  }

  tp->mac_mode &= ~MAC_MODE_LINK_POLARITY;
  tw32(MAC_MODE, tp->mac_mode);
  tr32(MAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  tp->hw_status->status =
    (SD_STATUS_UPDATED |
     (tp->hw_status->status & ~SD_STATUS_LINK_CHG));

  for (i = 0; i < 100; i++) {
    capros_Sleep_sleep(KR_SLEEP,.20);
    tw32(MAC_STATUS,
	 (MAC_STATUS_SYNC_CHANGED |
	  MAC_STATUS_CFG_CHANGED));
    tr32(MAC_STATUS);
    capros_Sleep_sleep(KR_SLEEP,.40);
    if ((tr32(MAC_STATUS) &
	 (MAC_STATUS_SYNC_CHANGED |
	  MAC_STATUS_CFG_CHANGED)) == 0)
      break;
  }

  if ((tr32(MAC_STATUS) & MAC_STATUS_PCS_SYNCED) == 0)
    current_link_up = 0;

  if (current_link_up == 1) {
    tp->link_config.active_speed = SPEED_1000;
    tp->link_config.active_duplex = DUPLEX_FULL;
  } else {
    tp->link_config.active_speed = SPEED_INVALID;
    tp->link_config.active_duplex = DUPLEX_INVALID;
  }

  if (current_link_up != netif_carrier_ok(tp->dev)) {
#if 0
    if (current_link_up)
      netif_carrier_on(tp->dev);
    else
      netif_carrier_off(tp->dev);
#endif
    tg3_link_report(tp);
  } else {
    uint32_t now_pause_cfg =
      tp->tg3_flags & (TG3_FLAG_RX_PAUSE |
		       TG3_FLAG_TX_PAUSE);
    if (orig_pause_cfg != now_pause_cfg ||
	orig_active_speed != tp->link_config.active_speed ||
	orig_active_duplex != tp->link_config.active_duplex)
      tg3_link_report(tp);
  }

  if ((tr32(MAC_STATUS) & MAC_STATUS_PCS_SYNCED) == 0) {
    tw32(MAC_MODE, tp->mac_mode | MAC_MODE_LINK_POLARITY);
    tr32(MAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,40);
    if (tp->tg3_flags & TG3_FLAG_INIT_COMPLETE) {
      tw32(MAC_MODE, tp->mac_mode);
      tr32(MAC_MODE);
      capros_Sleep_sleep(KR_SLEEP,40);
    }
  }

  return 0;
}
static int
tg3_setup_phy(struct tg3 *tp)
{
  int err;
  
  if (tp->phy_id == PHY_ID_SERDES) {
    err = tg3_setup_fiber_phy(tp);
  } else {
    err = tg3_setup_copper_phy(tp);
  }
  
  if (tp->link_config.active_speed == SPEED_1000 &&
      tp->link_config.active_duplex == DUPLEX_HALF)
    tw32(MAC_TX_LENGTHS,
	 ((2 << TX_LENGTHS_IPG_CRS_SHIFT) |
	  (6 << TX_LENGTHS_IPG_SHIFT) |
	  (0xff << TX_LENGTHS_SLOT_TIME_SHIFT)));
  else
    tw32(MAC_TX_LENGTHS,
	 ((2 << TX_LENGTHS_IPG_CRS_SHIFT) |
	  (6 << TX_LENGTHS_IPG_SHIFT) |
	  (32 << TX_LENGTHS_SLOT_TIME_SHIFT)));
  
  return err;
}


int 
tg3_set_power_state(struct tg3 *tp, int state)
{
  uint32_t misc_host_ctrl;
  uint16_t power_control, power_caps;
  int pm = tp->pm_cap;

  /* Make sure register accesses (indirect or otherwise)
   * will function correctly.
   */
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev,
			      TG3PCI_MISC_HOST_CTRL,
			      tp->misc_host_ctrl);

  pciprobe_read_config_word(KR_PCI_PROBE_C,tp->pdev,
			    pm + PCI_PM_CTRL,
			    &power_control);
  power_control |= PCI_PM_CTRL_PME_STATUS;
  power_control &= ~(PCI_PM_CTRL_STATE_MASK);
  switch (state) {
  case 0:
    power_control |= 0;
    pciprobe_write_config_word(KR_PCI_PROBE_C,tp->pdev,
			       pm + PCI_PM_CTRL,
			       power_control);
    tw32(GRC_LOCAL_CTRL, tp->grc_local_ctrl);
    tr32(GRC_LOCAL_CTRL);
    capros_Sleep_sleep(KR_SLEEP,0.100);

    return 0;

  case 1:
    power_control |= 1;
    break;

  case 2:
    power_control |= 2;
    break;

  case 3:
    power_control |= 3;
    break;

  default:
    kprintf(KR_OSTREAM,"Invalid power state (%d)requested.\n",state);
    return -RC_EINVAL;
  };

  power_control |= PCI_PM_CTRL_PME_ENABLE;

  misc_host_ctrl = tr32(TG3PCI_MISC_HOST_CTRL);
  tw32(TG3PCI_MISC_HOST_CTRL,
       misc_host_ctrl | MISC_HOST_CTRL_MASK_PCI_INT);

  if (tp->link_config.phy_is_low_power == 0) {
    tp->link_config.phy_is_low_power = 1;
    tp->link_config.orig_speed = tp->link_config.speed;
    tp->link_config.orig_duplex = tp->link_config.duplex;
    tp->link_config.orig_autoneg = tp->link_config.autoneg;
  }

  if (tp->phy_id != PHY_ID_SERDES) {
    tp->link_config.speed = SPEED_10;
    tp->link_config.duplex = DUPLEX_HALF;
    tp->link_config.autoneg = AUTONEG_ENABLE;
    tg3_setup_phy(tp);
  }

  pciprobe_read_config_word(KR_PCI_PROBE_C,tp->pdev, 
			    pm + PCI_PM_PMC, &power_caps);

  if (tp->tg3_flags & TG3_FLAG_WOL_ENABLE) {
    uint32_t mac_mode;

    if (tp->phy_id != PHY_ID_SERDES) {
      tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x5a);
      capros_Sleep_sleep(KR_SLEEP,.40);

      mac_mode = MAC_MODE_PORT_MODE_MII;

      if (GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5700 ||
	  !(tp->tg3_flags & TG3_FLAG_WOL_SPEED_100MB))
	mac_mode |= MAC_MODE_LINK_POLARITY;
    } else {
      mac_mode = MAC_MODE_PORT_MODE_TBI;
    }

    if (((power_caps & PCI_PM_CAP_PME_D3cold) &&
	 (tp->tg3_flags & TG3_FLAG_WOL_ENABLE)))
      mac_mode |= MAC_MODE_MAGIC_PKT_ENABLE;

    tw32(MAC_MODE, mac_mode);
    tr32(MAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,0.100);

    tw32(MAC_RX_MODE, RX_MODE_ENABLE);
    tr32(MAC_RX_MODE);
    capros_Sleep_sleep(KR_SLEEP,.10);
  }

  if (tp->tg3_flags & TG3_FLAG_WOL_SPEED_100MB) {
    uint32_t base_val;

    base_val = 0;
    if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
	GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701)
      base_val |= (CLOCK_CTRL_RXCLK_DISABLE |
		   CLOCK_CTRL_TXCLK_DISABLE);

    tw32(TG3PCI_CLOCK_CTRL, base_val |
	 CLOCK_CTRL_ALTCLK);
    tr32(TG3PCI_CLOCK_CTRL);
    capros_Sleep_sleep(KR_SLEEP,40);

    tw32(TG3PCI_CLOCK_CTRL, base_val |
	 CLOCK_CTRL_ALTCLK |
	 CLOCK_CTRL_44MHZ_CORE);
    tr32(TG3PCI_CLOCK_CTRL);
    capros_Sleep_sleep(KR_SLEEP,40);

    tw32(TG3PCI_CLOCK_CTRL, base_val |
	 CLOCK_CTRL_44MHZ_CORE);
    tr32(TG3PCI_CLOCK_CTRL);
    capros_Sleep_sleep(KR_SLEEP,40);
  } else {
    uint32_t base_val;

    base_val = 0;
    if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
	GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701)
      base_val |= (CLOCK_CTRL_RXCLK_DISABLE |
		   CLOCK_CTRL_TXCLK_DISABLE);

    tw32(TG3PCI_CLOCK_CTRL, base_val |
	 CLOCK_CTRL_ALTCLK |
	 CLOCK_CTRL_PWRDOWN_PLL133);
    tr32(TG3PCI_CLOCK_CTRL);
    capros_Sleep_sleep(KR_SLEEP,40);
  }

  if (!(tp->tg3_flags & TG3_FLAG_EEPROM_WRITE_PROT) &&
      (tp->tg3_flags & TG3_FLAG_WOL_ENABLE)) {
    if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
	GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701) {
      tw32(GRC_LOCAL_CTRL,
	   (GRC_LCLCTRL_GPIO_OE0 |
	    GRC_LCLCTRL_GPIO_OE1 |
	    GRC_LCLCTRL_GPIO_OE2 |
	    GRC_LCLCTRL_GPIO_OUTPUT0 |
	    GRC_LCLCTRL_GPIO_OUTPUT1));
      tr32(GRC_LOCAL_CTRL);
      capros_Sleep_sleep(KR_SLEEP,100);
    } else {
      tw32(GRC_LOCAL_CTRL,
	   (GRC_LCLCTRL_GPIO_OE0 |
	    GRC_LCLCTRL_GPIO_OE1 |
	    GRC_LCLCTRL_GPIO_OE2 |
	    GRC_LCLCTRL_GPIO_OUTPUT1 |
	    GRC_LCLCTRL_GPIO_OUTPUT2));
      tr32(GRC_LOCAL_CTRL);
      capros_Sleep_sleep(KR_SLEEP,100);

      tw32(GRC_LOCAL_CTRL,
	   (GRC_LCLCTRL_GPIO_OE0 |
	    GRC_LCLCTRL_GPIO_OE1 |
	    GRC_LCLCTRL_GPIO_OE2 |
	    GRC_LCLCTRL_GPIO_OUTPUT0 |
	    GRC_LCLCTRL_GPIO_OUTPUT1 |
	    GRC_LCLCTRL_GPIO_OUTPUT2));
      tr32(GRC_LOCAL_CTRL);
      capros_Sleep_sleep(KR_SLEEP,100);

      tw32(GRC_LOCAL_CTRL,
	   (GRC_LCLCTRL_GPIO_OE0 |
	    GRC_LCLCTRL_GPIO_OE1 |
	    GRC_LCLCTRL_GPIO_OE2 |
	    GRC_LCLCTRL_GPIO_OUTPUT0 |
	    GRC_LCLCTRL_GPIO_OUTPUT1));
      tr32(GRC_LOCAL_CTRL);
      capros_Sleep_sleep(KR_SLEEP,100);
    }
  }

  /* Finally, set the new power state. */
  pciprobe_write_config_word(KR_PCI_PROBE_C,tp->pdev, 
			     pm + PCI_PM_CTRL, power_control);

  return 0;
}

static int
tg3_phy_probe(struct tg3 *tp)
{
  uint32_t eeprom_phy_id, hw_phy_id_1, hw_phy_id_2;
  uint32_t hw_phy_id, hw_phy_id_masked;
  enum phy_led_mode eeprom_led_mode;
  uint32_t val;
  int eeprom_signature_found, err;

  tp->phy_id = PHY_ID_INVALID;
  
#if 0
  for (i = 0; i < ARRAY_SIZE(subsys_id_to_phy_id); i++) {
    if ((subsys_id_to_phy_id[i].subsys_vendor ==
	 tp->pdev->subsystem_vendor) &&
	(subsys_id_to_phy_id[i].subsys_devid ==
	 tp->pdev->subsystem_device)) {
      tp->phy_id = subsys_id_to_phy_id[i].phy_id;
      break;
    }
  }
#endif
  
  tp->phy_id =  PHY_ID_BCM5701;
  
  eeprom_phy_id = PHY_ID_INVALID;
  eeprom_led_mode = led_mode_auto;
  eeprom_signature_found = 0;
  tg3_read_mem(tp, NIC_SRAM_DATA_SIG, &val);
  
  if (val == NIC_SRAM_DATA_SIG_MAGIC) {
    uint32_t nic_cfg;

    tg3_read_mem(tp, NIC_SRAM_DATA_CFG, &nic_cfg);
    eeprom_signature_found = 1;

    if ((nic_cfg & NIC_SRAM_DATA_CFG_PHY_TYPE_MASK) ==
	NIC_SRAM_DATA_CFG_PHY_TYPE_FIBER) {
      eeprom_phy_id = PHY_ID_SERDES;
    } else {
      uint32_t nic_phy_id;

      tg3_read_mem(tp, NIC_SRAM_DATA_PHY_ID, &nic_phy_id);
      if (nic_phy_id != 0) {
	uint32_t id1 = nic_phy_id & NIC_SRAM_DATA_PHY_ID1_MASK;
	uint32_t id2 = nic_phy_id & NIC_SRAM_DATA_PHY_ID2_MASK;

	eeprom_phy_id  = (id1 >> 16) << 10;
	eeprom_phy_id |= (id2 & 0xfc00) << 16;
	eeprom_phy_id |= (id2 & 0x03ff) <<  0;
      }
    }

    switch (nic_cfg & NIC_SRAM_DATA_CFG_LED_MODE_MASK) {
    case NIC_SRAM_DATA_CFG_LED_TRIPLE_SPD:
      eeprom_led_mode = led_mode_three_link;
      break;

    case NIC_SRAM_DATA_CFG_LED_LINK_SPD:
      eeprom_led_mode = led_mode_link10;
      break;

    default:
      eeprom_led_mode = led_mode_auto;
      break;
    };
    if ((tp->pci_chip_rev_id == CHIPREV_ID_5703_A1 ||
	 tp->pci_chip_rev_id == CHIPREV_ID_5703_A2) &&
	(nic_cfg & NIC_SRAM_DATA_CFG_EEPROM_WP))
      tp->tg3_flags |= TG3_FLAG_EEPROM_WRITE_PROT;

    if (nic_cfg & NIC_SRAM_DATA_CFG_ASF_ENABLE)
      tp->tg3_flags |= TG3_FLAG_ENABLE_ASF;
    if (nic_cfg & NIC_SRAM_DATA_CFG_FIBER_WOL)
      tp->tg3_flags |= TG3_FLAG_SERDES_WOL_CAP;
  }

  /* Now read the physical PHY_ID from the chip and verify
   * that it is sane.  If it doesn't look good, we fall back
   * to either the hard-coded table based PHY_ID and failing
   * that the value found in the eeprom area.
   */
  err  = tg3_readphy(tp, MII_PHYSID1, &hw_phy_id_1);
  err |= tg3_readphy(tp, MII_PHYSID2, &hw_phy_id_2);

  hw_phy_id  = (hw_phy_id_1 & 0xffff) << 10;
  hw_phy_id |= (hw_phy_id_2 & 0xfc00) << 16;
  hw_phy_id |= (hw_phy_id_2 & 0x03ff) <<  0;

  hw_phy_id_masked = hw_phy_id & PHY_ID_MASK;

  if (!err && KNOWN_PHY_ID(hw_phy_id_masked)) {
    tp->phy_id = hw_phy_id;
  } else {
    /* phy_id currently holds the value found in the
     * subsys_id_to_phy_id[] table or PHY_ID_INVALID
     * if a match was not found there.
     */
    if (tp->phy_id == PHY_ID_INVALID) {
      if (!eeprom_signature_found ||
	  !KNOWN_PHY_ID(eeprom_phy_id & PHY_ID_MASK))
	return -RC_ENODEV;
      tp->phy_id = eeprom_phy_id;
    }
  }

  err = tg3_phy_reset(tp, 1);
  if (err)
    return err;

  if (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
      tp->pci_chip_rev_id == CHIPREV_ID_5701_B0) {
    uint32_t mii_tg3_ctrl;
		
    /* These chips, when reset, only advertise 10Mb
     * capabilities.  Fix that.*/
    err  = tg3_writephy(tp, MII_ADVERTISE,
			(ADVERTISE_CSMA |
			 ADVERTISE_PAUSE_CAP |
			 ADVERTISE_10HALF |
			 ADVERTISE_10FULL |
			 ADVERTISE_100HALF |
			 ADVERTISE_100FULL));
    mii_tg3_ctrl = (MII_TG3_CTRL_ADV_1000_HALF |
		    MII_TG3_CTRL_ADV_1000_FULL |
		    MII_TG3_CTRL_AS_MASTER |
		    MII_TG3_CTRL_ENABLE_AS_MASTER);
    if (tp->tg3_flags & TG3_FLAG_10_100_ONLY)
      mii_tg3_ctrl = 0;

    err |= tg3_writephy(tp, MII_TG3_CTRL, mii_tg3_ctrl);
    err |= tg3_writephy(tp, MII_BMCR,
			(BMCR_ANRESTART | BMCR_ANENABLE));
  }

  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703) {
    tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x0c00);
    tg3_writephy(tp, MII_TG3_DSP_ADDRESS, 0x201f);
    tg3_writephy(tp, MII_TG3_DSP_RW_PORT, 0x2aaa);
  }

  if ((GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704) &&
      (tp->pci_chip_rev_id == CHIPREV_ID_5704_A0)) {
    tg3_writephy(tp, 0x1c, 0x8d68);
    tg3_writephy(tp, 0x1c, 0x8d68);
  }

  /* Enable Ethernet@WireSpeed */
  tg3_writephy(tp, MII_TG3_AUX_CTRL, 0x7007);
  tg3_readphy(tp, MII_TG3_AUX_CTRL, &val);
  tg3_writephy(tp, MII_TG3_AUX_CTRL, (val | (1 << 15) | (1 << 4)));

  if (!err && ((tp->phy_id & PHY_ID_MASK) == PHY_ID_BCM5401)) {
    err = tg3_init_5401phy_dsp(tp);
  }

#if 0
  /* Determine the PHY led mode. */
  if (tp->pdev->subsystem_vendor == PCI_VENDOR_ID_DELL) {
    tp->led_mode = led_mode_link10;
  } else 
#endif

    {
      tp->led_mode = led_mode_three_link;
      if (eeprom_signature_found &&
	  eeprom_led_mode != led_mode_auto)
	tp->led_mode = eeprom_led_mode;
    }
  
#if 0
  if (tp->phy_id == PHY_ID_SERDES)
    tp->link_config.advertising =
      (ADVERTISED_1000baseT_Half |
       ADVERTISED_1000baseT_Full |
       ADVERTISED_Autoneg |
       ADVERTISED_FIBRE);
#endif

  if (tp->tg3_flags & TG3_FLAG_10_100_ONLY)
    tp->link_config.advertising &=
      ~(ADVERTISED_1000baseT_Half |
	ADVERTISED_1000baseT_Full);

  return err;
}

static 
void tg3_read_partno(struct tg3 *tp)
{
  unsigned char vpd_data[256];
  int i;
  
  for (i = 0; i < 256; i += 4) {
    uint32_t tmp;
    
    if (tg3_nvram_read(tp, 0x100 + i, &tmp))
      goto out_not_found;
    
    vpd_data[i + 0] = ((tmp >>  0) & 0xff);
    vpd_data[i + 1] = ((tmp >>  8) & 0xff);
    vpd_data[i + 2] = ((tmp >> 16) & 0xff);
    vpd_data[i + 3] = ((tmp >> 24) & 0xff);
  }
  
  /* Now parse and find the part number. */
  for (i = 0; i < 256; ) {
    unsigned char val = vpd_data[i];
    int block_end;
    
    if (val == 0x82 || val == 0x91) {
      i = (i + 3 +
	   (vpd_data[i + 1] +
	    (vpd_data[i + 2] << 8)));
      continue;
    }
    
    if (val != 0x90)
      goto out_not_found;
    
    block_end = (i + 3 +
		 (vpd_data[i + 1] +
		  (vpd_data[i + 2] << 8)));
    i += 3;
    while (i < block_end) {
      if (vpd_data[i + 0] == 'P' &&
	  vpd_data[i + 1] == 'N') {
	int partno_len = vpd_data[i + 2];
	
	if (partno_len > 24)
	  goto out_not_found;
	
	memcpy(tp->board_part_number,
	       &vpd_data[i + 3],
	       partno_len);
	
	/* Success. */
	return;
      }
    }
    
    /* Part number not found. */
    goto out_not_found;
  }
  
 out_not_found:
  strcpy(tp->board_part_number, "none");
}


int
tg3_get_invariants(struct tg3 *tp)
{
  uint32_t misc_ctrl_reg;
  uint32_t cacheline_sz_reg;
  uint32_t pci_state_reg, grc_misc_cfg;
  int err;

#if 0
  /* If we have an AMD 762 or Intel ICH/ICH0 chipset, write
   * reordering to the mailbox registers done by the host
   * controller can cause major troubles.  We read back from
   * every mailbox register write to force the writes to be
   * posted to the chip in order.
   */
  if (pciprobe_find_device(KR_PCI_PROBE_C,PCI_VENDOR_ID_INTEL,
			   PCIPROBE_DEVICE_ID_INTEL_82801AA_8, NULL) ||
      pciprobe_find_device(KR_PCI_PROBE_C,PCI_VENDOR_ID_INTEL,
			   PCI_DEVICE_ID_INTEL_82801AB_8, NULL) ||
      pciprobe_find_device(KR_PCI_PROBE_C,PCI_VENDOR_ID_AMD,
			   PCI_DEVICE_ID_AMD_FE_GATE_700C, NULL))
    tp->tg3_flags |= TG3_FLAG_MBOX_WRITE_REORDER;
#endif

  /* It is absolutely critical that TG3PCI_MISC_HOST_CTRL
   * has the register indirect write enable bit set before
   * we try to access any of the MMIO registers.  It is also
   * critical that the PCI-X hw workaround situation is decided
   * before that as well.
   */
  pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_MISC_HOST_CTRL,
			     &misc_ctrl_reg);
  tp->pci_chip_rev_id = (misc_ctrl_reg >>
			 MISC_HOST_CTRL_CHIPREV_SHIFT);
  /* Initialize misc host control in PCI block. */
  tp->misc_host_ctrl |= (misc_ctrl_reg &
			 MISC_HOST_CTRL_CHIPREV);
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_MISC_HOST_CTRL,
			      tp->misc_host_ctrl);

  pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_CACHELINESZ,
			     &cacheline_sz_reg);

  kprintf(KR_OSTREAM,"cacheline_sz = %x",cacheline_sz_reg);
  tp->pci_cacheline_sz = (cacheline_sz_reg >>  0) & 0xff;
  tp->pci_lat_timer    = (cacheline_sz_reg >>  8) & 0xff;
  tp->pci_hdr_type     = (cacheline_sz_reg >> 16) & 0xff;
  tp->pci_bist         = (cacheline_sz_reg >> 24) & 0xff;

  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 &&
      tp->pci_lat_timer < 64) {
    tp->pci_lat_timer = 64;

    cacheline_sz_reg  = ((tp->pci_cacheline_sz & 0xff) <<  0);
    cacheline_sz_reg |= ((tp->pci_lat_timer    & 0xff) <<  8);
    cacheline_sz_reg |= ((tp->pci_hdr_type     & 0xff) << 16);
    cacheline_sz_reg |= ((tp->pci_bist         & 0xff) << 24);

    pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_CACHELINESZ,
				cacheline_sz_reg);
  }

  pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_PCISTATE,
			     &pci_state_reg);
  
  if ((pci_state_reg & PCISTATE_CONV_PCI_MODE) == 0) {
    tp->tg3_flags |= TG3_FLAG_PCIX_MODE;

    /* If this is a 5700 BX chipset, and we are in PCI-X
     * mode, enable register write workaround.
     *
     * The workaround is to use indirect register accesses
     * for all chip writes not to mailbox registers.
     */
    if (GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5700_BX) {
      uint32_t pm_reg;
      uint16_t pci_cmd;

      tp->tg3_flags |= TG3_FLAG_PCIX_TARGET_HWBUG;

      /* The chip can have it's power management PCI config
       * space registers clobbered due to this bug.
       * So explicitly force the chip into D0 here.
       */
      pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_PM_CTRL_STAT,
				 &pm_reg);
      pm_reg &= ~PCI_PM_CTRL_STATE_MASK;
      pm_reg |= PCI_PM_CTRL_PME_ENABLE | 0 /* D0 */;
      pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_PM_CTRL_STAT,
				  pm_reg);

      /* Also, force SERR#/PERR# in PCI command. */
      pciprobe_read_config_word(KR_PCI_PROBE_C,tp->pdev, 
				PCI_COMMAND, &pci_cmd);
      pci_cmd |= PCI_COMMAND_PARITY | PCI_COMMAND_SERR;
      pciprobe_write_config_word(KR_PCI_PROBE_C,tp->pdev, 
				 PCI_COMMAND, pci_cmd);
    }
  }
  
  /* Back to back register writes can cause problems on this chip,
   * the workaround is to read back all reg writes except those to
   * mailbox regs.  See tg3_write_indirect_reg32().
   */
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701)
    tp->tg3_flags |= TG3_FLAG_5701_REG_WRITE_BUG;
  
  if ((pci_state_reg & PCISTATE_BUS_SPEED_HIGH) != 0)
    tp->tg3_flags |= TG3_FLAG_PCI_HIGH_SPEED;
  if ((pci_state_reg & PCISTATE_BUS_32BIT) != 0)
    tp->tg3_flags |= TG3_FLAG_PCI_32BIT;

  /* Chip-specific fixup from Broadcom driver */
  if ((tp->pci_chip_rev_id == CHIPREV_ID_5704_A0) &&
      (!(pci_state_reg & PCISTATE_RETRY_SAME_DMA))) {
    pci_state_reg |= PCISTATE_RETRY_SAME_DMA;
    pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
				TG3PCI_PCISTATE, pci_state_reg);
  }

  /* Force the chip into D0. */
  err = tg3_set_power_state(tp, 0);
  if (err) {
    kprintf(KR_OSTREAM,"transition to D0 failed\n");
    return err;
  }

  /* 5700 B0 chips do not support checksumming correctly due
   * to hardware bugs.
   */
  if (tp->pci_chip_rev_id == CHIPREV_ID_5700_B0)
    tp->tg3_flags |= TG3_FLAG_BROKEN_CHECKSUMS;

  /* Pseudo-header checksum is done by hardware logic and not
   * the offload processers, so make the chip do the pseudo-
   * header checksums on receive.  For transmit it is more
   * convenient to do the pseudo-header checksum in software
   * as Linux does that on transmit for us in all cases.
   */
  tp->tg3_flags |= TG3_FLAG_NO_TX_PSEUDO_CSUM;
  tp->tg3_flags &= ~TG3_FLAG_NO_RX_PSEUDO_CSUM;

  /* Derive initial jumbo mode from MTU assigned in
   * ether_setup() via the alloc_etherdev() call
   */
  if (tp->mtu > ETH_DATA_LEN)
    tp->tg3_flags |= TG3_FLAG_JUMBO_ENABLE;

  /* Determine WakeOnLan speed to use. */
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
      tp->pci_chip_rev_id == CHIPREV_ID_5701_A0 ||
      tp->pci_chip_rev_id == CHIPREV_ID_5701_B0 ||
      tp->pci_chip_rev_id == CHIPREV_ID_5701_B2) {
    tp->tg3_flags &= ~(TG3_FLAG_WOL_SPEED_100MB);
  } else {
    tp->tg3_flags |= TG3_FLAG_WOL_SPEED_100MB;
  }

  /* Only 5701 and later support tagged irq status mode.
   *
   * However, since we are using NAPI avoid tagged irq status
   * because the interrupt condition is more difficult to
   * fully clear in that mode.
   */
  tp->coalesce_mode = 0;

  if (GET_CHIP_REV(tp->pci_chip_rev_id) != CHIPREV_5700_AX &&
      GET_CHIP_REV(tp->pci_chip_rev_id) != CHIPREV_5700_BX)
    tp->coalesce_mode |= HOSTCC_MODE_32BYTE;

  /* Initialize MAC MI mode, polling disabled. */
  tw32(MAC_MI_MODE, tp->mi_mode);
  tr32(MAC_MI_MODE);
  capros_Sleep_sleep(KR_SLEEP,40);
    
  /* Initialize data/descriptor byte/word swapping. */
  tw32(GRC_MODE, tp->grc_mode);

  tg3_switch_clocks(tp);

  /* Clear this out for sanity. */
  tw32(TG3PCI_MEM_WIN_BASE_ADDR, 0);

  pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_PCISTATE,
			     &pci_state_reg);
  if ((pci_state_reg & PCISTATE_CONV_PCI_MODE) == 0 &&
      (tp->tg3_flags & TG3_FLAG_PCIX_TARGET_HWBUG) == 0) {
    uint32_t chiprevid = GET_CHIP_REV_ID(tp->misc_host_ctrl);

    if (chiprevid == CHIPREV_ID_5701_A0 ||
	chiprevid == CHIPREV_ID_5701_B0 ||
	chiprevid == CHIPREV_ID_5701_B2 ||
	chiprevid == CHIPREV_ID_5701_B5) {
      unsigned long sram_base;

      /* Write some dummy words into the SRAM status block
       * area, see if it reads back correctly.  If the return
       * value is bad, force enable the PCIX workaround.
       */
      sram_base = tp->regs + NIC_SRAM_WIN_BASE + NIC_SRAM_STATS_BLK;

      writel(0x00000000, sram_base);
      writel(0x00000000, sram_base + 4);
      writel(0xffffffff, sram_base + 4);
      if (readl(sram_base) != 0x00000000)
	tp->tg3_flags |= TG3_FLAG_PCIX_TARGET_HWBUG;
    }
  }

  capros_Sleep_sleep(KR_SLEEP,0.50);
  tg3_nvram_init(tp);

  /* Determine if TX descriptors will reside in
   * main memory or in the chip SRAM.
   */
  if (tp->tg3_flags & TG3_FLAG_PCIX_TARGET_HWBUG)
    tp->tg3_flags |= TG3_FLAG_HOST_TXDS;

  grc_misc_cfg = tr32(GRC_MISC_CFG);
  grc_misc_cfg &= GRC_MISC_CFG_BOARD_ID_MASK;

  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704 &&
      grc_misc_cfg == GRC_MISC_CFG_BOARD_ID_5704CIOBE) {
    tp->tg3_flags |= TG3_FLAG_SPLIT_MODE;
    tp->split_mode_max_reqs = SPLIT_MODE_5704_MAX_REQ;
  }

  /* this one is limited to 10/100 only */
  if (grc_misc_cfg == GRC_MISC_CFG_BOARD_ID_5702FE)
    tp->tg3_flags |= TG3_FLAG_10_100_ONLY;

  err = tg3_phy_probe(tp);
  if (err) {
    kprintf(KR_OSTREAM, "phy probe failed, err %d\n",err);
    /* ... but do not return immediately ... */
  }

  tg3_read_partno(tp);

  if (tp->phy_id == PHY_ID_SERDES) {
    tp->tg3_flags &= ~TG3_FLAG_USE_MI_INTERRUPT;

    /* And override led_mode in case Dell ever makes
     * a fibre board.
     */
    tp->led_mode = led_mode_three_link;
  } else {
    if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700)
      tp->tg3_flags |= TG3_FLAG_USE_MI_INTERRUPT;
    else
      tp->tg3_flags &= ~TG3_FLAG_USE_MI_INTERRUPT;
  }

  /* 5700 {AX,BX} chips have a broken status block link
   * change bit implementation, so we must use the
   * status register in those cases.
   */
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700)
    tp->tg3_flags |= TG3_FLAG_USE_LINKCHG_REG;
  else
    tp->tg3_flags &= ~TG3_FLAG_USE_LINKCHG_REG;

#if 0
  /* The led_mode is set during tg3_phy_probe, here we might
   * have to force the link status polling mechanism based
   * upon subsystem IDs.
   */
  if (tp->pdev->subsystem_vendor == PCI_VENDOR_ID_DELL &&
      tp->phy_id != PHY_ID_SERDES) {
    tp->tg3_flags |= (TG3_FLAG_USE_MI_INTERRUPT |
		      TG3_FLAG_USE_LINKCHG_REG);
  }
#endif
  
  /* For all SERDES we poll the MAC status register. */
  if (tp->phy_id == PHY_ID_SERDES)
    tp->tg3_flags |= TG3_FLAG_POLL_SERDES;
  else
    tp->tg3_flags &= ~TG3_FLAG_POLL_SERDES;


  /* 5700 BX chips need to have their TX producer index mailboxes
   * written twice to workaround a bug.
   */
  if (GET_CHIP_REV(tp->pci_chip_rev_id) == CHIPREV_5700_BX)
    tp->tg3_flags |= TG3_FLAG_TXD_MBOX_HWBUG;
  else
    tp->tg3_flags &= ~TG3_FLAG_TXD_MBOX_HWBUG;

#if 0
  /* 5700 chips can get confused if TX buffers straddle the
   * 4GB address boundary in some cases.
   */
  kprintf(KR_OSTREAM,"pci_chip_rev_id = %d",tp->pci_chip_rev_id);
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700)
    tp->dev->hard_start_xmit = tg3_start_xmit_4gbug;
  else
    tp->dev->hard_start_xmit = tg3_start_xmit;
#endif
  
  tp->rx_offset = 2;
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701 &&
      (tp->tg3_flags & TG3_FLAG_PCIX_MODE) != 0)
    tp->rx_offset = 0;

  /* By default, disable wake-on-lan.  User can change this
   * using ETHTOOL_SWOL. */
  tp->tg3_flags &= ~TG3_FLAG_WOL_ENABLE;

  return err;
}
int
tg3_get_device_address(struct tg3 *tp)
{
  uint32_t hi, lo, mac_offset;
  
  if (PCI_FUNC(tp->pdev->devfn) == 0)
    mac_offset = 0x7c;
  else
    mac_offset = 0xcc;
  
  /* First try to get it from MAC address mailbox. */
  tg3_read_mem(tp, NIC_SRAM_MAC_ADDR_HIGH_MBOX, &hi);
  if ((hi >> 16) == 0x484b) {
    eaddr[0] = (hi >>  8) & 0xff;
    eaddr[1] = (hi >>  0) & 0xff;
    
    tg3_read_mem(tp, NIC_SRAM_MAC_ADDR_LOW_MBOX, &lo);
    eaddr[2] = (lo >> 24) & 0xff;
    eaddr[3] = (lo >> 16) & 0xff;
    eaddr[4] = (lo >>  8) & 0xff;
    eaddr[5] = (lo >>  0) & 0xff;
  } 
  /* Next, try NVRAM. */
  else if (!tg3_nvram_read(tp, mac_offset + 0, &hi) &&
	   !tg3_nvram_read(tp, mac_offset + 4, &lo)) {
    eaddr[0] = ((hi >> 16) & 0xff);
    eaddr[1] = ((hi >> 24) & 0xff);
    eaddr[2] = ((lo >>  0) & 0xff);
    eaddr[3] = ((lo >>  8) & 0xff);
    eaddr[4] = ((lo >> 16) & 0xff);
    eaddr[5] = ((lo >> 24) & 0xff);
  }    /* Finally just fetch it out of the MAC control regs. */
  else {
    hi = tr32(MAC_ADDR_0_HIGH);
    lo = tr32(MAC_ADDR_0_LOW);
    
    eaddr[5] = lo & 0xff;
    eaddr[4] = (lo >> 8) & 0xff;
    eaddr[3] = (lo >> 16) & 0xff;
    eaddr[2] = (lo >> 24) & 0xff;
    eaddr[1] = hi & 0xff;
    eaddr[0] = (hi >> 8) & 0xff;
  }
  DEBUG_ALTIMA 
    kprintf(KR_OSTREAM, "MAC Addr %02x:%02x:%02x:%02x:%02x:%02x\n",
	    eaddr[0],eaddr[1],eaddr[2],eaddr[3],eaddr[4],eaddr[5]);

  
  return RC_OK;
}

/* Returns size of skb allocated or < 0 on error.
 *
 * We only need to fill in the address because the other members
 * of the RX descriptor are invariant, see tg3_init_rings.
 *
 * Note the purposeful assymetry of cpu vs. chip accesses.  For
 * posting buffers we only dirty the first cache line of the RX
 * descriptor (containing the address).  Whereas for the RX status
 * buffers the cpu only reads the last cacheline of the RX descriptor
 * (to fetch the error flags, vlan tag, checksum, and opaque cookie).
 */
static int 
tg3_alloc_rx_skb(struct tg3 *tp, uint32_t opaque_key,
		 int src_idx, uint32_t dest_idx_unmasked)
{
  struct tg3_rx_buffer_desc *desc;
  struct ring_info *map, *src_map;
  static uint32_t mapping = 0x20000u;
  int skb_size, dest_idx;
  
  src_map = NULL;
  switch (opaque_key) {
  case RXD_OPAQUE_RING_STD:
    dest_idx = dest_idx_unmasked % TG3_RX_RING_SIZE;
    desc = &tp->rx_std[dest_idx];
    map = &tp->rx_std_buffers[dest_idx];
    if (src_idx >= 0)
      src_map = &tp->rx_std_buffers[src_idx];
    skb_size = RX_PKT_BUF_SZ;
    break;
  case RXD_OPAQUE_RING_JUMBO:
    dest_idx = dest_idx_unmasked % TG3_RX_JUMBO_RING_SIZE;
    desc = &tp->rx_jumbo[dest_idx];
    map = &tp->rx_jumbo_buffers[dest_idx];
    if (src_idx >= 0)
      src_map = &tp->rx_jumbo_buffers[src_idx];
    skb_size = RX_JUMBO_PKT_BUF_SZ;
    break;
    
  default:
    return -RC_EINVAL;
  };
  
  desc->addr_hi = ((uint64_t)(PHYSADDR + mapping) >> 32);
  desc->addr_lo = ((uint64_t)(PHYSADDR + mapping) & 0xffffffff);
  mapping += skb_size;

  return skb_size;
}


/* Initialize tx/rx rings for packet processing.
 *
 * The chip has been shut down and the driver detached from
 * the networking, so no interrupts or new tx packets will
 * end up in the driver.  tp->{tx,}lock is not held and we are not
 * in an interrupt context and thus may sleep.
 */
void 
tg3_init_rings(struct tg3 *tp)
{
  unsigned long start, end;
  uint32_t i;

  /* Free up all the SKBs. */
  //tg3_free_rings(tp);
  
  /* Zero out all descriptors. */
  memset(tp->rx_std, 0, TG3_RX_RING_BYTES);
  memset(tp->rx_jumbo, 0, TG3_RX_JUMBO_RING_BYTES);
  memset(tp->rx_rcb, 0, TG3_RX_RCB_RING_BYTES);

  if (tp->tg3_flags & TG3_FLAG_HOST_TXDS) {
    memset(tp->tx_ring, 0, TG3_TX_RING_BYTES);
  } else {
    start = (tp->regs + NIC_SRAM_WIN_BASE +
	     NIC_SRAM_TX_BUFFER_DESC);
    end = start + TG3_TX_RING_BYTES;
    while (start < end) {
      writel(0, start);
      start += 4;
    }
    for (i = 0; i < TG3_TX_RING_SIZE; i++)
      tp->tx_buffers[i].prev_vlan_tag = 0;
  }

  /* Initialize invariants of the rings, we only set this
   * stuff once.  This works because the card does not
   * write into the rx buffer posting rings.
   */
  for (i = 0; i < TG3_RX_RING_SIZE; i++) {
    struct tg3_rx_buffer_desc *rxd;

    rxd = &tp->rx_std[i];
    rxd->idx_len = (RX_PKT_BUF_SZ - tp->rx_offset - 64)
      << RXD_LEN_SHIFT;
    rxd->type_flags = (RXD_FLAG_END << RXD_FLAGS_SHIFT);
    rxd->opaque = (RXD_OPAQUE_RING_STD |
		   (i << RXD_OPAQUE_INDEX_SHIFT));
  }
  if (tp->tg3_flags & TG3_FLAG_JUMBO_ENABLE) {
    for (i = 0; i < TG3_RX_JUMBO_RING_SIZE; i++) {
      struct tg3_rx_buffer_desc *rxd;

      rxd = &tp->rx_jumbo[i];
      rxd->idx_len = (RX_JUMBO_PKT_BUF_SZ - tp->rx_offset - 64) 
	<< RXD_LEN_SHIFT;
      rxd->type_flags = (RXD_FLAG_END << RXD_FLAGS_SHIFT)|RXD_FLAG_JUMBO;
      rxd->opaque = (RXD_OPAQUE_RING_JUMBO | (i << RXD_OPAQUE_INDEX_SHIFT));
    }
  }
  /* Now allocate fresh SKBs for each rx ring. */
  for (i = 0; i < tp->rx_pending; i++) {
    if (tg3_alloc_rx_skb(tp, RXD_OPAQUE_RING_STD,-1, i) < 0)
      break;
  }

  if (tp->tg3_flags & TG3_FLAG_JUMBO_ENABLE) {
    for (i = 0; i < tp->rx_jumbo_pending; i++) {
      if (tg3_alloc_rx_skb(tp, RXD_OPAQUE_RING_JUMBO,-1, i) < 0)
	break;
    }
  }
}

static void 
__tg3_set_rx_mode(struct tg3 *tp)
{
  uint32_t rx_mode;
  
  rx_mode = tp->rx_mode & ~(RX_MODE_PROMISC |
			    RX_MODE_KEEP_VLAN_TAG);
#if TG3_VLAN_TAG_USED
  if (!tp->vlgrp)
    rx_mode |= RX_MODE_KEEP_VLAN_TAG;
#else
  /* By definition, VLAN is disabled always in this case */
  rx_mode |= RX_MODE_KEEP_VLAN_TAG;
#endif

  
  /* FIX:: For now set the rx mode to be Promiscuous */
  rx_mode |= RX_MODE_PROMISC;
  
#if 0
  if (dev->flags & IFF_PROMISC) {
    /* Promiscuous mode. */
    rx_mode |= RX_MODE_PROMISC;
  } else if (dev->flags & IFF_ALLMULTI) {
    /* Accept all multicast. */
    tg3_set_multi (tp, 1);
  } else if (dev->mc_count < 1) {
    /* Reject all multicast. */
    tg3_set_multi (tp, 0);
  } else {
    /* Accept one or more multicast(s). */
    struct dev_mc_list *mclist;
    unsigned int i;
    uint32_t mc_filter[4] = { 0, };
    uint32_t regidx;
    uint32_t bit;
    uint32_t crc;
    for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
	 i++, mclist = mclist->next) {
      
      crc = calc_crc (mclist->dmi_addr, ETH_ALEN);
      bit = ~crc & 0x7f;
      regidx = (bit & 0x60) >> 5;
      bit &= 0x1f;
      mc_filter[regidx] |= (1 << bit);
    }
    
    tw32(MAC_HASH_REG_0, mc_filter[0]);
    tw32(MAC_HASH_REG_1, mc_filter[1]);
    tw32(MAC_HASH_REG_2, mc_filter[2]);
    tw32(MAC_HASH_REG_3, mc_filter[3]);
  }
#endif
  tp->rx_mode = rx_mode;
  tw32(MAC_RX_MODE, rx_mode);
  tr32(MAC_RX_MODE);
  capros_Sleep_sleep(KR_SLEEP,0.10);
}


/* tp->lock is held. */
static void 
tg3_chip_reset(struct tg3 *tp)
{
  uint32_t val;
  uint32_t flags_save;

  /* Force NVRAM to settle.
   * This deals with a chip bug which can result in EEPROM
   * corruption. */
  if (tp->tg3_flags & TG3_FLAG_NVRAM) {
    int i;
    
    tw32(NVRAM_SWARB, SWARB_REQ_SET1);
    for (i = 0; i < 100000; i++) {
      if (tr32(NVRAM_SWARB) & SWARB_GNT1)
	break;
      capros_Sleep_sleep(KR_SLEEP,.10);
    }
  }
  
  /* We must avoid the readl() that normally takes place.
   * It locks machines, causes machine checks, and other
   * fun things.  So, temporarily disable the 5701
   * hardware workaround, while we do the reset. */
  flags_save = tp->tg3_flags;
  tp->tg3_flags &= ~TG3_FLAG_5701_REG_WRITE_BUG;

  /* do the reset */
  tw32(GRC_MISC_CFG, GRC_MISC_CFG_CORECLK_RESET);

  /* restore 5701 hardware bug workaround flag */
  tp->tg3_flags = flags_save;
  /* Flush PCI posted writes.  The normal MMIO registers
   * are inaccessible at this time so this is the only
   * way to make this reliably.  I tried to use indirect
   * register read/write but this upset some 5701 variants.*/
  pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, PCI_COMMAND, &val);
  
  capros_Sleep_sleep(KR_SLEEP,.40);
  
  /* Re-enable indirect register accesses. */
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_MISC_HOST_CTRL,
			      tp->misc_host_ctrl);

  /* Set MAX PCI retry to zero. */
  val = (PCISTATE_ROM_ENABLE | PCISTATE_ROM_RETRY_ENABLE);
  if (tp->pci_chip_rev_id == CHIPREV_ID_5704_A0 &&
      (tp->tg3_flags & TG3_FLAG_PCIX_MODE))
    val |= PCISTATE_RETRY_SAME_DMA;
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_PCISTATE, val);
  
  pci_restore_state(tp->pdev, tp->pci_cfg_state);
  
  /* Make sure PCI-X relaxed ordering bit is clear. */
  pciprobe_read_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_X_CAPS, &val);
  val &= ~PCIX_CAPS_RELAXED_ORDERING;
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, TG3PCI_X_CAPS, val);

  tw32(MEMARB_MODE, MEMARB_MODE_ENABLE);
  tw32(TG3PCI_MISC_HOST_CTRL, tp->misc_host_ctrl);
}


static void 
tg3_stop_fw(struct tg3 *tp)
{
  if (tp->tg3_flags & TG3_FLAG_ENABLE_ASF) {
    uint32_t val;
    int i;
    
    tg3_write_mem(tp, NIC_SRAM_FW_CMD_MBOX, FWCMD_NICDRV_PAUSE_FW);
    val = tr32(GRC_RX_CPU_EVENT);
    val |= (1 << 14);
    tw32(GRC_RX_CPU_EVENT, val);
    
    /* Wait for RX cpu to ACK the event.  */
    for (i = 0; i < 100; i++) {
      if (!(tr32(GRC_RX_CPU_EVENT) & (1 << 14)))
	break;
      capros_Sleep_sleep(KR_SLEEP,0.1);
    }
  }
}


/* tp->lock is held. */
static int 
tg3_abort_hw(struct tg3 *tp)
{
  int i, err;
  
  tg3_disable_ints(tp);
  
  tp->rx_mode &= ~RX_MODE_ENABLE;
  tw32(MAC_RX_MODE, tp->rx_mode);
  tr32(MAC_RX_MODE);
  capros_Sleep_sleep(KR_SLEEP,0.10);
  err  = tg3_stop_block(tp, RCVBDI_MODE, RCVBDI_MODE_ENABLE);
  err |= tg3_stop_block(tp, RCVLPC_MODE, RCVLPC_MODE_ENABLE);
  err |= tg3_stop_block(tp, RCVLSC_MODE, RCVLSC_MODE_ENABLE);
  err |= tg3_stop_block(tp, RCVDBDI_MODE, RCVDBDI_MODE_ENABLE);
  err |= tg3_stop_block(tp, RCVDCC_MODE, RCVDCC_MODE_ENABLE);
  err |= tg3_stop_block(tp, RCVCC_MODE, RCVCC_MODE_ENABLE);
  
  err |= tg3_stop_block(tp, SNDBDS_MODE, SNDBDS_MODE_ENABLE);
  err |= tg3_stop_block(tp, SNDBDI_MODE, SNDBDI_MODE_ENABLE);
  err |= tg3_stop_block(tp, SNDDATAI_MODE, SNDDATAI_MODE_ENABLE);
  err |= tg3_stop_block(tp, RDMAC_MODE, RDMAC_MODE_ENABLE);
  err |= tg3_stop_block(tp, SNDDATAC_MODE, SNDDATAC_MODE_ENABLE);
  err |= tg3_stop_block(tp, SNDBDC_MODE, SNDBDC_MODE_ENABLE);
  if (err)
    goto out;
  tp->mac_mode &= ~MAC_MODE_TDE_ENABLE;
  tw32(MAC_MODE, tp->mac_mode);
  tr32(MAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,0.40);
  
  tp->tx_mode &= ~TX_MODE_ENABLE;
  tw32(MAC_TX_MODE, tp->tx_mode);
  tr32(MAC_TX_MODE);
  
  for (i = 0; i < MAX_WAIT_CNT; i++) {
    capros_Sleep_sleep(KR_SLEEP,0.100);
    if (!(tr32(MAC_TX_MODE) & TX_MODE_ENABLE))
      break;
  }
  if (i >= MAX_WAIT_CNT) {
    kprintf(KR_OSTREAM,"tg3_abort_hw timed out for"
	    "TX_MODE_ENABLE will not clear MAC_TX_MODE=%08x\n",
	    tr32(MAC_TX_MODE));
    return -RC_ENODEV;
  }
  err  = tg3_stop_block(tp, HOSTCC_MODE, HOSTCC_MODE_ENABLE);
  err |= tg3_stop_block(tp, WDMAC_MODE, WDMAC_MODE_ENABLE);
  err |= tg3_stop_block(tp, MBFREE_MODE, MBFREE_MODE_ENABLE);
  
  tw32(FTQ_RESET, 0xffffffff);
  tw32(FTQ_RESET, 0x00000000);
  
  err |= tg3_stop_block(tp, BUFMGR_MODE, BUFMGR_MODE_ENABLE);
  err |= tg3_stop_block(tp, MEMARB_MODE, MEMARB_MODE_ENABLE);
  if (err)
    goto out;
  
  memset(tp->hw_status, 0, TG3_HW_STATUS_SIZE);
 out:
  return err;
}



/* tp->lock is held. */
static void
__tg3_set_mac_addr(struct tg3 *tp)
{
  uint32_t addr_high, addr_low;
  int i;
  
  addr_high = ((eaddr[0] << 8) | eaddr[1]);
  addr_low = ((eaddr[2] << 24) |
	      (eaddr[3] << 16) |
	      (eaddr[4] <<  8) |
	      (eaddr[5] <<  0));
  for (i = 0; i < 4; i++) {
    tw32(MAC_ADDR_0_HIGH + (i * 8), addr_high);
    tw32(MAC_ADDR_0_LOW + (i * 8), addr_low);
  }
  addr_high = (eaddr[0] +
	       eaddr[1] +
	       eaddr[2] +
	       eaddr[3] +
	       eaddr[4] +
	       eaddr[5]) &
    TX_BACKOFF_SEED_MASK;
  tw32(MAC_TX_BACKOFF_SEED, addr_high);
}


/* tp->lock is held. */
int 
tg3_reset_hw(struct tg3 *tp)
{
  uint32_t val;
  int i, err;

  tg3_disable_ints(tp);

  tg3_stop_fw(tp);

  if (tp->tg3_flags & TG3_FLAG_INIT_COMPLETE) {
    err = tg3_abort_hw(tp);
    if (err)
      return err;
  }
  tg3_chip_reset(tp);

  tw32(GRC_MODE, tp->grc_mode);
  tg3_write_mem(tp,
		NIC_SRAM_FIRMWARE_MBOX,
		NIC_SRAM_FIRMWARE_MBOX_MAGIC1);
  if (tp->phy_id == PHY_ID_SERDES) {
    tp->mac_mode = MAC_MODE_PORT_MODE_TBI;
    tw32(MAC_MODE, tp->mac_mode);
  } else
    tw32(MAC_MODE, 0);
  tr32(MAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  /* Wait for firmware initialization to complete. */
  for (i = 0; i < 100000; i++) {
    tg3_read_mem(tp, NIC_SRAM_FIRMWARE_MBOX, &val);
    if (val == ~NIC_SRAM_FIRMWARE_MBOX_MAGIC1)
      break;
    capros_Sleep_sleep(KR_SLEEP,.010);
  }
  if (i >= 100000) {
    kprintf(KR_OSTREAM,"tg3_reset_hw timed out for"
	    "firmware will not restart magic=%08x\n",val);
    return -RC_ENODEV;
  }

  if (tp->tg3_flags & TG3_FLAG_ENABLE_ASF)
    tg3_write_mem(tp, NIC_SRAM_FW_DRV_STATE_MBOX,
		  DRV_STATE_START);
  else
    tg3_write_mem(tp, NIC_SRAM_FW_DRV_STATE_MBOX,
		  DRV_STATE_SUSPEND);

  /* This works around an issue with Athlon chipsets on
   * B3 tigon3 silicon.  This bit has no effect on any
   * other revision. */
  val = tr32(TG3PCI_CLOCK_CTRL);
  val |= CLOCK_CTRL_DELAY_PCI_GRANT;
  tw32(TG3PCI_CLOCK_CTRL, val);
  tr32(TG3PCI_CLOCK_CTRL);

  if (tp->pci_chip_rev_id == CHIPREV_ID_5704_A0 &&
      (tp->tg3_flags & TG3_FLAG_PCIX_MODE)) {
    val = tr32(TG3PCI_PCISTATE);
    val |= PCISTATE_RETRY_SAME_DMA;
    tw32(TG3PCI_PCISTATE, val);
  }
  
  /* Clear statistics/status block in chip, and status block in ram. */
  for (i = NIC_SRAM_STATS_BLK;
       i < NIC_SRAM_STATUS_BLK + TG3_HW_STATUS_SIZE;
       i += sizeof(uint32_t)) {
    tg3_write_mem(tp, i, 0);
    capros_Sleep_sleep(KR_SLEEP,.40);
  }
  memset(tp->hw_status, 0, TG3_HW_STATUS_SIZE);

  /* This value is determined during the probe time DMA
   * engine test, tg3_test_dma.
   */
  tw32(TG3PCI_DMA_RW_CTRL, tp->dma_rwctrl);

  tp->grc_mode &= ~(GRC_MODE_HOST_SENDBDS |
		    GRC_MODE_4X_NIC_SEND_RINGS |
		    GRC_MODE_NO_TX_PHDR_CSUM |
		    GRC_MODE_NO_RX_PHDR_CSUM);
  if (tp->tg3_flags & TG3_FLAG_HOST_TXDS)
    tp->grc_mode |= GRC_MODE_HOST_SENDBDS;
  else
    tp->grc_mode |= GRC_MODE_4X_NIC_SEND_RINGS;
  if (tp->tg3_flags & TG3_FLAG_NO_TX_PSEUDO_CSUM)
    tp->grc_mode |= GRC_MODE_NO_TX_PHDR_CSUM;
  if (tp->tg3_flags & TG3_FLAG_NO_RX_PSEUDO_CSUM)
    tp->grc_mode |= GRC_MODE_NO_RX_PHDR_CSUM;

  tw32(GRC_MODE,
       tp->grc_mode |
       (GRC_MODE_IRQ_ON_MAC_ATTN | GRC_MODE_HOST_STACKUP));

  /* Setup the timer prescalar register.  Clock is always 66Mhz. */
  tw32(GRC_MISC_CFG,
       (65 << GRC_MISC_CFG_PRESCALAR_SHIFT));

  /* Initialize MBUF/DESC pool. */
  tw32(BUFMGR_MB_POOL_ADDR, NIC_SRAM_MBUF_POOL_BASE);
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704)
    tw32(BUFMGR_MB_POOL_SIZE, NIC_SRAM_MBUF_POOL_SIZE64);
  else
    tw32(BUFMGR_MB_POOL_SIZE, NIC_SRAM_MBUF_POOL_SIZE96);
  tw32(BUFMGR_DMA_DESC_POOL_ADDR, NIC_SRAM_DMA_DESC_POOL_BASE);
  tw32(BUFMGR_DMA_DESC_POOL_SIZE, NIC_SRAM_DMA_DESC_POOL_SIZE);

  if (!(tp->tg3_flags & TG3_FLAG_JUMBO_ENABLE)) {
    tw32(BUFMGR_MB_RDMA_LOW_WATER,
	 tp->bufmgr_config.mbuf_read_dma_low_water);
    tw32(BUFMGR_MB_MACRX_LOW_WATER,
	 tp->bufmgr_config.mbuf_mac_rx_low_water);
    tw32(BUFMGR_MB_HIGH_WATER,
	 tp->bufmgr_config.mbuf_high_water);
  } else {
    tw32(BUFMGR_MB_RDMA_LOW_WATER,
	 tp->bufmgr_config.mbuf_read_dma_low_water_jumbo);
    tw32(BUFMGR_MB_MACRX_LOW_WATER,
	 tp->bufmgr_config.mbuf_mac_rx_low_water_jumbo);
    tw32(BUFMGR_MB_HIGH_WATER,
	 tp->bufmgr_config.mbuf_high_water_jumbo);
  }
  tw32(BUFMGR_DMA_LOW_WATER,
       tp->bufmgr_config.dma_low_water);
  tw32(BUFMGR_DMA_HIGH_WATER,
       tp->bufmgr_config.dma_high_water);

  tw32(BUFMGR_MODE, BUFMGR_MODE_ENABLE | BUFMGR_MODE_ATTN_ENABLE);
  for (i = 0; i < 2000; i++) {
    if (tr32(BUFMGR_MODE) & BUFMGR_MODE_ENABLE)
      break;
    capros_Sleep_sleep(KR_SLEEP,.10);
  }
  if (i >= 2000) {
    kprintf(KR_OSTREAM,"tg3_reset_hw cannot enable BUFMGR for");
    return -RC_ENODEV;
  }

  tw32(FTQ_RESET, 0xffffffff);
  tw32(FTQ_RESET, 0x00000000);
  for (i = 0; i < 2000; i++) {
    if (tr32(FTQ_RESET) == 0x00000000)
      break;
    capros_Sleep_sleep(KR_SLEEP,.10);
  }
  if (i >= 2000) {
    kprintf(KR_OSTREAM,"tg3_reset_hw cannot reset FTQ %08x",tr32(FTQ_RESET));
    return -RC_ENODEV;
  }

  /* Initialize TG3_BDINFO's at:
   *  RCVDBDI_STD_BD:	standard eth size rx ring
   *  RCVDBDI_JUMBO_BD:	jumbo frame rx ring
   *  RCVDBDI_MINI_BD:	small frame rx ring (??? does not work)
   *
   * like so:
   *  TG3_BDINFO_HOST_ADDR:	high/low parts of DMA address of ring
   *  TG3_BDINFO_MAXLEN_FLAGS:	(rx max buffer size << 16) |
   *                              ring attribute flags
   *  TG3_BDINFO_NIC_ADDR:	location of descriptors in nic SRAM
   *
   * Standard receive ring @ NIC_SRAM_RX_BUFFER_DESC, 512 entries.
   * Jumbo receive ring @ NIC_SRAM_RX_JUMBO_BUFFER_DESC, 256 entries.
   *
   * The size of each ring is fixed in the firmware, but the location is
   * configurable.
   */
  tw32(RCVDBDI_STD_BD + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_HIGH,
       ((uint64_t) tp->rx_std_mapping >> 32));
  tw32(RCVDBDI_STD_BD + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_LOW,
       ((uint64_t) tp->rx_std_mapping & 0xffffffff));
  tw32(RCVDBDI_STD_BD + TG3_BDINFO_MAXLEN_FLAGS,
       RX_STD_MAX_SIZE << BDINFO_FLAGS_MAXLEN_SHIFT);
  tw32(RCVDBDI_STD_BD + TG3_BDINFO_NIC_ADDR,
       NIC_SRAM_RX_BUFFER_DESC);

  tw32(RCVDBDI_MINI_BD + TG3_BDINFO_MAXLEN_FLAGS,
       BDINFO_FLAGS_DISABLED);

  if (tp->tg3_flags & TG3_FLAG_JUMBO_ENABLE) {
    tw32(RCVDBDI_JUMBO_BD + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_HIGH,
	 ((uint64_t) tp->rx_jumbo_mapping >> 32));
    tw32(RCVDBDI_JUMBO_BD + TG3_BDINFO_HOST_ADDR + TG3_64BIT_REG_LOW,
	 ((uint64_t) tp->rx_jumbo_mapping & 0xffffffff));
    tw32(RCVDBDI_JUMBO_BD + TG3_BDINFO_MAXLEN_FLAGS,
	 RX_JUMBO_MAX_SIZE << BDINFO_FLAGS_MAXLEN_SHIFT);
    tw32(RCVDBDI_JUMBO_BD + TG3_BDINFO_NIC_ADDR,
	 NIC_SRAM_RX_JUMBO_BUFFER_DESC);
  } else {
    tw32(RCVDBDI_JUMBO_BD + TG3_BDINFO_MAXLEN_FLAGS,
	 BDINFO_FLAGS_DISABLED);
  }

  /* Setup replenish thresholds. */
  tw32(RCVBDI_STD_THRESH, tp->rx_pending / 8);
  tw32(RCVBDI_JUMBO_THRESH, tp->rx_jumbo_pending / 8);

  /* Clear out send RCB ring in SRAM. */
  for (i = NIC_SRAM_SEND_RCB; i < NIC_SRAM_RCV_RET_RCB; i += TG3_BDINFO_SIZE)
    tg3_write_mem(tp, i + TG3_BDINFO_MAXLEN_FLAGS, BDINFO_FLAGS_DISABLED);

  tp->tx_prod = 0;
  tp->tx_cons = 0;
  tw32_mailbox(MAILBOX_SNDHOST_PROD_IDX_0 + TG3_64BIT_REG_LOW, 0);
  tw32_mailbox(MAILBOX_SNDNIC_PROD_IDX_0 + TG3_64BIT_REG_LOW, 0);
  if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
    tr32(MAILBOX_SNDNIC_PROD_IDX_0 + TG3_64BIT_REG_LOW);

  if (tp->tg3_flags & TG3_FLAG_HOST_TXDS) {
    tg3_set_bdinfo(tp, NIC_SRAM_SEND_RCB,
		   tp->tx_desc_mapping,
		   (TG3_TX_RING_SIZE <<  BDINFO_FLAGS_MAXLEN_SHIFT),
		   NIC_SRAM_TX_BUFFER_DESC);
  } else {
    tg3_set_bdinfo(tp, NIC_SRAM_SEND_RCB,
		   0,
		   BDINFO_FLAGS_DISABLED,
		   NIC_SRAM_TX_BUFFER_DESC);
  }

  for (i= NIC_SRAM_RCV_RET_RCB; i < NIC_SRAM_STATS_BLK; i += TG3_BDINFO_SIZE){
    tg3_write_mem(tp, i + TG3_BDINFO_MAXLEN_FLAGS,
		  BDINFO_FLAGS_DISABLED);
  }

  tp->rx_rcb_ptr = 0;
  tw32_mailbox(MAILBOX_RCVRET_CON_IDX_0 + TG3_64BIT_REG_LOW, 0);
  if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
    tr32(MAILBOX_RCVRET_CON_IDX_0 + TG3_64BIT_REG_LOW);

  tg3_set_bdinfo(tp, NIC_SRAM_RCV_RET_RCB,
		 tp->rx_rcb_mapping,
		 (TG3_RX_RCB_RING_SIZE <<
		  BDINFO_FLAGS_MAXLEN_SHIFT),
		 0);

  tp->rx_std_ptr = tp->rx_pending;
  tw32_mailbox(MAILBOX_RCV_STD_PROD_IDX + TG3_64BIT_REG_LOW,
	       tp->rx_std_ptr);
  if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
    tr32(MAILBOX_RCV_STD_PROD_IDX + TG3_64BIT_REG_LOW);

  if (tp->tg3_flags & TG3_FLAG_JUMBO_ENABLE)
    tp->rx_jumbo_ptr = tp->rx_jumbo_pending;
  else
    tp->rx_jumbo_ptr = 0;
  tw32_mailbox(MAILBOX_RCV_JUMBO_PROD_IDX + TG3_64BIT_REG_LOW,
	       tp->rx_jumbo_ptr);
  if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
    tr32(MAILBOX_RCV_JUMBO_PROD_IDX + TG3_64BIT_REG_LOW);

  /* Initialize MAC address and backoff seed. */
  __tg3_set_mac_addr(tp);

  /* MTU + ethernet header + FCS + optional VLAN tag */
  tw32(MAC_RX_MTU_SIZE, tp->mtu + ETH_HLEN + 8);

  /* The slot time is changed by tg3_setup_phy if we
   * run at gigabit with half duplex.
   */
  tw32(MAC_TX_LENGTHS,
       (2 << TX_LENGTHS_IPG_CRS_SHIFT) |
       (6 << TX_LENGTHS_IPG_SHIFT) |
       (32 << TX_LENGTHS_SLOT_TIME_SHIFT));

  /* Receive rules. */
  tw32(MAC_RCV_RULE_CFG, RCV_RULE_CFG_DEFAULT_CLASS);
  tw32(RCVLPC_CONFIG, 0x0181);

  /* Receive/send statistics. */
  tw32(RCVLPC_STATS_ENABLE, 0xffffff);
  tw32(RCVLPC_STATSCTRL, RCVLPC_STATSCTRL_ENABLE);
  tw32(SNDDATAI_STATSENAB, 0xffffff);
  tw32(SNDDATAI_STATSCTRL,
       (SNDDATAI_SCTRL_ENABLE |
	SNDDATAI_SCTRL_FASTUPD));

  /* Setup host coalescing engine. */
  tw32(HOSTCC_MODE, 0);
  for (i = 0; i < 2000; i++) {
    if (!(tr32(HOSTCC_MODE) & HOSTCC_MODE_ENABLE))
      break;
    capros_Sleep_sleep(KR_SLEEP,.10);
  }

  tw32(HOSTCC_RXCOL_TICKS, 0);
  tw32(HOSTCC_RXMAX_FRAMES, 1);
  tw32(HOSTCC_RXCOAL_TICK_INT, 0);
  tw32(HOSTCC_RXCOAL_MAXF_INT, 1);
  tw32(HOSTCC_TXCOL_TICKS, LOW_TXCOL_TICKS);
  tw32(HOSTCC_TXMAX_FRAMES, LOW_RXMAX_FRAMES);
  tw32(HOSTCC_TXCOAL_TICK_INT, 0);
  tw32(HOSTCC_TXCOAL_MAXF_INT, 0);
  tw32(HOSTCC_STAT_COAL_TICKS,
       DEFAULT_STAT_COAL_TICKS);

  /* Status/statistics block address. */
  tw32(HOSTCC_STATS_BLK_HOST_ADDR + TG3_64BIT_REG_HIGH,
       ((uint64_t) tp->stats_mapping >> 32));
  tw32(HOSTCC_STATS_BLK_HOST_ADDR + TG3_64BIT_REG_LOW,
       ((uint64_t) tp->stats_mapping & 0xffffffff));
  tw32(HOSTCC_STATUS_BLK_HOST_ADDR + TG3_64BIT_REG_HIGH,
       ((uint64_t) tp->status_mapping >> 32));
  tw32(HOSTCC_STATUS_BLK_HOST_ADDR + TG3_64BIT_REG_LOW,
       ((uint64_t) tp->status_mapping & 0xffffffff));
  tw32(HOSTCC_STATS_BLK_NIC_ADDR, NIC_SRAM_STATS_BLK);
  tw32(HOSTCC_STATUS_BLK_NIC_ADDR, NIC_SRAM_STATUS_BLK);

  tw32(HOSTCC_MODE, HOSTCC_MODE_ENABLE | tp->coalesce_mode);

  tw32(RCVCC_MODE, RCVCC_MODE_ENABLE | RCVCC_MODE_ATTN_ENABLE);
  tw32(RCVLPC_MODE, RCVLPC_MODE_ENABLE);
  tw32(RCVLSC_MODE, RCVLSC_MODE_ENABLE | RCVLSC_MODE_ATTN_ENABLE);

  tp->mac_mode = MAC_MODE_TXSTAT_ENABLE | MAC_MODE_RXSTAT_ENABLE |
    MAC_MODE_TDE_ENABLE | MAC_MODE_RDE_ENABLE | MAC_MODE_FHDE_ENABLE;
  tw32(MAC_MODE, tp->mac_mode | MAC_MODE_RXSTAT_CLEAR | MAC_MODE_TXSTAT_CLEAR);
  tr32(MAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  tp->grc_local_ctrl = GRC_LCLCTRL_INT_ON_ATTN | GRC_LCLCTRL_AUTO_SEEPROM;
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700)
    tp->grc_local_ctrl |= (GRC_LCLCTRL_GPIO_OE1 |
			   GRC_LCLCTRL_GPIO_OUTPUT1);
  tw32(GRC_LOCAL_CTRL, tp->grc_local_ctrl);
  tr32(GRC_LOCAL_CTRL);
  capros_Sleep_sleep(KR_SLEEP,0.100);

  tw32_mailbox(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW, 0);
  tr32(MAILBOX_INTERRUPT_0);

  tw32(DMAC_MODE, DMAC_MODE_ENABLE);
  tr32(DMAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  tw32(WDMAC_MODE, (WDMAC_MODE_ENABLE | WDMAC_MODE_TGTABORT_ENAB |
		    WDMAC_MODE_MSTABORT_ENAB | WDMAC_MODE_PARITYERR_ENAB |
		    WDMAC_MODE_ADDROFLOW_ENAB | WDMAC_MODE_FIFOOFLOW_ENAB |
		    WDMAC_MODE_FIFOURUN_ENAB | WDMAC_MODE_FIFOOREAD_ENAB |
		    WDMAC_MODE_LNGREAD_ENAB));
  tr32(WDMAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704 &&
      (tp->tg3_flags & TG3_FLAG_PCIX_MODE)) {
    val = tr32(TG3PCI_X_CAPS);
    val &= ~(PCIX_CAPS_SPLIT_MASK | PCIX_CAPS_BURST_MASK);
    val |= (PCIX_CAPS_MAX_BURST_5704 << PCIX_CAPS_BURST_SHIFT);
    if (tp->tg3_flags & TG3_FLAG_SPLIT_MODE)
      val |= (tp->split_mode_max_reqs <<
	      PCIX_CAPS_SPLIT_SHIFT);
    tw32(TG3PCI_X_CAPS, val);
  }

  val = (RDMAC_MODE_ENABLE | RDMAC_MODE_TGTABORT_ENAB |
	 RDMAC_MODE_MSTABORT_ENAB | RDMAC_MODE_PARITYERR_ENAB |
	 RDMAC_MODE_ADDROFLOW_ENAB | RDMAC_MODE_FIFOOFLOW_ENAB |
	 RDMAC_MODE_FIFOURUN_ENAB | RDMAC_MODE_FIFOOREAD_ENAB |
	 RDMAC_MODE_LNGREAD_ENAB);
  if (tp->tg3_flags & TG3_FLAG_SPLIT_MODE)
    val |= RDMAC_MODE_SPLIT_ENABLE;
  tw32(RDMAC_MODE, val);
  tr32(RDMAC_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  tw32(RCVDCC_MODE, RCVDCC_MODE_ENABLE | RCVDCC_MODE_ATTN_ENABLE);
  tw32(MBFREE_MODE, MBFREE_MODE_ENABLE);
  tw32(SNDDATAC_MODE, SNDDATAC_MODE_ENABLE);
  tw32(SNDBDC_MODE, SNDBDC_MODE_ENABLE | SNDBDC_MODE_ATTN_ENABLE);
  tw32(RCVBDI_MODE, RCVBDI_MODE_ENABLE | RCVBDI_MODE_RCB_ATTN_ENAB);
  tw32(RCVDBDI_MODE, RCVDBDI_MODE_ENABLE | RCVDBDI_MODE_INV_RING_SZ);
  tw32(SNDDATAI_MODE, SNDDATAI_MODE_ENABLE);
  tw32(SNDBDI_MODE, SNDBDI_MODE_ENABLE | SNDBDI_MODE_ATTN_ENABLE);
  tw32(SNDBDS_MODE, SNDBDS_MODE_ENABLE | SNDBDS_MODE_ATTN_ENABLE);

#if 0
  if (tp->pci_chip_rev_id == CHIPREV_ID_5701_A0) {
    err = tg3_load_5701_a0_firmware_fix(tp);
    if (err)
      return err;
  }
#endif

#if TG3_DO_TSO != 0
  err = tg3_load_tso_firmware(tp);
  if (err)
    return err;
#endif

  tp->tx_mode = TX_MODE_ENABLE;
  tw32(MAC_TX_MODE, tp->tx_mode);
  tr32(MAC_TX_MODE);
  capros_Sleep_sleep(KR_SLEEP,0.100);

  tp->rx_mode = RX_MODE_ENABLE;
  tw32(MAC_RX_MODE, tp->rx_mode);
  tr32(MAC_RX_MODE);
  capros_Sleep_sleep(KR_SLEEP,.10);

  if (tp->link_config.phy_is_low_power) {
    tp->link_config.phy_is_low_power = 0;
    tp->link_config.speed = tp->link_config.orig_speed;
    tp->link_config.duplex = tp->link_config.orig_duplex;
    tp->link_config.autoneg = tp->link_config.orig_autoneg;
  }

  tp->mi_mode = MAC_MI_MODE_BASE;
  tw32(MAC_MI_MODE, tp->mi_mode);
  tr32(MAC_MI_MODE);
  capros_Sleep_sleep(KR_SLEEP,.40);

  tw32(MAC_LED_CTRL, 0);
  tw32(MAC_MI_STAT, MAC_MI_STAT_LNKSTAT_ATTN_ENAB);
  tw32(MAC_RX_MODE, RX_MODE_RESET);
  tr32(MAC_RX_MODE);
  capros_Sleep_sleep(KR_SLEEP,.10);
  tw32(MAC_RX_MODE, tp->rx_mode);
  tr32(MAC_RX_MODE);
  capros_Sleep_sleep(KR_SLEEP,.10);

  if (tp->pci_chip_rev_id == CHIPREV_ID_5703_A1)
    tw32(MAC_SERDES_CFG, 0x616000);

  /* Prevent chip from dropping frames when flow control
   * is enabled.
   */
  tw32(MAC_LOW_WMARK_MAX_RX_FRAME, 2);
  tr32(MAC_LOW_WMARK_MAX_RX_FRAME);

  err = tg3_setup_phy(tp);
  if (err)
    return err;

  if (tp->phy_id != PHY_ID_SERDES) {
    uint32_t tmp;

    /* Clear CRC stats. */
    tg3_readphy(tp, 0x1e, &tmp);
    tg3_writephy(tp, 0x1e, tmp | 0x8000);
    tg3_readphy(tp, 0x14, &tmp);
  }

  __tg3_set_rx_mode(tp);

  /* Initialize receive rules. */
  tw32(MAC_RCV_RULE_0,  0xc2000000 & RCV_RULE_DISABLE_MASK);
  tw32(MAC_RCV_VALUE_0, 0xffffffff & RCV_RULE_DISABLE_MASK);
  tw32(MAC_RCV_RULE_1,  0x86000004 & RCV_RULE_DISABLE_MASK);
  tw32(MAC_RCV_VALUE_1, 0xffffffff & RCV_RULE_DISABLE_MASK);
#if 0
  tw32(MAC_RCV_RULE_2,  0); tw32(MAC_RCV_VALUE_2,  0);
  tw32(MAC_RCV_RULE_3,  0); tw32(MAC_RCV_VALUE_3,  0);
#endif
  tw32(MAC_RCV_RULE_4,  0); tw32(MAC_RCV_VALUE_4,  0);
  tw32(MAC_RCV_RULE_5,  0); tw32(MAC_RCV_VALUE_5,  0);
  tw32(MAC_RCV_RULE_6,  0); tw32(MAC_RCV_VALUE_6,  0);
  tw32(MAC_RCV_RULE_7,  0); tw32(MAC_RCV_VALUE_7,  0);
  tw32(MAC_RCV_RULE_8,  0); tw32(MAC_RCV_VALUE_8,  0);
  tw32(MAC_RCV_RULE_9,  0); tw32(MAC_RCV_VALUE_9,  0);
  tw32(MAC_RCV_RULE_10,  0); tw32(MAC_RCV_VALUE_10,  0);
  tw32(MAC_RCV_RULE_11,  0); tw32(MAC_RCV_VALUE_11,  0);
  tw32(MAC_RCV_RULE_12,  0); tw32(MAC_RCV_VALUE_12,  0);
  tw32(MAC_RCV_RULE_13,  0); tw32(MAC_RCV_VALUE_13,  0);
  tw32(MAC_RCV_RULE_14,  0); tw32(MAC_RCV_VALUE_14,  0);
  tw32(MAC_RCV_RULE_15,  0); tw32(MAC_RCV_VALUE_15,  0);

  if (tp->tg3_flags & TG3_FLAG_INIT_COMPLETE)
    tg3_enable_ints(tp);

  return 0;
}

/* Called at device open time to get the chip ready for
 * packet processing.  Invoked with tp->lock held.
 */
int 
tg3_init_hw(struct tg3 *tp)
{
  int err;
  
  /* Force the chip into D0. */
  err = tg3_set_power_state(tp, 0);
  if (err) {
    kprintf(KR_OSTREAM,"Error forcing chip into power DO");
    return err;
  }
  
  tg3_switch_clocks(tp);
  
  tw32(TG3PCI_MEM_WIN_BASE_ADDR, 0);
  
  err = tg3_reset_hw(tp);

  return err;
}

int 
tg3_alloc_consistent(struct tg3 *tp) 
{
  tp->rx_std_buffers = (void *)DMA_START;

  memset(&tp->rx_std_buffers[0], 0, 
	 (sizeof(struct ring_info) *(TG3_RX_RING_SIZE +
				     TG3_RX_JUMBO_RING_SIZE)) +
	 (sizeof(struct tx_ring_info) * TG3_TX_RING_SIZE));

  tp->rx_jumbo_buffers = &tp->rx_std_buffers[TG3_RX_RING_SIZE];
  tp->tx_buffers = (struct tx_ring_info *)
    &tp->rx_jumbo_buffers[TG3_RX_JUMBO_RING_SIZE];

  tp->rx_std   = (void *)desc_start;
  tp->rx_std_mapping = virt_to_bus((uint32_t)&tp->rx_std[0]);
    
  tp->rx_jumbo = (void *)((uint32_t)&tp->rx_std[0] + TG3_RX_RING_BYTES);
  tp->rx_jumbo_mapping = virt_to_bus((uint32_t)&tp->rx_jumbo[0]);
  
  tp->rx_rcb   = (void *)((uint32_t)&tp->rx_jumbo[0]+TG3_RX_JUMBO_RING_BYTES);
  tp->rx_rcb_mapping = virt_to_bus((uint32_t)&tp->rx_rcb[0]);
  

  tp->tx_ring  = (void *)((uint32_t)&tp->rx_rcb[0] +  TG3_RX_RCB_RING_BYTES);
  tp->tx_desc_mapping = virt_to_bus((uint32_t)&tp->tx_ring[0]);
  
  tp->hw_status = (void *)((uint32_t)&tp->tx_ring[0] + TG3_TX_RING_BYTES);
  tp->status_mapping = virt_to_bus((uint32_t)&tp->hw_status[0]);
  
  tp->hw_stats = (void *)((uint32_t)&tp->hw_status[0]+ TG3_HW_STATUS_SIZE);
  tp->stats_mapping = virt_to_bus((uint32_t)&tp->hw_stats[0]);

  memset(&tp->hw_status[0], 0, TG3_HW_STATUS_SIZE);
  memset(&tp->hw_stats[0], 0, sizeof(struct tg3_hw_stats));

  return RC_OK;
}

static int 
tg3_open(struct tg3 *tp)
{
  int err;
  
  tg3_disable_ints(tp);
  tp->tg3_flags &= ~TG3_FLAG_INIT_COMPLETE;

  /* If you move this call, make sure TG3_FLAG_HOST_TXDS in
   * tp->tg3_flags is accurate at that new place. */
  err = tg3_alloc_consistent(tp);
  if (err)
    return err;

  tg3_init_rings(tp);
  
  err = tg3_init_hw(tp);
  if (err) {
    kprintf(KR_OSTREAM,"HW initialization ... [FAILED]");
    return 1;
  } else {
    tp->tg3_flags |= TG3_FLAG_INIT_COMPLETE;
  }

  tg3_enable_ints(tp);
  
  tp->tg3_flags |= TG3_FLAG_INIT_COMPLETE;
  return 0;
}


/* Tigon3 never reports partial packet sends.  So we do not
 * need special logic to handle SKBs that have not had all
 * of their frags sent yet, like SunGEM does.
 */
static void 
tg3_tx(struct tg3 *tp)
{
  uint32_t hw_idx = tp->hw_status->idx[0].tx_consumer;
  uint32_t sw_idx = tp->tx_cons;
  
  sw_idx = hw_idx;
  tp->tx_cons = sw_idx;
  
}


/* We only need to move over in the address because the other
 * members of the RX descriptor are invariant.  See notes above
 * tg3_alloc_rx_skb for full details.
 */
static void 
tg3_recycle_rx(struct tg3 *tp, uint32_t opaque_key,
	       int src_idx, uint32_t dest_idx_unmasked)
{
  struct tg3_rx_buffer_desc *src_desc, *dest_desc;
  struct ring_info *src_map, *dest_map;
  int dest_idx;
  switch (opaque_key) {
  case RXD_OPAQUE_RING_STD:
    dest_idx = dest_idx_unmasked % TG3_RX_RING_SIZE;
    dest_desc = &tp->rx_std[dest_idx];
    dest_map = &tp->rx_std_buffers[dest_idx];
    src_desc = &tp->rx_std[src_idx];
    src_map = &tp->rx_std_buffers[src_idx];
    break;
  case RXD_OPAQUE_RING_JUMBO:
    dest_idx = dest_idx_unmasked % TG3_RX_JUMBO_RING_SIZE;
    dest_desc = &tp->rx_jumbo[dest_idx];
    dest_map = &tp->rx_jumbo_buffers[dest_idx];
    src_desc = &tp->rx_jumbo[src_idx];
    src_map = &tp->rx_jumbo_buffers[src_idx];
    break;
    
  default:
    return;
  };
  dest_map->skb = src_map->skb;
  dest_desc->addr_hi = src_desc->addr_hi;
  dest_desc->addr_lo = src_desc->addr_lo;
  
  src_map->skb = NULL;
}


/* The RX ring scheme is composed of multiple rings which post fresh
 * buffers to the chip, and one special ring the chip uses to report
 * status back to the host.
 *
 * The special ring reports the status of received packets to the
 * host.  The chip does not write into the original descriptor the
 * RX buffer was obtained from.  The chip simply takes the original
 * descriptor as provided by the host, updates the status and length
 * field, then writes this into the next status ring entry.
 *
 * Each ring the host uses to post buffers to the chip is described
 * by a TG3_BDINFO entry in the chips SRAM area.  When a packet arrives,
 * it is first placed into the on-chip ram.  When the packet's length
 * is known, it walks down the TG3_BDINFO entries to select the ring.
 * Each TG3_BDINFO specifies a MAXLEN field and the first TG3_BDINFO
 * which is within the range of the new packet's length is chosen.
 *
 * The "seperate ring for rx status" scheme may sound queer, but it makes
 * sense from a cache coherency perspective.  If only the host writes
 * to the buffer post rings, and only the chip writes to the rx status
 * rings, then cache lines never move beyond shared-modified state.
 * If both the host and chip were to write into the same ring, cache line
 * eviction could occur since both entities want it in an exclusive state.
 */
static int 
tg3_rx(struct tg3 *tp)
{
  uint32_t work_mask;
  uint32_t rx_rcb_ptr = tp->rx_rcb_ptr;
  uint16_t hw_idx, sw_idx;
  int received;
  
  hw_idx = tp->hw_status->idx[0].rx_producer;
  sw_idx = rx_rcb_ptr % TG3_RX_RCB_RING_SIZE;
  work_mask = 0;
  received = 0;
  while (sw_idx != hw_idx) {
    struct tg3_rx_buffer_desc *desc = &tp->rx_rcb[sw_idx];
    unsigned int len;
    void *skb;
    uint32_t opaque_key, desc_idx, *post_ptr;
    desc_idx = desc->opaque & RXD_OPAQUE_INDEX_MASK;
    opaque_key = desc->opaque & RXD_OPAQUE_RING_MASK;
    if (opaque_key == RXD_OPAQUE_RING_STD) {
      skb = tp->rx_std_buffers[desc_idx].skb;
      post_ptr = &tp->rx_std_ptr;
    } else if (opaque_key == RXD_OPAQUE_RING_JUMBO) {
      skb = tp->rx_jumbo_buffers[desc_idx].skb;
      post_ptr = &tp->rx_jumbo_ptr;
    }
    else {
      goto next_pkt_nopost;
    }
    
    work_mask |= opaque_key;
    if ((desc->err_vlan & RXD_ERR_MASK) != 0 &&
	(desc->err_vlan != RXD_ERR_ODD_NIBBLE_RCVD_MII)) {
      tg3_recycle_rx(tp, opaque_key,
		     desc_idx, *post_ptr);
      /* Other statistics kept track of by card. */
      tp->net_stats.rx_dropped++;
      goto next_pkt;
    }
    
    /* omit crc */
    len = ((desc->idx_len & RXD_LEN_MASK) >> RXD_LEN_SHIFT) - 4; 
    kprintf(KR_OSTREAM,"Len = %d",len);

#if 0    
    if ((tp->tg3_flags & TG3_FLAG_RX_CHECKSUMS) &&
	(desc->type_flags & RXD_FLAG_TCPUDP_CSUM) &&
	(((desc->ip_tcp_csum & RXD_TCPCSUM_MASK)
	  >> RXD_TCPCSUM_SHIFT) == 0xffff))
      skb->ip_summed = CHECKSUM_UNNECESSARY;
    else
      skb->ip_summed = CHECKSUM_NONE;
    skb->protocol = eth_type_trans(skb, tp->dev);
#if TG3_VLAN_TAG_USED
    if (tp->vlgrp != NULL &&
	desc->type_flags & RXD_FLAG_VLAN) {
      tg3_vlan_rx(tp, skb,
		  desc->err_vlan & RXD_VLAN_MASK);
    } else
#endif
      netif_receive_skb(skb);
#endif    

    received++;
    
  next_pkt:
    (*post_ptr)++;
  next_pkt_nopost:
    rx_rcb_ptr++;
    sw_idx = rx_rcb_ptr % TG3_RX_RCB_RING_SIZE;
  }
  /* ACK the status ring. */
  tp->rx_rcb_ptr = rx_rcb_ptr;
  tw32_mailbox(MAILBOX_RCVRET_CON_IDX_0 + TG3_64BIT_REG_LOW,
	       (rx_rcb_ptr % TG3_RX_RCB_RING_SIZE));
  if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
    tr32(MAILBOX_RCVRET_CON_IDX_0 + TG3_64BIT_REG_LOW);
  
  /* Refill RX ring(s). */
  if (work_mask & RXD_OPAQUE_RING_STD) {
    sw_idx = tp->rx_std_ptr % TG3_RX_RING_SIZE;
    tw32_mailbox(MAILBOX_RCV_STD_PROD_IDX + TG3_64BIT_REG_LOW,
		 sw_idx);
    if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
      tr32(MAILBOX_RCV_STD_PROD_IDX + TG3_64BIT_REG_LOW);
  }
  if (work_mask & RXD_OPAQUE_RING_JUMBO) {
    sw_idx = tp->rx_jumbo_ptr % TG3_RX_JUMBO_RING_SIZE;
    tw32_mailbox(MAILBOX_RCV_JUMBO_PROD_IDX + TG3_64BIT_REG_LOW,
		 sw_idx);
    if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
      tr32(MAILBOX_RCV_JUMBO_PROD_IDX + TG3_64BIT_REG_LOW);
  }
  
  return received;
}


static inline 
unsigned int tg3_has_work(struct tg3 *tp)
{
  struct tg3_hw_status *sblk = tp->hw_status;
  unsigned int work_exists = 0;
  
  /* check for phy events */
  if (!(tp->tg3_flags &
	(TG3_FLAG_USE_LINKCHG_REG |
	 TG3_FLAG_POLL_SERDES))) {
    if (sblk->status & SD_STATUS_LINK_CHG) {
      kprintf(KR_OSTREAM,"Link XCHG");
      tg3_setup_phy(tp);
      work_exists = 1;
    }
  }
  /* check for RX/TX work to do */
  if (sblk->idx[0].tx_consumer != tp->tx_cons) {
    tg3_tx(tp);
    work_exists = 1;
  }
  if( sblk->idx[0].rx_producer != tp->rx_rcb_ptr) {
    tg3_rx(tp);
    work_exists = 1;
  }
  return work_exists;
}

static void 
tg3_interrupt(struct tg3 *tp)
{
  struct tg3_hw_status *sblk = tp->hw_status;

  /*DEBUG_ALTIMA*/ kprintf(KR_OSTREAM,"tg3_interrupt %d ",sblk->status);
  if (sblk->status & SD_STATUS_UPDATED) {
    /* writing any value to intr-mbox-0 clears PCI INTA# and
     * chip-internal interrupt pending events.
     * writing non-zero to intr-mbox-0 additional tells the
     * NIC to stop sending us irqs, engaging "in-intr-handler"
     * event coalescing. */
    tw32_mailbox(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW,
		 0x00000001);
    /* Flush PCI write.  This also guarantees that our
     * status block has been flushed to host memory. */
    tr32(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW);
    sblk->status &= ~SD_STATUS_UPDATED;

    tg3_has_work(tp);
        
    tg3_enable_ints(tp);
  } else {
    /* no work, shared interrupt perhaps?  re-enable
     * interrupts, and flush that PCI write */
    tw32_mailbox(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW,0x00000000);
    tr32(MAILBOX_INTERRUPT_0 + TG3_64BIT_REG_LOW);
  }
}

static inline void 
tg3_set_txd(struct tg3 *tp, int entry,
	    dma_addr_t mapping, int len, uint32_t flags,
	    uint32_t mss_and_is_end)
{
  int is_end = (mss_and_is_end & 0x1);
  uint32_t mss = (mss_and_is_end >> 1);
  uint32_t vlan_tag = 0;
  
  if (is_end)
    flags |= TXD_FLAG_END;
  if (flags & TXD_FLAG_VLAN) {
    vlan_tag = flags >> 16;
    flags &= 0xffff;
  }
  vlan_tag |= (mss << TXD_MSS_SHIFT);
  if (tp->tg3_flags & TG3_FLAG_HOST_TXDS) {
    struct tg3_tx_buffer_desc *txd = &tp->tx_ring[entry];

    txd->addr_hi = ((uint64_t) mapping >> 32);
    txd->addr_lo = ((uint64_t) mapping & 0xffffffff);
    txd->len_flags = (len << TXD_LEN_SHIFT) | flags;
    txd->vlan_tag = vlan_tag << TXD_VLAN_TAG_SHIFT;
  } else {
    struct tx_ring_info *txr = &tp->tx_buffers[entry];
    unsigned long txd;

    txd = (tp->regs +
	   NIC_SRAM_WIN_BASE +
	   NIC_SRAM_TX_BUFFER_DESC);
    txd += (entry * TXD_SIZE);

    /* Save some PIOs */
    if (sizeof(dma_addr_t) != sizeof(uint32_t))
      writel(((uint64_t) mapping >> 32),
	     txd + TXD_ADDR + TG3_64BIT_REG_HIGH);

    writel(((uint64_t) mapping & 0xffffffff),
	   txd + TXD_ADDR + TG3_64BIT_REG_LOW);
    writel(len << TXD_LEN_SHIFT | flags, txd + TXD_LEN_FLAGS);
    if (txr->prev_vlan_tag != vlan_tag) {
      writel(vlan_tag << TXD_VLAN_TAG_SHIFT, txd + TXD_VLAN_TAG);
      txr->prev_vlan_tag = vlan_tag;
    }
  }
}

static int 
tg3_start_xmit(struct tg3 *tp,uint8_t *dest,int type,int size, char *pkt)
{
  uint32_t mapping;
  uint32_t entry, base_flags, mss;

  /* This is a hard error, log it. */
  if (TX_BUFFS_AVAIL(tp) <= 1) {
    //netif_stop_queue(dev);
    kprintf(KR_OSTREAM,"tg3_start_xmit::BUG! Tx Ring full when queue awake!");
    return 1;
  }
  
  entry = tp->tx_prod;
  base_flags = 0;
  base_flags |= TXD_FLAG_TCPUDP_CSUM;
  mss = 0;
  
  /* Our standard place where we put all our transmit data */
  mapping = 0x17000;
  memcpy((void *)(DMA_START + mapping),pkt,size);
  
  //tp->tx_buffers[entry].skb = skb;
  tg3_set_txd(tp, entry, PHYSADDR + mapping,size, base_flags,1);
  
  entry = NEXT_TX(entry);

  /* Packets are ready, update Tx producer idx local and on card.
   * We know this is not a 5700 (by virtue of not being a chip
   * requiring the 4GB overflow workaround) so we can safely omit
   * the double-write bug tests.
   */
  if (tp->tg3_flags & TG3_FLAG_HOST_TXDS) {
    tw32_mailbox((MAILBOX_SNDHOST_PROD_IDX_0 +
		  TG3_64BIT_REG_LOW), entry);
    if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
      tr32(MAILBOX_SNDHOST_PROD_IDX_0 +
	   TG3_64BIT_REG_LOW);
  } else {
    /* First, make sure tg3 sees last descriptor fully
     * in SRAM. */
    if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
      tr32(MAILBOX_SNDNIC_PROD_IDX_0 + TG3_64BIT_REG_LOW);
    
    tw32_mailbox((MAILBOX_SNDNIC_PROD_IDX_0 + TG3_64BIT_REG_LOW), entry);
    /* Now post the mailbox write itself.  */
    if (tp->tg3_flags & TG3_FLAG_MBOX_WRITE_REORDER)
      tr32(MAILBOX_SNDNIC_PROD_IDX_0 + TG3_64BIT_REG_LOW);
  }

  tp->tx_prod = entry;
  if (TX_BUFFS_AVAIL(tp) <= (MAX_SKB_FRAGS + 1)) {
    kprintf(KR_OSTREAM,"tg3_start_xmit:: bufs available insufficient");
    //netif_stop_queue(dev);
  }
  return 0;
}

static int 
tg3_do_test_dma(struct tg3 *tp, uint32_t *buf, uint32_t buf_dma, 
		int size, int to_device)
{
  struct tg3_internal_buffer_desc test_desc;
  uint32_t sram_dma_descs;
  int i, ret;
  
  sram_dma_descs = NIC_SRAM_DMA_DESC_POOL_BASE;
  
  tw32(FTQ_RCVBD_COMP_FIFO_ENQDEQ, 0);
  tw32(FTQ_RCVDATA_COMP_FIFO_ENQDEQ, 0);
  tw32(RDMAC_STATUS, 0);
  tw32(WDMAC_STATUS, 0);
  
  tw32(BUFMGR_MODE, 0);
  tw32(FTQ_RESET, 0);
  /* pci_alloc_consistent gives only non-DAC addresses */
  test_desc.addr_hi = 0;
  test_desc.addr_lo = buf_dma & 0xffffffff;
  test_desc.nic_mbuf = 0x00002100;
  test_desc.len = size;
  if (to_device) {
    test_desc.cqid_sqid = (13 << 8) | 2;
    tw32(RDMAC_MODE, RDMAC_MODE_RESET);
    tr32(RDMAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,40);
    
    tw32(RDMAC_MODE, RDMAC_MODE_ENABLE);
    tr32(RDMAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,40);
  } else {
    test_desc.cqid_sqid = (16 << 8) | 7;
    tw32(WDMAC_MODE, WDMAC_MODE_RESET);
    tr32(WDMAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,40);
	  
    tw32(WDMAC_MODE, WDMAC_MODE_ENABLE);
    tr32(WDMAC_MODE);
    capros_Sleep_sleep(KR_SLEEP,40);
  }
  test_desc.flags = 0x00000004;
  for (i = 0; i < (sizeof(test_desc) / sizeof(uint32_t)); i++) {
    uint32_t val;

    val = *(((uint32_t *)&test_desc) + i);
    pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
				TG3PCI_MEM_WIN_BASE_ADDR,
				sram_dma_descs + (i * sizeof(uint32_t)));
    pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
				TG3PCI_MEM_WIN_DATA, val);
  }
  pciprobe_write_config_dword(KR_PCI_PROBE_C,tp->pdev, 
			      TG3PCI_MEM_WIN_BASE_ADDR, 0);
  
  if (to_device) {
    tw32(FTQ_DMA_HIGH_READ_FIFO_ENQDEQ, sram_dma_descs);
  } else {
    tw32(FTQ_DMA_HIGH_WRITE_FIFO_ENQDEQ, sram_dma_descs);
  }
  
  ret = -RC_ENODEV;
  for (i = 0; i < 40; i++) {
    uint32_t val;
    if (to_device)
      val = tr32(FTQ_RCVBD_COMP_FIFO_ENQDEQ);
    else
      val = tr32(FTQ_RCVDATA_COMP_FIFO_ENQDEQ);
    if ((val & 0xffff) == sram_dma_descs) {
      ret = 0;
      break;
    }
    
    capros_Sleep_sleep(KR_SLEEP,0.100);
  }
  
  return ret;
}

#define TEST_BUFFER_SIZE        0x40
int 
tg3_test_dma(struct tg3 *tp)
{
  uint32_t buf_dma;
  uint32_t *buf;
  int ret;
  
  buf = (void *)0x80017000;
  buf_dma = 0x80017000 - 0x80000000 + PHYSADDR;
  tw32(TG3PCI_CLOCK_CTRL, 0);
  
  if ((tp->tg3_flags & TG3_FLAG_PCIX_MODE) == 0) {
    tp->dma_rwctrl =
      (0x7 << DMA_RWCTRL_PCI_WRITE_CMD_SHIFT) |
      (0x6 << DMA_RWCTRL_PCI_READ_CMD_SHIFT) |
      (0x7 << DMA_RWCTRL_WRITE_WATER_SHIFT) |
      (0x7 << DMA_RWCTRL_READ_WATER_SHIFT) |
      (0x0f << DMA_RWCTRL_MIN_DMA_SHIFT);
    /* XXX 5705 note: set MIN_DMA to zero here */
  } else {
    if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704)
      tp->dma_rwctrl =
	(0x7 << DMA_RWCTRL_PCI_WRITE_CMD_SHIFT) |
	(0x6 << DMA_RWCTRL_PCI_READ_CMD_SHIFT) |
	(0x3 << DMA_RWCTRL_WRITE_WATER_SHIFT) |
	(0x7 << DMA_RWCTRL_READ_WATER_SHIFT) |
	(0x00 << DMA_RWCTRL_MIN_DMA_SHIFT);
    else
      tp->dma_rwctrl =
	(0x7 << DMA_RWCTRL_PCI_WRITE_CMD_SHIFT) |
	(0x6 << DMA_RWCTRL_PCI_READ_CMD_SHIFT) |
	(0x3 << DMA_RWCTRL_WRITE_WATER_SHIFT) |
	(0x3 << DMA_RWCTRL_READ_WATER_SHIFT) |
	(0x0f << DMA_RWCTRL_MIN_DMA_SHIFT);
    
    /* Wheee, some more chip bugs... */
    if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
	GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704) {
      uint32_t ccval = (tr32(TG3PCI_CLOCK_CTRL) & 0x1f);
      
      if (ccval == 0x6 || ccval == 0x7)
	tp->dma_rwctrl |= DMA_RWCTRL_ONE_DMA;
    }
  }
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5703 ||
      GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5704)
    tp->dma_rwctrl &= ~(DMA_RWCTRL_MIN_DMA
			<< DMA_RWCTRL_MIN_DMA_SHIFT);
  
  if (GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5700 ||
      GET_ASIC_REV(tp->pci_chip_rev_id) == ASIC_REV_5701) {
    /* Remove this if it causes problems for some boards. */
    tp->dma_rwctrl |= DMA_RWCTRL_USE_MEM_READ_MULT;
  }
  
  tw32(TG3PCI_DMA_RW_CTRL, tp->dma_rwctrl);
  
  ret = 0;
  if (GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5700 &&
      GET_ASIC_REV(tp->pci_chip_rev_id) != ASIC_REV_5701)
    goto out;
  
  while (1) {
    uint32_t *p, i;
    
    p = buf;
    for (i = 0; i < TEST_BUFFER_SIZE / sizeof(uint32_t); i++)
      p[i] = i;
    
    /* Send the buffer to the chip. */
    ret = tg3_do_test_dma(tp, buf, buf_dma, TEST_BUFFER_SIZE, 1);
    if (ret)
      break;
    
    p = buf;
    for (i = 0; i < TEST_BUFFER_SIZE / sizeof(uint32_t); i++)
      p[i] = 0;
    
    /* Now read it back. */
    ret = tg3_do_test_dma(tp, buf, buf_dma, TEST_BUFFER_SIZE, 0);
    if (ret)
      break;
    /* Verify it. */
    p = buf;
    for (i = 0; i < TEST_BUFFER_SIZE / sizeof(uint32_t); i++) {
      if (p[i] == i)
	continue;

      if ((tp->dma_rwctrl & DMA_RWCTRL_WRITE_BNDRY_MASK) ==
	  DMA_RWCTRL_WRITE_BNDRY_DISAB) {
	tp->dma_rwctrl |= DMA_RWCTRL_WRITE_BNDRY_16;
	tw32(TG3PCI_DMA_RW_CTRL, tp->dma_rwctrl);
	break;
      } else {
	ret = -RC_ENODEV;
	goto out;
      }
    }
    
    if (i == (TEST_BUFFER_SIZE / sizeof(uint32_t))) {
      /* Success. */
      ret = 0;
      break;
    }
  }
  
 out:
  return ret;
}

static char * tg3_phy_string(struct tg3 *tp)
{
  switch (tp->phy_id & PHY_ID_MASK) {
  case PHY_ID_BCM5400:    return "5400";
  case PHY_ID_BCM5401:    return "5401";
  case PHY_ID_BCM5411:    return "5411";
  case PHY_ID_BCM5701:    return "5701";
  case PHY_ID_BCM5703:    return "5703";
  case PHY_ID_BCM5704:    return "5704";
  case PHY_ID_BCM8002:    return "8002";
  case PHY_ID_SERDES:     return "serdes";
  default:                return "unknown";
  };
}

static inline 
void tg3_set_mtu(struct tg3 *tp,
		 int new_mtu)
{
  tp->mtu = new_mtu;
  if (new_mtu > ETH_DATA_LEN)
    tp->tg3_flags |= TG3_FLAG_JUMBO_ENABLE;
  else
    tp->tg3_flags &= ~TG3_FLAG_JUMBO_ENABLE;
}

/* Initialize the pci_probe domain, do a probe and search for 3c905c card
 * using vendor and device ids */
int
altima_probe()
{
  result_t result;
  uint32_t total;
  unsigned short vendor = VENDOR_ALTIMA_AC9100;
  struct tg3 *tp = &TP;
  int i;
  
  /* Construct an instance of the pci probe domain */
  result = constructor_request(KR_PCI_PROBE_C,KR_BANK,KR_SCHED,
			       KR_VOID,KR_PCI_PROBE_C);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  /* Initialize the probe */
  result = pciprobe_initialize(KR_PCI_PROBE_C);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  DEBUG_ALTIMA kprintf(KR_OSTREAM,"PciProbe init ...  %s.\n",
		       (result == RC_OK) ? "SUCCESS" : "FAILED");

  /* Find all the devices of the vendor */
  result = pciprobe_vendor_total(KR_PCI_PROBE_C, vendor, &total);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  DEBUG_ALTIMA  kprintf(KR_OSTREAM,"No of Altima Devices found = %d",total);
  
  result = pciprobe_vendor_next(KR_PCI_PROBE_C, vendor,0,&NETDEV);
  if(result!=RC_OK) return RC_PCI_PROBE_ERROR;
  
  DEBUG_ALTIMA {
    kprintf(KR_OSTREAM,"Pci_Find_VendorID ... %s",
	    (result == RC_OK) ? "SUCCESS" : "FAILED");
    kprintf(KR_OSTREAM,"RCV:00:%02x [%04x/%04x] [%04x/%04x] BR[0] %x IRQ %d,MASTER=%s bus = %x\n", 
	    NETDEV.devfn, NETDEV.subsystem_vendor,
	    NETDEV.subsystem_device, NETDEV.vendor,
	    NETDEV.device,NETDEV.base_address[0],NETDEV.irq,
	    NETDEV.master?"YES":"NO",NETDEV.busnumber);
  }
  
  NETDEV.base_address[0] -= 4;

  /* FIX:: Should use the range allocation manager. */
  /* FIX: ask Shap about this -- Merge Bug??? at 0x5000000u */
  for(i=0x1000000u;i>0;i+=DMA_SIZE) {
    /* Hopefully we can do DMA onto this RAM area */
    result = capros_DevPrivs_publishMem(KR_DEVPRIVS,i,i+DMA_SIZE, 0);
    if(result==RC_OK) {
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
  
  tp->pdev = &NETDEV;
  { 
    uint16_t pci_cmd;
    int pm_cap;
    
    /* Force memory write invalidate off.  If we leave it on,
     * then on 5700_BX chips we have to enable a workaround.
     * The workaround is to set the TG3PCI_DMA_RW_CTRL boundry
     * to match the cacheline size.  The Broadcom driver have this
     * workaround but turns MWI off all the times so never uses
     * it.  This seems to suggest that the workaround is insufficient.
     */
    pciprobe_read_config_word(KR_PCI_PROBE_C,tp->pdev,PCI_COMMAND,&pci_cmd);
    pci_cmd &= ~PCI_COMMAND_INVALIDATE;
    pciprobe_write_config_word(KR_PCI_PROBE_C,tp->pdev,PCI_COMMAND,pci_cmd);

    pm_cap = pci_find_capability(tp->pdev, PCI_CAP_ID_PM);
    if (pm_cap == 0) {
      kprintf(KR_OSTREAM,"Cannot find PowerManagement cap aborting.\n");
      return 1;
    }else {
      DEBUG_ALTIMA kprintf(KR_OSTREAM,"Power Management capability found");
      tp->pm_cap = pm_cap;
    }
  }

  result = constructor_request(KR_MEMMAP_C,KR_BANK,KR_SCHED,KR_VOID,KR_REGS);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Constructing Memmap --- FAILED(%d)",result);
    return result;
  }
  
  /* Publish the tg3 memory space */
  result = capros_DevPrivs_publishMem(KR_DEVPRIVS,NETDEV.base_address[0], 
				    NETDEV.base_address[0]+0x10000, 0);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"tg3::publish mem--Failed %x",result);

  /* Map this space into our address space */
  result = memmap_map(KR_REGS,KR_PHYSRANGE,
		      NETDEV.base_address[0],0x10000,&regs_lss);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Memmaping --- FAILED(%d)",result);
    return result;
  }
  
  result = constructor_request(KR_MEMMAP_C,KR_BANK,KR_SCHED,KR_VOID,KR_REGS);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Constructing Memmap --- FAILED(%d)",result);
    return result;
  }
  
  /* Publish the tg3 memory space */
  result = capros_DevPrivs_publishMem(KR_DEVPRIVS,NETDEV.base_address[0], 
				    NETDEV.base_address[0]+0x10000, 0);
  if(result!=RC_OK) kprintf(KR_OSTREAM,"tg3::publish mem--Failed %x",result);

  /* Map this space into our address space */
  result = memmap_map(KR_REGS,KR_PHYSRANGE,
		      NETDEV.base_address[0],0x10000,&regs_lss);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Memmaping --- FAILED(%d)",result);
    return result;
  }
  kprintf(KR_OSTREAM,"regs_lss =  %d",regs_lss);

  /* Patch up our addresspace to reflect the new mapping and 
   * touch all pages to avoid page faulting later */
  patch_addrspace();
  init_mapped_memory(dma_abszero,DMA_SIZE);
  
  /* Now our tg3 pci_dev_data pointer is set right */
  tp->regs = REGS_START;
  tp->mac_mode = TG3_DEF_MAC_MODE;
  tp->rx_mode = TG3_DEF_RX_MODE;
  tp->tx_mode = TG3_DEF_TX_MODE;
  tp->mi_mode = MAC_MI_MODE_BASE;
  
  /* The word/byte swap controls here control register access byte
   * swapping.  DMA data byte swapping is controlled in the GRC_MODE
   * setting below.*/
  tp->misc_host_ctrl =
    MISC_HOST_CTRL_MASK_PCI_INT |
    MISC_HOST_CTRL_WORD_SWAP |
    MISC_HOST_CTRL_INDIR_ACCESS |
    MISC_HOST_CTRL_PCISTATE_RW;
  /* The NONFRM (non-frame) byte/word swap controls take effect
   * on descriptor entries, anything which isn't packet data.
   *
   * The StrongARM chips on the board (one for tx, one for rx)
   * are running in big-endian mode.
   */
  tp->grc_mode = (GRC_MODE_WSWAP_DATA | GRC_MODE_BSWAP_DATA |
		  GRC_MODE_WSWAP_NONFRM_DATA);
  
  tg3_init_link_config(tp);
  tg3_init_bufmgr_config(tp);
  
  tp->rx_pending = TG3_DEF_RX_RING_PENDING;
  tp->rx_jumbo_pending = TG3_DEF_RX_JUMBO_RING_PENDING;
  tp->tx_pending = TG3_DEF_TX_RING_PENDING;
  
  result = tg3_get_invariants(tp);
  if(result!=RC_OK) {
    kprintf(KR_OSTREAM,"Tg3:Problem fetching invariants of chip");
    return 1;
  }

  result = tg3_get_device_address(tp);
  if (result!=RC_OK) {
    kprintf(KR_OSTREAM,"Tg3:Could not obtain valid ethernet address");
    return 1;
  }
  pci_save_state(tp->pdev, tp->pci_cfg_state);

  /* Allocate the IRQ in the pci device structure */
  result = capros_DevPrivs_allocIRQ(KR_DEVPRIVS,NETDEV.irq, 0);
  if(result != RC_OK) {
    kprintf(KR_OSTREAM,"IRQ %d not allocated",NETDEV.irq);
    return RC_IRQ_ALLOC_FAILED;
  }else   
    DEBUG_ALTIMA 
      kprintf(KR_OSTREAM,"Allocating IRQ %d ... [SUCCESS]",NETDEV.irq);
  
  /* Start the helper process which waits on the pci irq for us and 
   * signals us of irq arrival */
  if((result=StartHelper())!= RC_OK) {
    kprintf(KR_OSTREAM,"Starting Helper ... [FAILED %d]",result);
    return result;
  }

  tg3_set_mtu(tp,9000);

  result = tg3_test_dma(tp);
  if (result) {
    kprintf(KR_OSTREAM,"DMA engine test failed, aborting");
    return 1;
  }else {
    kprintf(KR_OSTREAM,"testing DMA engine ... [SUCCESS]");
  }
  
  if ((tp->tg3_flags & TG3_FLAG_BROKEN_CHECKSUMS) == 0) {
    tp->tg3_flags |= TG3_FLAG_RX_CHECKSUMS;
  } else
    tp->tg3_flags &= ~TG3_FLAG_RX_CHECKSUMS;

  result = tg3_open(tp);
  if (result!=RC_OK) {
    kprintf(KR_OSTREAM,"Altima probe:Could not open tg3 for use");
    return 1;
  }


  
  kprintf(KR_OSTREAM,"Tigon3 [partno(%s) rev %04x PHY(%s)] (PCI%s:%s:%s)"
" %sBaseT Ethernet, mtu = %d ",
	  tp->board_part_number,
	  tp->pci_chip_rev_id,
	  tg3_phy_string(tp),
	  ((tp->tg3_flags & TG3_FLAG_PCIX_MODE) ? "X" : ""),
	  ((tp->tg3_flags & TG3_FLAG_PCI_HIGH_SPEED) ?
	   ((tp->tg3_flags & TG3_FLAG_PCIX_MODE) ? "133MHz" : "66MHz") :
	   ((tp->tg3_flags & TG3_FLAG_PCIX_MODE) ? "100MHz" : "33MHz")),
	  ((tp->tg3_flags & TG3_FLAG_PCI_32BIT) ? "32-bit" : "64-bit"),
	  (tp->tg3_flags & TG3_FLAG_10_100_ONLY) ? "10/100" : "10/100/1000",
	  tp->mtu);

  kprintf(KR_OSTREAM, "MAC Addr %02x:%02x:%02x:%02x:%02x:%02x\n",
	  eaddr[0],eaddr[1],eaddr[2],eaddr[3],eaddr[4],eaddr[5]);
  
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
      DEBUG_ALTIMA kprintf(KR_OSTREAM,"irq arrived");
      tg3_interrupt(&TP);
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
      
      msg->snd_code = tg3_start_xmit(&TP,dest,type,msg->rcv_sent,rcv_buffer);
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
  capros_Process_setIOSpace(KR_SELF, KR_DEVPRIVS);
  capros_Process_makeStartKey(KR_SELF, 0, KR_START);

  /* Probe for an altima type network card, if it exists initialize it
   * and return a start key . */
  memset(&msg,0,sizeof(msg));
  msg.snd_invKey = KR_RETURN;
  msg.snd_key0   = KR_START;
  msg.snd_rsmkey = KR_RETURN;

  msg.snd_code = altima_probe();
  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_data = rcv_buffer;
  msg.rcv_limit  = sizeof(rcv_buffer);
  do {
    msg.snd_invKey = KR_RETURN;
    msg.snd_rsmkey = KR_RETURN;
    RETURN(&msg);
  } while (ProcessRequest(&msg));

  return 0;
}
       
  
