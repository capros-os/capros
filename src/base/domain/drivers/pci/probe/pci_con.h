#include <eros/target.h>
//#include "list.h"
#include "ioport.h"

#define PAGE_OFFSET_RAW 0xc0000000
#define PAGE_OFFSET (PAGE_OFFSET_RAW)
/*
#define __va(x)                      ((void *)((unsigned long)(x)+PAGE_OFFSET))
*/

#define PCI_NO_CHECKS 0x400

#define PCI_VENDOR_ID  0x00
#define PCI_DEVICE_ID  0x02
#define PCI_COMMAND    0x04

#define PCI_COMMAND_IO 0x1
#define PCI_COMMAND_MEMORY 0x2
#define PCI_COMMAND_MASTER 0x4
#define PCI_COMMAND_SPECIAL 0x8
#define PCI_COMMAND_INVALIDATE 0x10
#define PCI_COMMAND_VGA_PALETTE 0x20
#define PCI_COMMAND_PARITY 0x40
#define PCI_COMMAND_WAIT 0x80
#define PCI_COMMAND_SERR 0x100
#define PCI_COMMAND_FAST_BACK 0x200

#define PCI_STATUS   0x16
#define PCI_STAUS_CAP_LIST 0x10
#define PCI_STATUS_66MHZ 0x20
#define PCI_STATUS_UDF 0x40
#define PCI_STATUS_FAST_BACK 0x80
#define PCI_STATUS_PARITY 0x100
#define PCI_STATUS_DEVSEL_MASK 0x600

 
#define PCI_DMA_BIDIRECTIONAL 0
#define PCI_DMA_TODEVICE      1
#define PCI_DMA_FROMDEVICE    2
#define PCI_DMA_NONE          3

#define DEVICE_COUNT_COMPATIBLE 4
#define DEVICE_COUNT_IRQ        2
#define DEVICE_COUNT_DMA        2
#define DEVICE_COUNT_RESOURCE  12

/*
 *  Error values that may be returned by PCI functions.
 */
#define PCIBIOS_SUCCESSFUL		0x00
#define PCIBIOS_FUNC_NOT_SUPPORTED	0x81
#define PCIBIOS_BAD_VENDOR_ID		0x83
#define PCIBIOS_DEVICE_NOT_FOUND	0x86
#define PCIBIOS_BAD_REGISTER_NUMBER	0x87
#define PCIBIOS_SET_FAILED		0x88
#define PCIBIOS_BUFFER_TOO_SMALL	0x89

#define PCI_CLASS_REVISION	0x08	/* High 24 bits are class, low 8
					   revision */
#define PCI_REVISION_ID         0x08    /* Revision ID */
#define PCI_CLASS_PROG          0x09    /* Reg. Level Programming Interface */
#define PCI_CLASS_DEVICE        0x0a    /* Device class */

#define PCI_CACHE_LINE_SIZE	0x0c	/* 8 bits */
#define PCI_LATENCY_TIMER	0x0d	/* 8 bits */
#define PCI_HEADER_TYPE		0x0e	/* 8 bits */
#define  PCI_HEADER_TYPE_NORMAL	0
#define  PCI_HEADER_TYPE_BRIDGE 1
#define  PCI_HEADER_TYPE_CARDBUS 2

#define PCI_BIST		0x0f	/* 8 bits */
#define PCI_BIST_CODE_MASK	0x0f	/* Return result */
#define PCI_BIST_START		0x40	/* 1 to start BIST, 2 secs or less */
#define PCI_BIST_CAPABLE	0x80	/* 1 if BIST capable */

#define PCI_DEVFN(slot,func)        ((((slot) & 0x1f) << 3) | ((func) & 0x07))

#define pci_dev_b(n) list_entry(n, struct pci_dev, bus_list)

#define PCI_NUM_RESOURCES 11
#define PCI_PRIMARY_BUS             0x18    /* Primary bus number */
#define PCI_SUBORDINATE_BUS 0x1a    /* Highest bus number behind the bridge */
#define PCI_BRIDGE_RESOURCES 7


#define pcibios_assign_all_busses()      0


/*
 * Base addresses specify locations in memory or I/O space.
 * Decoded size can be determined by writing a value of 
 * 0xffffffff to the register, and reading it back.  Only 
 * 1 bits are decoded.
 */
#define PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define PCI_BASE_ADDRESS_1	0x14	/* 32 bits [htype 0,1 only] */
#define PCI_BASE_ADDRESS_2	0x18	/* 32 bits [htype 0 only] */
#define PCI_BASE_ADDRESS_3	0x1c	/* 32 bits */
#define PCI_BASE_ADDRESS_4	0x20	/* 32 bits */
#define PCI_BASE_ADDRESS_5	0x24	/* 32 bits */
#define  PCI_BASE_ADDRESS_SPACE	0x01	/* 0 = memory, 1 = I/O */
#define  PCI_BASE_ADDRESS_SPACE_IO 0x01
#define  PCI_BASE_ADDRESS_SPACE_MEMORY 0x00
#define  PCI_BASE_ADDRESS_MEM_TYPE_MASK 0x06
#define  PCI_BASE_ADDRESS_MEM_TYPE_32	0x00	/* 32 bit address */
#define  PCI_BASE_ADDRESS_MEM_TYPE_1M	0x02	/* Below 1M [obsolete] */
#define  PCI_BASE_ADDRESS_MEM_TYPE_64	0x04	/* 64 bit address */
#define  PCI_BASE_ADDRESS_MEM_PREFETCH	0x08	/* prefetchable? */
#define  PCI_BASE_ADDRESS_MEM_MASK	(~0x0fUL)
#define  PCI_BASE_ADDRESS_IO_MASK	(~0x03UL)
/* bit 1 is reserved if address_space = 1 */


/* Header type 0 (normal devices) */
#define PCI_CARDBUS_CIS		0x28
#define PCI_SUBSYSTEM_VENDOR_ID	0x2c
#define PCI_SUBSYSTEM_ID	0x2e  
#define PCI_ROM_ADDRESS		0x30	/* Bits 31..11 are address, 10..1 reserved */
#define  PCI_ROM_ADDRESS_ENABLE	0x01
#define PCI_ROM_ADDRESS_MASK	(~0x7ffUL)

#define PCI_CAPABILITY_LIST	0x34	/* Offset of first capability list entry */


/* Header type 1 (PCI-to-PCI bridges) */
#define PCI_PRIMARY_BUS		0x18	/* Primary bus number */
#define PCI_SECONDARY_BUS	0x19	/* Secondary bus number */
#define PCI_SUBORDINATE_BUS	0x1a	/* Highest bus number behind the bridge */
#define PCI_SEC_LATENCY_TIMER	0x1b	/* Latency timer for secondary interface */
#define PCI_IO_BASE		0x1c	/* I/O range behind the bridge */
#define PCI_IO_LIMIT		0x1d
#define  PCI_IO_RANGE_TYPE_MASK	0x0f	/* I/O bridging type */
#define  PCI_IO_RANGE_TYPE_16	0x00
#define  PCI_IO_RANGE_TYPE_32	0x01
#define  PCI_IO_RANGE_MASK	~0x0f
#define PCI_SEC_STATUS		0x1e	/* Secondary status register, only bit 14 used */
#define PCI_MEMORY_BASE		0x20	/* Memory range behind */
#define PCI_MEMORY_LIMIT	0x22
#define  PCI_MEMORY_RANGE_TYPE_MASK 0x0f
#define  PCI_MEMORY_RANGE_MASK	~0x0f
#define PCI_PREF_MEMORY_BASE	0x24	/* Prefetchable memory range behind */
#define PCI_PREF_MEMORY_LIMIT	0x26
#define  PCI_PREF_RANGE_TYPE_MASK 0x0f
#define  PCI_PREF_RANGE_TYPE_32	0x00
#define  PCI_PREF_RANGE_TYPE_64	0x01
#define  PCI_PREF_RANGE_MASK	~0x0f
#define PCI_PREF_BASE_UPPER32	0x28	/* Upper half of prefetchable memory range */
#define PCI_PREF_LIMIT_UPPER32	0x2c
#define PCI_IO_BASE_UPPER16	0x30	/* Upper half of I/O addresses */
#define PCI_IO_LIMIT_UPPER16	0x32
/* 0x34 same as for htype 0 */
/* 0x35-0x3b is reserved */
#define PCI_ROM_ADDRESS1	0x38	/* Same as PCI_ROM_ADDRESS, but for htype 1 */
/* 0x3c-0x3d are same as for htype 0 */
#define PCI_BRIDGE_CONTROL	0x3e
#define  PCI_BRIDGE_CTL_PARITY	0x01	/* Enable parity detection on secondary interface */
#define  PCI_BRIDGE_CTL_SERR	0x02	/* The same for SERR forwarding */
#define  PCI_BRIDGE_CTL_NO_ISA	0x04	/* Disable bridging of ISA ports */
#define  PCI_BRIDGE_CTL_VGA	0x08	/* Forward VGA addresses */
#define  PCI_BRIDGE_CTL_MASTER_ABORT 0x20  /* Report master aborts */
#define  PCI_BRIDGE_CTL_BUS_RESET 0x40	/* Secondary bus reset */
#define  PCI_BRIDGE_CTL_FAST_BACK 0x80	/* Fast Back2Back enabled on secondary interface */

#define PCI_ANY_ID (~0)
#define PCI_FIXUP_HEADER    1   
#define PCI_FIXUP_FINAL             2 
#define PCI_INTERRUPT_PIN   0x3d    /* 8 bits */
#define PCI_INTERRUPT_LINE  0x3c    /* 8 bits */
#define PCI_ROM_ADDRESS             0x30    /* Bits 31..11 are address, 10..1 reserved */
#define PCI_SUBSYSTEM_VENDOR_ID     0x2c
#define PCI_SUBSYSTEM_ID    0x2e  
#define PCI_CB_SUBSYSTEM_VENDOR_ID 0x40
#define PCI_CB_SUBSYSTEM_ID 0x42



#define PCI_SLOT(devfn)             (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)             ((devfn) & 0x07)


/*
 *  For PCI devices, the region numbers are assigned this way:
 *
 *	0-5	standard PCI regions
 *	6	expansion ROM
 *	7-10	bridges: address space assigned to buses behind the bridge
 */

#define PCI_ROM_RESOURCE 6




struct pci_dev {
  struct pci_bus  *bus;           /* bus this device is on */
  struct pci_dev  *sibling;       /* next device on this bus */
  struct pci_dev  *next;          /* chain of all devices */
  
  void            *sysdata;       /* hook for sys-specific extension */
  struct proc_dir_entry *procent; /* device entry in /proc/bus/pci */
  
  unsigned int    devfn;          /* encoded device & function index */
  unsigned short  vendor;
  unsigned short  device;
  unsigned short  subsystem_vendor;
  unsigned short  subsystem_device;
  unsigned int    class;          /* 3 bytes: (base,sub,prog-if) */
  unsigned int    hdr_type;       /* PCI header type */
  unsigned int     master : 1;     /* set if device is master capable */
  uint8_t    irq;            /* irq generated by this device */
  unsigned long   base_address[6];
  unsigned long   rom_address;
  uint32_t extra; 
};

struct pci_bus {

  struct pci_bus  *parent;        /* parent bus this bridge is on */
  struct pci_bus  *children;      /* chain of P2P bridges on this bus */
  struct pci_bus  *next;          /* chain of all PCI buses */
  
  struct pci_dev  *self;          /* bridge device as seen by parent */
  struct pci_dev  *devices;       /* devices behind this bridge */
  
  void            *sysdata;       /* hook for sys-specific extension */
  struct proc_dir_entry *procdir; /* directory entry in /proc/bus/pci */

  uint8_t   number;         /* bus number */
  uint8_t   primary;        /* number of primary bridge */
  uint8_t   secondary;      /* number of secondary bridge */
  uint8_t   subordinate;    /* max number of subordinate buses */
};



/*

*/

struct pci_ops;

/*

*/


struct pci_device_id {
	unsigned int vendor, device;		/* Vendor and device ID or PCI_ANY_ID */
	unsigned int subvendor, subdevice;	/* Subsystem ID's or PCI_ANY_ID */
	unsigned int class, class_mask;		/* (class,subclass,prog-if) triplet */
	unsigned long driver_data;		/* Data private to the driver */
};


struct pci_fixup {
	int pass;
	uint16_t vendor, device;			/* You can use PCI_ANY_ID here of course */
	void (*hook)(struct pci_dev *dev);
};

#define pci_bus_b(n) list_entry(n, struct pci_bus, node)

extern struct list_head pci_root_buses;	/* list of all known PCI buses */
//extern struct list_head pci_devices;	/* list of all devices */
//FIRST 
extern struct pci_dev *pci_devices;
extern struct pci_fixup pcibios_fixups[];

/* Generic PCI functions used internally */

void pci_init(void);
/*
uint32_t pci_bus_exists(const struct list_head *list, int nr);
struct pci_bus *pci_scan_bus(int bus, struct pci_ops *ops, void *sysdata);
struct pci_bus *pci_alloc_primary_bus(int bus);
struct pci_dev *pci_scan_slot(struct pci_dev *temp);
int pci_proc_attach_device(struct pci_dev *dev);
int pci_proc_detach_device(struct pci_dev *dev);
void pci_name_device(struct pci_dev *dev);
char *pci_class_name(uint32_t class);
void pci_read_bridge_bases(struct pci_bus *child);
struct resource *pci_find_parent_resource(const struct pci_dev *dev, struct resource *res);
int pci_setup_device(struct pci_dev *dev);
int pci_get_interrupt_pin(struct pci_dev *dev, struct pci_dev **bridge);

*/

/* Generic PCI functions exported to card drivers */
/*
struct pci_dev *pci_find_device (unsigned int vendor, unsigned int device, const struct pci_dev *from);
struct pci_dev *pci_find_subsys (unsigned int vendor, unsigned int device,
				 unsigned int ss_vendor, unsigned int ss_device,
				 const struct pci_dev *from);
struct pci_dev *pci_find_class (unsigned int class, const struct pci_dev *from);
struct pci_dev *pci_find_slot (unsigned int bus, unsigned int devfn);
int pci_find_capability (struct pci_dev *dev, int cap);
*/


int pci_read_config_byte(uint8_t bus,uint8_t devfn, int where, uint8_t *val);
int pci_read_config_word(uint8_t bus,uint8_t devfn, int where, uint16_t *val);
int pci_read_config_dword(uint8_t bus,uint8_t devfn, int where, uint32_t *val);
int pci_write_config_byte(uint8_t bus,uint8_t devfn, int where, uint8_t *val);
int pci_write_config_word(uint8_t bus,uint8_t devfn, int where, uint16_t *val);
int pci_write_config_dword(uint8_t bus,uint8_t devfn, int where, uint32_t *val);

/*

*/

/*
int pci_enable_device(struct pci_dev *dev);
void pci_set_master(struct pci_dev *dev);
int pci_set_power_state(struct pci_dev *dev, int state);
int pci_assign_resource(struct pci_dev *dev, int i);
*/

/* Helper functions for low-level code (drivers/pci/setup-[bus,res].c) */
/*
  int pci_claim_resource(struct pci_dev *, int);
  void pci_assign_unassigned_resources(void);
  void pdev_enable_device(struct pci_dev *);
  void pdev_sort_resources(struct pci_dev *, struct resource_list *, uint32_t);
  unsigned long pci_bridge_check_io(struct pci_dev *);
  void pci_fixup_irqs(uint8_t (*)(struct pci_dev *, uint8_t *),
  int (*)(struct pci_dev *, uint8_t, uint8_t));
  #define HAVE_PCI_REQ_REGIONS 1
  int pci_request_regions(struct pci_dev *, char *);
  void pci_release_regions(struct pci_dev *);
*/
/*
static inline struct pci_dev *pci_find_slot(unsigned int bus, unsigned int devfn)
{ return NULL; }
*/

#define pci_for_each_dev(dev) \
	for(dev = NULL; 0; )








