/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>

#include <idl/capros/DevPrivs.h>
#include <idl/capros/Node.h>
#include <idl/capros/Process.h>
#include <idl/capros/arch/i386/Process.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <domain/drivers/PciProbeKey.h>

#include "constituents.h"

#include "malloc.h"

#include "pci_con.h"
#include "pci_i386.h"
#include "pci_ids.h"
#include "pci_ioport.h"

#define KR_OSTREAM   KR_APP(0)
#define KR_START     KR_APP(1)
#define KR_DEVPRIVS  KR_APP(2)
#define KR_MYNODE    KR_APP(3)
#define KR_MYSPACE   KR_APP(4)
#define KR_PHYSRANGE KR_APP(5)
#define KR_SCRATCH   KR_APP(6)
#define KR_SCRATCH2  KR_APP(7)

unsigned int pci_probe = PCI_PROBE_CONF1 | PCI_PROBE_CONF2;

/* Compatibility wrappers for the Linux code: */
#define EINVAL 5
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t  u8;
#define DBG(s) kdprintf(KR_OSTREAM, s)
#define KERN_INFO "info: "
#define printk(s) kprintf(KR_OSTREAM, s)
#undef request_region
#define request_region(p, l, s)  (void) 0

struct pci_ops {
	int (*read_byte)(struct pci_dev *, int where, u8 *val);
	int (*read_word)(struct pci_dev *, int where, u16 *val);
	int (*read_dword)(struct pci_dev *, int where, u32 *val);
	int (*write_byte)(struct pci_dev *, int where, u8 val);
	int (*write_word)(struct pci_dev *, int where, u16 val);
	int (*write_dword)(struct pci_dev *, int where, u32 val);
};
struct pci_ops *pci_ops_ptr = NULL;

#define spin_lock_irqsave(lock,flg) ((void) &flg)
#define spin_unlock_irqrestore(lock,flg) ((void) &flg)
#define __save_flags(x) ((void)x)
#define __restore_flags(x) ((void)x)
#define __cli() (void)0
#define MAX_SIZE 1000

/* Globals */
char rcv_buffer[MAX_SIZE];  /* Data is received into this buffer */
unsigned int find_total = 0;

#define RETURN_IF_NOT_INITIALIZED  if (!pci_initialized) { \
                                 msg->snd_code = RC_Pci_NotInitialized; \
kprintf(KR_OSTREAM, "*** PCI DRIVER NOT INITIALIZED!\n"); \
                                 return true; \
                               }

#define RETURN_IF_NO_DEVICE  { \
                                 msg->snd_code = RC_No_Such_Pci_Device; \
kprintf(KR_OSTREAM, "*** No Such PCI Device!\n"); \
                                 return true; \
                               }

/* Type 1 accesses: */
#define PCI_CONF1_ADDRESS(bus, dev, fn, reg) \
	(0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3))

static int pci_conf1_read (int seg, int bus, int dev, int fn, int reg, int len, u32 *value) /* !CONFIG_MULTIQUAD */
{
	unsigned long flags;

	if (bus > 255 || dev > 31 || fn > 7 || reg > 255)
		return -EINVAL;

	spin_lock_irqsave(&pci_config_lock, flags);

	outl(PCI_CONF1_ADDRESS(bus, dev, fn, reg), 0xCF8);

	switch (len) {
	case 1:
		*value = inb(0xCFC + (reg & 3));
		break;
	case 2:
		*value = inw(0xCFC + (reg & 2));
		break;
	case 4:
		*value = inl(0xCFC);
		break;
	}

	return 0;
}

static int pci_conf1_write (int seg, int bus, int dev, int fn, int reg, int len, u32 value) /* !CONFIG_MULTIQUAD */
{
	unsigned long flags;

	if ((bus > 255 || dev > 31 || fn > 7 || reg > 255)) 
		return -EINVAL;

	spin_lock_irqsave(&pci_config_lock, flags);

	outl(PCI_CONF1_ADDRESS(bus, dev, fn, reg), 0xCF8);

	switch (len) {
	case 1:
		outb((u8)value, 0xCFC + (reg & 3));
		break;
	case 2:
		outw((u16)value, 0xCFC + (reg & 2));
		break;
	case 4:
		outl((u32)value, 0xCFC);
		break;
	}

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

#undef PCI_CONF1_ADDRESS

static int pci_conf1_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	int result; 
	u32 data;

	result = pci_conf1_read(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 1, &data);

	*value = (u8)data;

	return result;
}

static int pci_conf1_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	int result; 
	u32 data;

	result = pci_conf1_read(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 2, &data);

	*value = (u16)data;

	return result;
}

static int pci_conf1_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	return pci_conf1_read(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 4, value);
}

static int pci_conf1_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	return pci_conf1_write(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 1, value);
}

static int pci_conf1_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	return pci_conf1_write(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 2, value);
}

static int pci_conf1_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	return pci_conf1_write(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 4, value);
}

static struct pci_ops pci_direct_conf1 = {
	pci_conf1_read_config_byte,
	pci_conf1_read_config_word,
	pci_conf1_read_config_dword,
	pci_conf1_write_config_byte,
	pci_conf1_write_config_word,
	pci_conf1_write_config_dword
};

/*
 * Functions for accessing PCI configuration space with type 2 accesses
 */

#define PCI_CONF2_ADDRESS(dev, reg)	(u16)(0xC000 | (dev << 8) | reg)

static int pci_conf2_read (int seg, int bus, int dev, int fn, int reg, int len, u32 *value)
{
	unsigned long flags;

	if (bus > 255 || dev > 31 || fn > 7 || reg > 255)
		return -EINVAL;

	if (dev & 0x10) 
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&pci_config_lock, flags);

	outb((u8)(0xF0 | (fn << 1)), 0xCF8);
	outb((u8)bus, 0xCFA);

	switch (len) {
	case 1:
		*value = inb(PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 2:
		*value = inw(PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 4:
		*value = inl(PCI_CONF2_ADDRESS(dev, reg));
		break;
	}

	outb (0, 0xCF8);

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

static int pci_conf2_write (int seg, int bus, int dev, int fn, int reg, int len, u32 value)
{
	unsigned long flags;

	if ((bus > 255 || dev > 31 || fn > 7 || reg > 255)) 
		return -EINVAL;

	if (dev & 0x10) 
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&pci_config_lock, flags);

	outb((u8)(0xF0 | (fn << 1)), 0xCF8);
	outb((u8)bus, 0xCFA);

	switch (len) {
	case 1:
		outb ((u8)value, PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 2:
		outw ((u16)value, PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 4:
		outl ((u32)value, PCI_CONF2_ADDRESS(dev, reg));
		break;
	}

	outb (0, 0xCF8);    

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

#undef PCI_CONF2_ADDRESS

static int pci_conf2_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	int result; 
	u32 data;
	result = pci_conf2_read(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 1, &data);
	*value = (u8)data;
	return result;
}

static int pci_conf2_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	int result; 
	u32 data;
	result = pci_conf2_read(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 2, &data);
	*value = (u16)data;
	return result;
}

static int pci_conf2_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	return pci_conf2_read(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 4, value);
}

static int pci_conf2_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	return pci_conf2_write(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 1, value);
}

static int pci_conf2_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	return pci_conf2_write(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 2, value);
}

static int pci_conf2_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	return pci_conf2_write(0, dev->bus->number, PCI_SLOT(dev->devfn), 
		PCI_FUNC(dev->devfn), where, 4, value);
}

static struct pci_ops pci_direct_conf2 = {
	pci_conf2_read_config_byte,
	pci_conf2_read_config_word,
	pci_conf2_read_config_dword,
	pci_conf2_write_config_byte,
	pci_conf2_write_config_word,
	pci_conf2_write_config_dword
};

#define pci_read_config_byte(d,a,p)   (pci_ops_ptr->read_byte)(d,a,p)
#define pci_read_config_word(d,a,p)   (pci_ops_ptr->read_word)(d,a,p)
#define pci_read_config_dword(d,a,p)  (pci_ops_ptr->read_dword)(d,a,p)

#define pci_write_config_byte(d,a,p)  (pci_ops_ptr->write_byte)(d,a,p)
#define pci_write_config_word(d,a,p)  (pci_ops_ptr->write_word)(d,a,p)
#define pci_write_config_dword(d,a,p) (pci_ops_ptr->write_dword)(d,a,p)

#define PCI_MODE1 1
#define PCI_MODE2 2

//Malloc.c
#define ROUND_UP(x, y) ( ((x) % (y)) ? ( ((x) % (y)) + (y) ) : (x) )
extern void end();
static uint32_t top =(uint32_t) &end;
//
static bool  ProcessRequest(Message *msg);
struct pci_bus pci_root;
struct pci_bus *pci_bus_i;
struct pci_dev *pci_devices = NULL;
struct pci_dev *dev_counter  =NULL;
//LIST_HEAD(pci_root_buses);
//LIST_HEAD(pci_devices);


static uint32_t  pci_do_scan_bus(struct pci_bus *bus);
int pcibios_last_bus = -1;

static struct pci_dev **pci_last_dev_p = &pci_devices;
//static struct pci_dev **pci_last_dev_anshu = &pci_devices;

static int32_t pci_reverse = 0;

#define  CONFIG_CMD(bus,devfn,where) ((0x80000000 | bus << 16) | (devfn <<8) |(where & ~3))

unsigned int CONFIG_CM =0x00;

struct pci_device_info {
  unsigned short device;
  unsigned short seen;
  const char *name;
};

struct pci_vendor_info {
  unsigned short vendor;
  unsigned short nr;
  const char *name;
  struct pci_device_info *devices;
};

struct pci_bus *pci_root_bus;
struct pci_ops *pci_root_ops;



//////////Memcpy.c /////////


void *
memcpy(void *to, const void *from, size_t count)
{
  unsigned char *cto = (unsigned char *) to;
  unsigned char *cfrom = (unsigned char *) from;

  while (count--)
    *cto++ = *cfrom++;

  return to;
}


///////////Memset.c /////////////
#include <eros/target.h>

void *
memset(void *to, int c, size_t count)
{
  unsigned char *cto = (unsigned char *) to;

  while (count--)
    *cto++ = c;

  return to;
}



/////////Malloc.c /////////
void *
malloc(size_t sz)
{
  uint32_t ptr;
  
  top = ROUND_UP(top,sizeof(uint32_t)); 
  ptr = top;
  top +=  sz ;

  return (void*) ptr;
}
///////


uint32_t try;


/*-------------------------------------------------------------------------------*/





/*****************************************************************/

static void pci_read_bases(struct pci_dev *dev, unsigned int howmany){
  unsigned int reg;
  uint32_t l;

   for(reg=0; reg<howmany; reg++) {
      pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
     if (l == 0xffffffff)
       continue;
    dev->base_address[reg] = l;
     if ((l & (PCI_BASE_ADDRESS_SPACE | PCI_BASE_ADDRESS_MEM_TYPE_MASK))== (PCI_BASE_ADDRESS_SPACE_MEMORY | PCI_BASE_ADDRESS_MEM_TYPE_64)) {
      reg++;
        pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + (reg << 2), &l);
      if (l) {
#if BITS_PER_LONG == 64
	dev->base_address[reg-1] |= ((unsigned long) l) << 32;
#else
	kprintf(KR_OSTREAM,"PCI: Unable to handle 64-bit address for device %02x:%02x\
n", 
	       dev->bus->number, dev->devfn);  
	dev->base_address[reg-1] = 0;
#endif
      } 
    }
  }
   // for (i=0;i<howmany;i++){kprintf(KR_OSTREAM,"BR[%d] : %06x\n",i,dev->base_address[i]);}
  
  }



/**********************************************************/

static uint32_t pci_do_scan_bus(struct pci_bus *bus)
{
  uint8_t devfn, max;//pass;
  uint32_t l,class;
  uint8_t cmd, irq,tmp,hdr_type,is_multi=0;
  //struct list_head *ln;
  struct pci_dev *dev,**bus_last;
  struct pci_bus *child;      

  static struct pci_dev theDev;

  kprintf(KR_OSTREAM,"Inside: pci_do_scan_bus\n");
  kprintf(KR_OSTREAM,"Scanning bus %02x\n", bus->number);
  bus_last=&bus->devices;
  max = bus->secondary;
  for (devfn=0; devfn < 0xff; ++devfn) {
    if (PCI_FUNC(devfn) && !is_multi) {
      /* not a multi-function device */
      continue;
    }
    
    theDev.bus = bus;
    theDev.devfn = devfn;
    theDev.vendor = l & 0xffff;
    theDev.device = (l >> 16) & 0xffff;	  

    if (pci_read_config_byte(&theDev, PCI_HEADER_TYPE, &hdr_type))
      continue;

    if (!PCI_FUNC(devfn))
	    is_multi = hdr_type & 0x80;
    
    if (pci_read_config_dword(&theDev, PCI_VENDOR_ID, &l) ||
	/* some broken boards return 0 if a slot is empty: */
	      l == 0xffffffff || l == 0x00000000 || l == 0x0000ffff || l == 0xffff0000)
      continue;
    
    dev=malloc(sizeof(*dev));
    
    if(dev==NULL)
      {
              kprintf(KR_OSTREAM,"Out of memory\n");
	      continue;
      }
    
    
    
    memset(dev, 0, sizeof(*dev));
    dev->bus = bus;
    dev->devfn=devfn;
    dev->vendor = l & 0xffff;
    dev->device = (l >> 16) & 0xffff;	  
    
    
    /* determine if device can be a master */
    pci_read_config_byte(dev, PCI_COMMAND, &cmd);
    /* Set the PCI_COMMAND_IO bit also. 
     * FIX::: This is only for I/O space devices
     * Reset for memory space devices */
    pci_write_config_byte(dev, PCI_COMMAND, cmd | 
			  PCI_COMMAND_MASTER|PCI_COMMAND_IO);
    pci_read_config_byte(dev, PCI_COMMAND, &tmp);
    dev->master = ((tmp & PCI_COMMAND_MASTER) != 0);
    pci_read_config_dword(dev, PCI_CLASS_REVISION, &class);
    class >>= 8;                                /* upper 3 bytes */
    dev->class = class;
    class >>= 8;
    dev->hdr_type = hdr_type;
    
    //Switch for the header type
    switch (hdr_type & 0x7f) {    
      
    case PCI_HEADER_TYPE_NORMAL:                /* standard header */
      
      if (class == PCI_CLASS_BRIDGE_PCI)
	goto bad;
      /*
       * If the card generates interrupts, read IRQ number
       * (some architectures change it during pci_fixup())
	     */
      pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &irq);
      if (irq)
	pci_read_config_byte(dev, PCI_INTERRUPT_LINE, &irq);
      dev->irq = irq;
      
      pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, 
			   &dev->subsystem_vendor);
      pci_read_config_word(dev, PCI_SUBSYSTEM_ID, 
			   &dev->subsystem_device);

      /*
       * read base address registers, again pci_fixup() can
       * tweak these
       */
      pci_read_bases(dev, 6);
      pci_read_config_dword(dev, PCI_ROM_ADDRESS, &l);
      dev->rom_address = (l == 0xffffffff) ? 0 : l;
      break;
    case PCI_HEADER_TYPE_BRIDGE:                /* bridge header */
      if (class != PCI_CLASS_BRIDGE_PCI)
	goto bad;
      pci_read_bases(dev, 2);
      pci_read_config_dword(dev, PCI_ROM_ADDRESS1, &l);
      dev->rom_address = (l == 0xffffffff) ? 0 : l;
      break;
    case PCI_HEADER_TYPE_CARDBUS:               /* CardBus bridge header */
      if (class != PCI_CLASS_BRIDGE_CARDBUS)
	goto bad;
      pci_read_bases(dev, 1);
      break;	  
      
    default:                                    /* unknown header */
    bad:
      kprintf(KR_OSTREAM, "PCI: %02x:%02x [%04x/%04x/%06x] has unknown"
	      " header type %02x, ignoring.\n",
	      bus->number, dev->devfn, dev->vendor, dev->device, class, hdr_type);
      continue;
    }
    
    kprintf(KR_OSTREAM,"PCI: %02x:%02x [%04x/%04x] %06x\n", bus->number, dev->devfn, dev->vendor, 
	    dev->device,dev->class);
    /*
     * Put it into the global PCI device chain. It's used to
     * find devices once everything is set up.
     */
    if (!pci_reverse) {
	   *pci_last_dev_p = dev;
	   pci_last_dev_p = &dev->next;
    } else {
      dev->next = pci_devices;
      pci_devices = dev;
    }
    
    if (pci_devices == NULL) 
      kdprintf(KR_OSTREAM, "pci_devices is not getting updated\n");

    /*
     * Now insert it into the list of devices held
     * by the parent bus.
     */
    *bus_last = dev;
    bus_last = &dev->sibling;	  
  }
  
	/*
	 * After performing arch-dependent fixup of the bus, look behind
	 * all PCI-to-PCI bridges on this bus.
	 */
	//pci_fixup_bus(bus);
	/*
	 * The fixup code may have just found some peer pci bridges on this
	 * machine.  Update the max variable if that happened so we don't
	 * get duplicate bus numbers.
	 */
	for(child=&pci_root; child; child=child->next)
	  max=((max > child->subordinate) ? max : child->subordinate);
	
	for(dev=bus->devices; dev; dev=dev->sibling)
	  /*
	   * If it's a bridge, scan the bus behind it.
	   */
	  if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI) {
	    uint32_t  buses;
	    uint16_t cr;
	    
                         /*
			  * Check for a duplicate bus.  If we already scanned
                          * this bus number as a peer bus, don't also scan it
                          * as a child bus
                          */
	    if(
	       ((dev->vendor == PCI_VENDOR_ID_SERVERWORKS) &&
		((dev->device == PCI_DEVICE_ID_SERVERWORKS_HE) ||
		 (dev->device == PCI_DEVICE_ID_SERVERWORKS_LE) ||
		 (dev->device == PCI_DEVICE_ID_SERVERWORKS_CMIC_HE))) ||
	       ((dev->vendor == PCI_VENDOR_ID_COMPAQ) &&
		(dev->device == PCI_DEVICE_ID_COMPAQ_6010)) ||
	       ((dev->vendor == PCI_VENDOR_ID_INTEL) &&
		((dev->device == PCI_DEVICE_ID_INTEL_82454NX) ||
		 (dev->device == PCI_DEVICE_ID_INTEL_82451NX)))
	       )
                                 goto skip_it;
	    /*
	     * Read the existing primary/secondary/subordinate bus
	     * number configuration to determine if the PCI bridge
	     * has already been configured by the system.  If so,
	     * check to see if we've already scanned this bus as
	     * a result of peer bus scanning, if so, skip this.
	     */
	    pci_read_config_dword(dev, PCI_PRIMARY_BUS, &buses);
	    if ((buses & 0xFFFFFF) != 0)
                           {
                             for(child=pci_root.next;child;child=child->next)
			       if(child->number == ((buses >> 8) & 0xff))
				 goto skip_it;
                           }
	    
	    /*
	     * Insert it into the tree of buses.
	     */
	    child = malloc(sizeof(*child));
	    if(child==NULL)
	      {
		kprintf(KR_OSTREAM, "pci: out of memory for bridge.\n");
		continue;
	      }
	    memset(child, 0, sizeof(*child));
	    child->next = bus->children;
	    bus->children = child;
	    child->self = dev;
	    child->parent = bus;
	    
	    /*
	     * Set up the primary, secondary and subordinate
	     * bus numbers.
	     */
	    child->number = child->secondary = ++max;
	    child->primary = bus->secondary;
	    child->subordinate = 0xff;
	    /*
	     * Clear all status bits and turn off memory,
	     * I/O and master enables.
	     */
	    pci_read_config_word(dev, PCI_COMMAND, &cr);
	    pci_write_config_word(dev, PCI_COMMAND, 0x0000);
	    pci_write_config_word(dev, PCI_STATUS, 0xffff);
	    /*
	     * Read the existing primary/secondary/subordinate bus
	     * number configuration to determine if the PCI bridge
	     * has already been configured by the system.  If so,
	     * do not modify the configuration, merely note it.
	     */
	    pci_read_config_dword(dev, PCI_PRIMARY_BUS, &buses);
	    if ((buses & 0xFFFFFF) != 0)
	      {
		uint32_t cmax;
 
		child->primary = buses & 0xFF;
		child->secondary = (buses >> 8) & 0xFF;
		child->subordinate = (buses >> 16) & 0xFF;
		child->number = child->secondary;
		cmax = pci_do_scan_bus(child);
		if (cmax > max) max = cmax;
	      }
	    else
	      {
		/*
		 * Configure the bus numbers for this bridge:
		 */
		buses &= 0xff000000;
		buses |=
		  (((unsigned int)(child->primary)     <<  0) |
		   ((unsigned int)(child->secondary)   <<  8) |
		   ((unsigned int)(child->subordinate) << 16));
		pci_write_config_dword(dev, PCI_PRIMARY_BUS, buses);
		/*
		 * Now we can scan all subordinate buses:
		 */
		max = pci_do_scan_bus(child);
		/*
		 * Set the subordinate bus number to its real
		 * value:
		 */
		child->subordinate = max;
		buses = (buses & 0xff00ffff)
		  | ((unsigned int)(child->subordinate) << 16);
		pci_write_config_dword(dev, PCI_PRIMARY_BUS, buses);
	      }
	    pci_write_config_word(dev, PCI_COMMAND, cr);
	  skip_it:
	    ;
	  }
	kprintf(KR_OSTREAM,"PCI: pci_scan_bus returning with max=%02x\n", max);
	return max;
}
/*************************************************************/

/**************************************/
/*
 * Before we decide to use direct hardware access mechanisms, we try to do some
 * trivial checks to ensure it at least _seems_ to be working -- we just test
 * whether bus 00 contains a host bridge (this is similar to checking
 * techniques used in XFree86, but ours should be more reliable since we
 * attempt to make use of direct access hints provided by the PCI BIOS).
 *
 * This should be close to trivial, but it isn't, because there are buggy
 * chipsets (yes, you guessed it, by Intel and Compaq) that have no class ID.
 */
static int pci_sanity_check(struct pci_ops *o)
{
	u16 x;
	static struct pci_bus bus;		/* Fake bus and device */
	static struct pci_dev dev;

	if (pci_probe & PCI_NO_CHECKS)
		return 1;
	bus.number = 0;
	dev.bus = &bus;
	for(dev.devfn=0; dev.devfn < 0x100; dev.devfn++)
		if ((!o->read_word(&dev, PCI_CLASS_DEVICE, &x) &&
		     (x == PCI_CLASS_BRIDGE_HOST || x == PCI_CLASS_DISPLAY_VGA)) ||
		    (!o->read_word(&dev, PCI_VENDOR_ID, &x) &&
		     (x == PCI_VENDOR_ID_INTEL || x == PCI_VENDOR_ID_COMPAQ)))
			return 1;
	DBG("PCI: Sanity check failed\n");
	return 0;
}

static struct pci_ops *pci_check_direct(void)
{
  unsigned int tmp;
  unsigned long flags;

  __save_flags(flags); __cli();

  /*
   * Check if configuration type 1 works.
   */
  if (pci_probe & PCI_PROBE_CONF1) {
    outb (0x01, 0xCFB);
    tmp = inl (0xCF8);
    outl (0x80000000, 0xCF8);
    if (inl (0xCF8) == 0x80000000 &&
	pci_sanity_check(&pci_direct_conf1)) {
      outl (tmp, 0xCF8);
      __restore_flags(flags);
      printk(KERN_INFO "PCI: Using configuration type 1\n");
      request_region(0xCF8, 8, "PCI conf1");

#ifdef CONFIG_MULTIQUAD			
      /* Multi-Quad has an extended PCI Conf1 */
      if(clustered_apic_mode == CLUSTERED_APIC_NUMAQ)
	return &pci_direct_mq_conf1;
#endif				
      return &pci_direct_conf1;
    }
    outl (tmp, 0xCF8);
  }

  /*
   * Check if configuration type 2 works.
   */
  if (pci_probe & PCI_PROBE_CONF2) {
    outb (0x00, 0xCFB);
    outb (0x00, 0xCF8);
    outb (0x00, 0xCFA);
    if (inb (0xCF8) == 0x00 && inb (0xCFA) == 0x00 &&
	pci_sanity_check(&pci_direct_conf2)) {
      __restore_flags(flags);
      printk(KERN_INFO "PCI: Using configuration type 2\n");
      request_region(0xCF8, 4, "PCI conf2");
      return &pci_direct_conf2;
    }
  }

  __restore_flags(flags);
  return NULL;
}

#if 0
int pci_mode_check()
{
  unsigned int tmp;
  
  kprintf(KR_OSTREAM,"Inside: pci_mode_check\n");
    //Checking if configuration typeis  1
    outb(0x01,0xCFB);
    tmp=inl(0xCF8);
    outl(0x80000000,0xCF8);
    kprintf(KR_OSTREAM,"Tmp = %x \n",tmp);
    if (inl(0xCF8) == 0x80000000)// && pci_sanity_check(&pci_direct_conf1))
      {
	outl(tmp,0xCF8);
	kprintf(KR_OSTREAM, "PCI : Direct  Configuration Type 1\n");
	return PCI_MODE1;
      }
     outl(tmp,0xCF8); 
     
     /**
	To check if configuration type 2 works . Since  for the current m/c 
	type 1 works I have only the type1 implemented .
	Will need to find a way to check type 2 configuration 
	it . ..... This needs to be done later on and checked . 
     **/  
     
     /*
       outb(0x00,0xCFB);
       outb(0x00,0xCF8);
       outb(0x00,0xCFA);
       
       if (inb(0xCF8) == 0x00 && inb(0xCFA)==0x00){
       
       kprintf(KR_OSTREAM, "Checking direct ...type 2\n");
       
       }
       return PCI_MODE2;;
     */
     kprintf(KR_OSTREAM, "Type 1 not found \n");
     return PCI_MODE2;
}
#endif



/***************************************/
void pci_init(void)
{
#if 0
  //  struct pci_ops *dir = NULL;
  kprintf(KR_OSTREAM,"Inside: pci_init\n");

  memset(&pci_root,0,sizeof(pci_root));

  switch(pci_mode_check()){
  case PCI_MODE1:
    kprintf(KR_OSTREAM,"PCI INIT:MODE 1 detected\n");
    pci_root.subordinate = pci_do_scan_bus(&pci_root);
    pci_initialized = true;
    break;
  case PCI_MODE2:
    kprintf(KR_OSTREAM,"PCI INIT:MODE 2 detected  (shap hack: use mode 1 anyway)\n");
    pci_root.subordinate = pci_do_scan_bus(&pci_root);
    pci_initialized = true;
    break;
  default:
    kprintf(KR_OSTREAM,"PCI INIT:No PCI Bus detected\n");
    break;
  }
#else
  pci_ops_ptr = pci_check_direct();
  if (pci_ops_ptr == NULL)
    kdprintf(KR_OSTREAM, "No PCI method found\n");
  pci_root.subordinate = pci_do_scan_bus(&pci_root);
#endif
}


/***************************************/
bool  pci_get_vendorid_device(uint32_t vendorid,uint32_t index)
{
  uint32_t cur_indx=0;
  kprintf(KR_OSTREAM,"Inside: pci_get_device\n");
  kprintf(KR_OSTREAM,"Value of vendor id %04x & index number is %04d \n",vendorid,index);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if (dev_counter->vendor == vendorid)
	{

	  if (cur_indx == index)
	    {
	  
	      //  kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x\n", dev_counter->devfn, dev_counter->vendor, 
	      //      dev_counter->device,dev_counter->base_address[0]);
	      return true;
	    }
	  cur_indx++;
	}
    }
  return false;
}



/***************************************/
bool  pci_get_classid_device(uint32_t classID,uint32_t index)
{
  uint32_t cur_indx=0;
  //classID=0x30000;
  kprintf(KR_OSTREAM,"Inside: pci_get_classid_device\n");
  kprintf(KR_OSTREAM,"Value of class id %04x & index number is %04d \n",classID,index);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if ((dev_counter->class >> 8) == classID)
	{

	  if (cur_indx == index)
	    {
	  
	        kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x %06x\n", dev_counter->devfn, dev_counter->vendor, 
	           dev_counter->device,dev_counter->base_address[0],dev_counter->class);
	      return true;
	    }
	  cur_indx++;
	}
    }
  return false;
}


/***************************************/
bool  pci_get_base_classid_device(uint32_t baseClassID,uint32_t index)
{
  uint32_t cur_indx=0;
  //classID=0x30000;
  kprintf(KR_OSTREAM,"Inside: pci_get_base_classid_device\n");
  kprintf(KR_OSTREAM,"Value of base class id %04x & index number is %04d \n",baseClassID,index);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if ((dev_counter->class >> 16) == baseClassID)
	{

	  if (cur_indx == index)
	    {
	  
	        kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x %06x\n", dev_counter->devfn, dev_counter->vendor, 
	           dev_counter->device,dev_counter->base_address[0],dev_counter->class);
	      return true;
	    }
	  cur_indx++;
	}
    }
  return false;
}


/***************************************/
bool  pci_get_vendevid_device(uint32_t vendorid,uint32_t devid,uint32_t index)
{
  uint32_t cur_indx=0;
  kprintf(KR_OSTREAM,"Inside: pci_get_device\n");
  kprintf(KR_OSTREAM,"Value of vendor id %04x\n",vendorid);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if ((dev_counter->vendor == vendorid)&&(dev_counter->device == devid) )
	{
	  if (cur_indx == index)
	    {
	      //  kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x\n", dev_counter->devfn, dev_counter->vendor, 
	      //      dev_counter->device,dev_counter->base_address[0]);
	      return true;
	    }
	  cur_indx++;
	}
    }
  return false;
}


/***************************************/
bool pci_get_vendorid_total(uint32_t vendorid)
{
  kprintf(KR_OSTREAM,"Inside: pci_get_vendorid_total\n");
  find_total =0; //Resetting the total counter for next search 
  dev_counter = pci_devices;
  kprintf(KR_OSTREAM,"Value of vendor id %04x\n",vendorid);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if (dev_counter->vendor == vendorid)
	{
	  //         kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x\n", dev_counter->devfn, dev_counter->vendor, 
	  // dev_counter->device,dev_counter->base_address[0]);
	  find_total++;  
	}
    }
  kprintf(KR_OSTREAM,"The total value is %d\n",find_total);
  //(find_total == 0) ? return false : return true;
  if (!find_total) {return false;} else {return true;}
}



/***************************************/
bool pci_get_vendevid_total(uint32_t vendorid,uint32_t devid)
{
  kprintf(KR_OSTREAM,"Inside: pci_get_vendorid_total\n");
  find_total =0; //Resetting the total counter for next search 
  dev_counter = pci_devices;
  kprintf(KR_OSTREAM,"Value of vendor id %04x\n",vendorid);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if((dev_counter->vendor == vendorid) &&(dev_counter->device == devid) )
     	{
	  //         kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x\n", dev_counter->devfn, dev_counter->vendor, 
	  // dev_counter->device,dev_counter->base_address[0]);
	  find_total++;  
	}
    }
  //  kprintf(KR_OSTREAM,"The total value is %d\n",find_total);
  //(find_total == 0) ? return false : return true;
  if (!find_total) {return false;} else {return true;}
}


bool pci_get_classid_total(uint32_t classID)
{
  kprintf(KR_OSTREAM,"Inside: pci_get_classid_total\n");
  find_total =0; //Resetting the total counter for next search 
  dev_counter = pci_devices;
  //  classID=0x30000;
  kprintf(KR_OSTREAM,"Value of class id %06x\n",classID);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if((dev_counter->class >> 8) == classID) 
     	{
          kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x %06x\n", dev_counter->devfn, dev_counter->vendor, 
	 dev_counter->device,dev_counter->base_address[0],dev_counter->class);
	  find_total++;  
	}
    }
  kprintf(KR_OSTREAM,"The total value is %d\n",find_total);
  //(find_total == 0) ? return false : return true;
  if (!find_total) {return false;} else {return true;}
}

bool pci_get_base_classid_total(uint32_t baseClassID)
{
  kprintf(KR_OSTREAM,"Inside: pci_get_base_classid_total\n");
  find_total =0; //Resetting the total counter for next search 
  dev_counter = pci_devices;
  //  baseClassID=0x30000;
  kprintf(KR_OSTREAM,"Value of base class id %06x\n",baseClassID);
  for (dev_counter = pci_devices; dev_counter; dev_counter = dev_counter->next)
    {
      if((dev_counter->class >> 16) == baseClassID) 
     	{
          kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x %06x\n", dev_counter->devfn, dev_counter->vendor, 
	 dev_counter->device,dev_counter->base_address[0],dev_counter->class);
	  find_total++;  
	}
    }
  kprintf(KR_OSTREAM,"The total value is %d\n",find_total);
  //(find_total == 0) ? return false : return true;
  if (!find_total) {return false;} else {return true;}
}



static bool
ProcessRequest(Message *msg)
{

  unsigned short vendor;

  uint32_t index,devid,class;
  //  unsigned short  device; May be in a diff function of with some switches

  struct pci_dev_data *snd_dev=NULL;


 

  switch (msg->rcv_code) {
    
  case OC_Pci_Initialize:
    {
      if (!pci_ops_ptr)
	pci_init();

    }/*End of OC_Intialize*/
    break;

  case OC_Pci_Scan_Bus:
    {
      //Just a place holder for now
      //Not sure if this needs to be implemented
      
    }/*End of OC_Pci_Scan_Bus*/
    break;
    
  case OC_Pci_Find_VendorID:
    {
      vendor = msg->rcv_w1;
      devid  = msg->rcv_w2;
      index  = msg->rcv_w3;

      if (!pci_ops_ptr)
	pci_init();

      //      pci_init();
      //vendor = 0x15AD;
      if (!pci_get_vendorid_device(vendor,index))
	{
	  kprintf(KR_OSTREAM,"VendorID not found\n");
	  RETURN_IF_NO_DEVICE;
	}

      // kprintf(KR_OSTREAM,"Size:%06d\n",sizeof(struct pci_dev) );
      // kprintf(KR_OSTREAM,"Size:%06d\n",sizeof(struct pci_dev_data) );
      kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x %d\n", 
	      dev_counter->devfn, dev_counter->vendor, 
	      dev_counter->device,dev_counter->base_address[0],
	      dev_counter->bus->number);

      snd_dev->devfn        = dev_counter->devfn;
      snd_dev->vendor       = dev_counter->vendor;
      snd_dev->device       = dev_counter->device;
      snd_dev->subsystem_vendor       = dev_counter->subsystem_vendor;
      snd_dev->subsystem_device       = dev_counter->subsystem_device;
      snd_dev->busnumber      = dev_counter->bus->number;
      snd_dev->class        = dev_counter->class;
      snd_dev->hdr_type     = dev_counter->hdr_type;
      snd_dev->master       = dev_counter->master;
      snd_dev->irq          = dev_counter->irq;      
      snd_dev->base_address[0] = dev_counter->base_address[0];
      //Sending just 1st bar right now .. have to make it upto 6
      snd_dev->rom_address  = dev_counter->rom_address;
  
      msg->snd_data = snd_dev;
      msg->snd_len = sizeof(struct pci_dev_data);
      msg->snd_code = RC_OK;

    }/*End of OC_Pci_Find_Device */

    break;

  case OC_Pci_Find_VendorID_Total:
    {
      if (!pci_ops_ptr)
	pci_init();

      vendor = msg->rcv_w1;
      devid  = msg->rcv_w2;
      index  = msg->rcv_w3;

      if(! pci_get_vendorid_total(vendor)) RETURN_IF_NO_DEVICE;
      msg->snd_w1 = find_total;
      msg->snd_code = RC_OK;

    }/*End of OC_Pci_Find_VendorID_Total */

    break;


  case OC_Pci_Find_ClassID_Total:
    {
      if (!pci_ops_ptr)
	pci_init();

      class=msg->rcv_w1;

      if(! pci_get_classid_total(class)) RETURN_IF_NO_DEVICE;
      msg->snd_w1 = find_total;
      msg->snd_code = RC_OK;

    }

    break;

  case OC_Pci_Find_Base_ClassID_Total:
    {
      if (!pci_ops_ptr)
	pci_init();

      class=msg->rcv_w1;
      if(! pci_get_base_classid_total(class)) RETURN_IF_NO_DEVICE;
      msg->snd_w1 = find_total;
      msg->snd_code = RC_OK;

    }

    break;


  case OC_Pci_Find_VenDevID:
    {
      if (!pci_ops_ptr)
	pci_init();

      vendor = msg->rcv_w1;
      devid  = msg->rcv_w2;
      index  = msg->rcv_w3;
      //      pci_init();
      //vendor = 0x15AD;
      if (!pci_get_vendevid_device(vendor,devid,index))
	{
	  kprintf(KR_OSTREAM,"VendorDeviceID not found\n");
	  RETURN_IF_NO_DEVICE;
	}

      // kprintf(KR_OSTREAM,"Size:%06d\n",sizeof(struct pci_dev) );
      // kprintf(KR_OSTREAM,"Size:%06d\n",sizeof(struct pci_dev_data) );
      kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x %x\n", 
	      dev_counter->devfn, dev_counter->vendor, 
	      dev_counter->device,dev_counter->base_address[0],
	      dev_counter->bus);

      snd_dev->devfn        = dev_counter->devfn;
      snd_dev->vendor       = dev_counter->vendor;
      snd_dev->device       = dev_counter->device;
      snd_dev->subsystem_vendor       = dev_counter->subsystem_vendor;
      snd_dev->subsystem_device       = dev_counter->subsystem_device;
      snd_dev->busnumber        = dev_counter->bus->number;
      snd_dev->class        = dev_counter->class;
      snd_dev->hdr_type     = dev_counter->hdr_type;
      snd_dev->master       = dev_counter->master;
      snd_dev->irq          = dev_counter->irq;      
      snd_dev->base_address[0] = dev_counter->base_address[0];
      //Sending just 1st bar right now .. have to make it upto 6
      snd_dev->rom_address  = dev_counter->rom_address;
      msg->snd_data = snd_dev;
      msg->snd_len = sizeof(struct pci_dev_data);
      msg->snd_code = RC_OK;

    }/*End of OC_Pci_Find_Device */

    break;
    
  case OC_Pci_Find_VenDevID_Total:
    {
      if (!pci_ops_ptr)
	pci_init();

      if(! pci_get_vendevid_total(vendor,devid)) RETURN_IF_NO_DEVICE;
      kprintf(KR_OSTREAM,"The total value is %d\n",find_total);
      msg->snd_w1 = find_total;
      msg->snd_code = RC_OK;

    }
    
    break;

  case OC_Pci_Find_ClassID:
    {
      if (!pci_ops_ptr)
	pci_init();

      class=msg->rcv_w1;
      index = msg->rcv_w3;

      if (!pci_get_classid_device(class,index))
	{
	  kprintf(KR_OSTREAM,"ClassID not found\n");
	  RETURN_IF_NO_DEVICE;
	}

      kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x\n", dev_counter->devfn, dev_counter->vendor, 
	      dev_counter->device,dev_counter->base_address[0]);

      snd_dev->devfn        = dev_counter->devfn;
      snd_dev->vendor       = dev_counter->vendor;
      snd_dev->device       = dev_counter->device;
      snd_dev->subsystem_vendor       = dev_counter->subsystem_vendor;
      snd_dev->subsystem_device       = dev_counter->subsystem_device;
      snd_dev->busnumber        = dev_counter->bus->number;
      snd_dev->class        = dev_counter->class;
      snd_dev->hdr_type     = dev_counter->hdr_type;
      snd_dev->master       = dev_counter->master;
      snd_dev->irq          = dev_counter->irq;      
      snd_dev->base_address[0] = dev_counter->base_address[0];
      //Sending just 1st bar right now .. have to make it upto 6
      snd_dev->rom_address  = dev_counter->rom_address;
  

      msg->snd_data = snd_dev;
      msg->snd_len = sizeof(struct pci_dev_data);
      msg->snd_code = RC_OK;

    }/*End of OC_Pci_Find_ClassID */

    break;

  case OC_Pci_Find_Base_ClassID:
    {
      if (!pci_ops_ptr)
	pci_init();

      class=msg->rcv_w1;
      index = msg->rcv_w3;

      if (!pci_get_base_classid_device(class,index))
	{
	  kprintf(KR_OSTREAM,"BaseClassID not found\n");
	  RETURN_IF_NO_DEVICE;
	}

      kprintf(KR_OSTREAM,"Chain: 00:%02x [%04x/%04x] BR[0] %06x\n", dev_counter->devfn, dev_counter->vendor, 
	      dev_counter->device,dev_counter->base_address[0]);

      snd_dev->devfn        = dev_counter->devfn;
      snd_dev->vendor       = dev_counter->vendor;
      snd_dev->device       = dev_counter->device;
      snd_dev->subsystem_vendor       = dev_counter->subsystem_vendor;
      snd_dev->subsystem_device       = dev_counter->subsystem_device;
      snd_dev->busnumber        = dev_counter->bus->number;
      snd_dev->class        = dev_counter->class;
      snd_dev->hdr_type     = dev_counter->hdr_type;
      snd_dev->master       = dev_counter->master;
      snd_dev->irq          = dev_counter->irq;      
      snd_dev->base_address[0] = dev_counter->base_address[0];
      //Sending just 1st bar right now .. have to make it upto 6
      snd_dev->rom_address  = dev_counter->rom_address;
  

      msg->snd_data = snd_dev;
      msg->snd_len = sizeof(struct pci_dev_data);
      msg->snd_code = RC_OK;
      
    }/*End of OC_Pci_Find_ClassID */
    break;
  case OC_Pci_write_config_dword: 
    {
      struct pci_dev_data *dev;
      
      dev = (struct pci_dev_data *)&rcv_buffer[0];
      msg->snd_w1 = pci_conf1_write(0, dev->busnumber, 
				    PCI_SLOT(dev->devfn),PCI_FUNC(dev->devfn), 
				    msg->rcv_w1, 4, msg->rcv_w2);
      msg->snd_code = RC_OK;
    }
    
    break;
  
  case OC_Pci_read_config_dword: 
    {
      struct pci_dev_data *dev;
      uint32_t value;
      
      dev = (struct pci_dev_data *)&rcv_buffer[0];
      msg->rcv_w1 = pci_conf1_read(0, dev->busnumber, PCI_SLOT(dev->devfn), 
				   PCI_FUNC(dev->devfn),msg->rcv_w1, 4,&value);
      msg->snd_w1 = value;
      msg->snd_code = RC_OK;
    }
    break;
 case OC_Pci_write_config_word: 
    {
      struct pci_dev_data *dev;
      
      dev = (struct pci_dev_data *)&rcv_buffer[0];
      msg->snd_w1 = pci_conf1_write(0, dev->busnumber, 
				    PCI_SLOT(dev->devfn), 
				    PCI_FUNC(dev->devfn), 
				    msg->rcv_w1, 2, msg->rcv_w2);
      msg->snd_code = RC_OK;
    }
    
    break;
 case OC_Pci_read_config_word: 
   {
     struct pci_dev_data *dev;
     uint32_t value;
     
     dev = (struct pci_dev_data *)&rcv_buffer[0];
     msg->rcv_w1 = pci_conf1_read(0,dev->busnumber, PCI_SLOT(dev->devfn), 
				  PCI_FUNC(dev->devfn),msg->rcv_w1,2,&value);
     msg->snd_w1 = value;
     msg->snd_code = RC_OK;
    }
    break;
  case OC_Pci_write_config_byte: 
    {
      struct pci_dev_data *dev;
      
      dev = (struct pci_dev_data *)&rcv_buffer[0];
      msg->snd_w1 = pci_conf1_write(0, dev->busnumber, 
				    PCI_SLOT(dev->devfn), 
				    PCI_FUNC(dev->devfn), 
				    msg->rcv_w1, 1, msg->rcv_w2);
      msg->snd_code = RC_OK;
    }
    
    break;
 case OC_Pci_read_config_byte: 
   {
     struct pci_dev_data *dev;
     uint32_t value;
     
     dev = (struct pci_dev_data *)&rcv_buffer[0];
     msg->rcv_w1 = pci_conf1_read(0,dev->busnumber, PCI_SLOT(dev->devfn), 
				  PCI_FUNC(dev->devfn),msg->rcv_w1,1,&value);
     msg->snd_w1 = value;
     msg->snd_code = RC_OK;
    }
   break;
  }


  //OC_Pci_Find_VendorID  
  //OC_Pci_Find_VenDevID 
  //OC_Pci_Find_VenDevID_Total  


  return true;
}



/*************************** MAIN **********************/
int
main(void)
{
  Message msg;

  capros_Node_getSlot(KR_CONSTIT, KC_OSTREAM,   KR_OSTREAM);
  capros_Node_getSlot(KR_CONSTIT, KC_DEVPRIVS,  KR_DEVPRIVS);
  capros_Node_getSlot(KR_CONSTIT, KC_PHYSRANGE, KR_PHYSRANGE);
  capros_arch_i386_Process_setIoSpace(KR_SELF, KR_DEVPRIVS);

 /*Make a start key to pass back to the constructor*/
  capros_Process_makeStartKey(KR_SELF,0,KR_START);

  kprintf(KR_OSTREAM,"*******************************************\n");
  kprintf(KR_OSTREAM,"Initializing PCI Probe ...\n");




    msg.snd_invKey   = KR_RETURN;      //Inv_Key
    msg.snd_code   = 0;
    msg.snd_key0   = KR_VOID;
    msg.snd_key1   = KR_VOID;
    msg.snd_key2   = KR_VOID;
    msg.snd_rsmkey = KR_RETURN;
    msg.snd_data   = 0;
    msg.snd_len    = 0;
    msg.snd_w1     = 0;
    msg.snd_w2     = 0;
    msg.snd_w3     = 0;
    

    msg.rcv_rsmkey = KR_RETURN;
    msg.rcv_key0   = KR_VOID;
    msg.rcv_key1   = KR_VOID;
    msg.rcv_key2   = KR_VOID;
    msg.rcv_data   = &rcv_buffer[0];
    msg.rcv_limit  = sizeof(struct pci_dev);
    msg.rcv_code   = 0;
    msg.rcv_w1     = 0;
    msg.rcv_w2     = 0;
    msg.rcv_w3     = 0;

    //    msg.rcv_data=&rcvdata;
    msg.rcv_limit = 8;
    msg.snd_key0=KR_START;
    RETURN(&msg); /*returning the start key*/

    kprintf(KR_OSTREAM,"Got invoked \n");
  /* Ready to process request until termination */
  while(ProcessRequest(&msg)){
    msg.snd_invKey = KR_RETURN;
    RETURN(&msg);
  }
  return 0;
}


