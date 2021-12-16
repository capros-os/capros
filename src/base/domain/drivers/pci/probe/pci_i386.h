

#define PCI_PROBE_BIOS 1
#define PCI_PROBE_CONF1 2
#define PCI_PROBE_CONF2 4

/****** irq.c *******/
#define  PIRQ_SIGNATURE       (('$' << 0) + ('P' << 8) + ('I' << 16) + ('R' << 24))
#define PIRQ_VERSION 0x0100


/******irq.c ********/

void pcibios_enable_irq(struct pci_dev *dev);

void pcibios_irq_init(void);

struct irq_routing_table *pcibios_get_irq_routing_table(void);

struct irq_info {
        uint8_t bus, devfn;                  /* Bus, device and function */
        struct {
                uint8_t link;                /* IRQ line ID, chipset dependent, 0=not routed */
                uint16_t bitmap;             /* Available IRQs */
        }  irq[4];
        uint8_t slot;                        /* Slot number, 0=onboard */
        uint8_t rfu;
};/*** Check the importance of __attribute__ (packed) *****/

struct irq_routing_table {
        uint32_t signature;                  /* PIRQ_SIGNATURE should be here */
        uint16_t version;                    /* PIRQ_VERSION */
        uint16_t size;                       /* Table size in bytes */
        uint8_t rtr_bus, rtr_devfn;          /* Where the interrupt router lies */
        uint16_t exclusive_irqs;             /* IRQs devoted exclusively to PCI usage */
        uint16_t rtr_vendor, rtr_device;     /* Vendor and device ID of interrupt router */
        uint32_t miniport_data;              /* Crap */
        uint8_t rfu[11];
        uint8_t checksum;                    /* Modulo 256 checksum must give zero */
        struct irq_info slots[0];
};


