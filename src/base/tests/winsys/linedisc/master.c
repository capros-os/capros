/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* A simple primordial master domain that initializes the window
   system (and all associated domains) and then constructs a sample
   window system client domain. */

#undef MAKE_VMWARE_PUKE

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>

#include <idl/capros/Sleep.h>

#include <stdlib.h>

#include <domain/ConstructorKey.h>
#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <idl/capros/winsys/master.h>

#include <domain/EventMgrKey.h>
#include <domain/SessionCreatorKey.h>
#include <domain/SessionKey.h>
#include <domain/drivers/PciProbeKey.h>
#include <domain/drivers/ps2.h>

/* Testing */
#include <domain/LineDiscKey.h>

#include "constituents.h"

#define VMWareVENDORID 0x15AD

#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/

#define KR_PS2_DRIVER    KR_APP(2) /* The key for PS/2 driver */
#define KR_EVENT_MGR     KR_APP(3) /* The key for event mgr */
#define KR_WINDOW_SYS    KR_APP(4) /* The key for window system */
#define KR_PCI_PROBE     KR_APP(5) /* The key for the PCI prober */

#define KR_SESSION_CREATOR         KR_APP(6)
#define KR_TRUSTED_SESSION_CREATOR KR_APP(7)
#define KR_SESSION                 KR_APP(8)

#define KR_ETERM        KR_APP(9) /* The key for the client
				      constructor */
#define KR_TEST         KR_APP(10)

/* This is primordial stuff that a real window system client won't
   have to worry about. */
static uint32_t
probe_and_init_video(cap_t kr_winsys, cap_t kr_probe)
{
  struct pci_dev_data probe_result;

  uint32_t result;
  uint32_t device_id;
  uint32_t pci_bar;

  kprintf(KR_OSTREAM, "Master: ...calling pciprobe_initialize()...");
  result = pciprobe_initialize(kr_probe);

  if (result != RC_OK)
    return result;

  kprintf(KR_OSTREAM, "Master: ... calling pciprobe_firstdev()...");

  result = pciprobe_vendor_next(kr_probe, VMWareVENDORID, 
				0, &probe_result);

  if (result != RC_OK)
    return result;

  device_id = probe_result.device;
  pci_bar = probe_result.base_address[0];

  kprintf(KR_OSTREAM, "Master: Initializing window system...");
  return eros_domain_winsys_master_initialize(kr_winsys, kr_probe);
}

int
main(void)
{
  /* Get needed keys from our constituents node */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_WINDOW_SYS_C, KR_WINDOW_SYS);
  node_extended_copy(KR_CONSTIT, KC_EVENT_MGR_C, KR_EVENT_MGR);
  node_extended_copy(KR_CONSTIT, KC_PCI_PROBE_C, KR_PCI_PROBE);
  node_extended_copy(KR_CONSTIT, KC_ETERM_C, KR_ETERM);
  node_extended_copy(KR_CONSTIT, KC_TEST, KR_TEST);

  kprintf(KR_OSTREAM, "Master domain says hi ...\n");

  /* Do some primordial probing */
  kprintf(KR_OSTREAM, "About to construct PCI probe domain...");
  if (constructor_request(KR_PCI_PROBE, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_PCI_PROBE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct pci probe...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Do some more primordial stuff: construct the window system... */
  kprintf(KR_OSTREAM, "About to construct window system domain...");
  if (constructor_request(KR_WINDOW_SYS, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_WINDOW_SYS) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct window system...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* More primordial stuff: construct the window system's event
     manager... */
  kprintf(KR_OSTREAM, "About to construct event manager domain...");
  if (constructor_request(KR_EVENT_MGR, KR_BANK, KR_SCHED, KR_WINDOW_SYS, 
			  KR_EVENT_MGR) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct event manager...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Initialize the video subsystem */
  if (probe_and_init_video(KR_WINDOW_SYS, KR_PCI_PROBE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: probing/init'ing video subsystem...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  if (eros_domain_winsys_master_get_session_creators(KR_WINDOW_SYS, 
						KR_TRUSTED_SESSION_CREATOR,
						KR_SESSION_CREATOR) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: getting session creator keys.\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

    /* Now construct the sample client, passing it the key to an
       appropriate session creator. This client will ask the window
       system for a window, and then kick off various "child" domains
       to prepare to handle requests from the line discipline domain. */
  if (constructor_request(KR_ETERM, KR_BANK, KR_SCHED, KR_SESSION_CREATOR,
			  KR_ETERM) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct eterm.\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "Master: now constructing test...\n");
  if (constructor_request(KR_TEST, KR_BANK, KR_SCHED, KR_ETERM,
			  KR_VOID) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct test domain.\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "Master's work is done.\n");

  return 0;
}

