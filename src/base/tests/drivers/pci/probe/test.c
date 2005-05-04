#include <stddef.h>
#include <eros/target.h>
#include <eros/NodeKey.h>
#include <eros/Invoke.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

#include <domain/drivers/PciProbeKey.h>

#include "constituents.h"

#define KR_OSTREAM      KR_APP(0)
#define KR_PCI_PROBE_C  KR_APP(1)
#define KR_PCI_PROBE    KR_APP(2)

#define VENDOR_VMWARE 0x15ad
#define VENDOR_INTEL  0x8086
#define VIDEO_CARD    0x03u
int 
main(void)
{
  uint32_t result,total;
  unsigned short  vendor;
  uint32_t baseClass;
  //  unsigned short device;
//  struct pci_dev_data rcv_dev;
  total = 0;
  vendor = VENDOR_VMWARE;
  //vendor = VENDOR_INTEL;
  baseClass = (uint32_t)VIDEO_CARD;
  node_extended_copy(KR_CONSTIT, KC_OSTREAM,   KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_PCI_PROBE_C,  KR_PCI_PROBE_C);

  constructor_request(KR_PCI_PROBE_C,KR_BANK,KR_SCHED,KR_VOID,KR_PCI_PROBE_C);

  /********* OC_INIT **************************/

  kprintf(KR_OSTREAM,"Before Initializing PCI routine.\n");
  result = pciprobe_initialize(KR_PCI_PROBE_C);
  kprintf(KR_OSTREAM,"Invocation for OC_INIT returned %s.\n",
	  (result ==RC_OK) ? "SUCCESS" : "FAILED");


  /***********OC_TOTAL ************************/
#if 0
  kprintf(KR_OSTREAM,"Before get Total  routine .\n");
  result = pciprobe_vendor_total(KR_PCI_PROBE_C, vendor, &total);
  kprintf(KR_OSTREAM,"Invocation for OC_Pci_Find_VendorID_Total "
	  "returned %s.\n",(result ==RC_OK) ? "SUCCESS" : "FAILED");
  kprintf(KR_OSTREAM,"Total Number for  vendor %04x device is  = %d\n", 
	  vendor, total);

  /***********************************/

  kprintf(KR_OSTREAM,"Before Get Device PCI routine.\n");
  result = pciprobe_vendor_next(KR_PCI_PROBE_C, vendor, 
				(vendor == VENDOR_INTEL) ? 4 : 0,
				&rcv_dev);
  kprintf(KR_OSTREAM,"Invocation for OC_Pci_Find_VendorID returned %s.\n",
	  (result ==RC_OK) ? "SUCCESS" : "FAILED");
  kprintf(KR_OSTREAM,"RCV: 00:%02x [%04x/%04x] BR[0] %06d\n", 
	  rcv_dev.devfn, rcv_dev.vendor,
	  rcv_dev.device,rcv_dev.base_address[0]);

  /***********************************/     
#endif

  /***********OC_TOTAL ************************/

  kprintf(KR_OSTREAM,"Before get Total  routine .\n");
  kprintf(KR_OSTREAM,"base classID b4 %6x.\n", baseClass);

  result = pciprobe_base_class_total(KR_PCI_PROBE_C, baseClass, &total);
  kprintf(KR_OSTREAM,"Invocation for OC_Pci_Find_Base_ClassID_Total "
	  "returned %s.\n",(result ==RC_OK) ? "SUCCESS" : "FAILED");
  kprintf(KR_OSTREAM,"Total Number for  base class %02x device is  = %d\n", 
	  baseClass, total);

#if 0
  /***********************************/
  kprintf(KR_OSTREAM,"Before Get Device PCI routine.\n");
  result = pciprobe_class_next(KR_PCI_PROBE_C, class, 
				(vendor == VENDOR_INTEL) ? 4 : 0,
				&rcv_dev);
  kprintf(KR_OSTREAM,"Invocation for OC_Pci_Find_ClassID returned %s.\n",
	  (result ==RC_OK) ? "SUCCESS" : "FAILED");
  kprintf(KR_OSTREAM,"RCV: 00:%02x [%04x/%04x] BR[0] %06d\n", 
	  rcv_dev.devfn, rcv_dev.vendor,
	  rcv_dev.device,rcv_dev.base_address[0]);

  /***********************************/     
#endif

  return 0;
}
