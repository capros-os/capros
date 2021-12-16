
#define IO_SPACE_LIMIT 0xffff

extern struct resource iomem_resource;

/*
 * Resources are tree-like, allowing
 * nesting etc..
 */
struct resource {
	const char *name;
	unsigned long start, end;
	unsigned long flags;
	struct resource *parent, *sibling, *child;
};
struct resource_list {
	struct resource_list *next;
	struct resource *res;
	struct pci_dev *dev;
};










