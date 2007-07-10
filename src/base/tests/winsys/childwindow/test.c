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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/KeyConst.h>

#include <idl/capros/Sleep.h>

#include <stdlib.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>

/* Include the needed interfaces */
#include <idl/capros/winsys/master.h>
#include <domain/SessionCreatorKey.h>
#include <domain/SessionKey.h>

#include <domain/EventMgrKey.h>
#include <domain/drivers/PciProbeKey.h>

#include <domain/drivers/ps2.h>

#include <graphics/rect.h>
#include <graphics/color.h>
#include <addrspace/addrspace.h>

#include "constituents.h"

#define VMWareVENDORID 0x15AD

/* This test demonstrates how a child window works.  It creates a
top-level window, then a child window within that top-level.  Note
that the application implements the policy for how children are drawn
with respect to resize events. */

#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/

#define KR_SLEEP         KR_APP(1) /* A capability for sleeping */
#define KR_EVENT_MGR_C   KR_APP(2) /* The constructor key for display mgr */
#define KR_EVENT_MGR     KR_APP(3) /* The start key for the display mgr */
#define KR_WINDOW_SYS_C  KR_APP(4) /* The constructor key for window
				      system */
#define KR_WINDOW_SYS    KR_APP(5) /* The start key for the window
				      system */

#define KR_SESSION_CREATOR         KR_APP(6)
#define KR_TRUSTED_SESSION_CREATOR KR_APP(7)
#define KR_PCI_PROBE_C             KR_APP(8)
#define KR_PCI_PROBE               KR_APP(9)

#define KR_SESSION           KR_APP(10)
#define KR_TRUSTED_SESSION   KR_APP(11)

#define KR_WINDOW            KR_APP(12)

#define KR_NEW_SUBSPACE      KR_APP(13)
#define KR_SCRATCH           KR_APP(14)

#define KR_SUB_BANK          KR_APP(15)

#define COLOR1 0x00091cbd
#define COLOR2 0x008c4da3
#define COLOR3 0x000bd10f

//#define TRACE
//#define USE_DEFAULT_WINDOW

#define _2GB_   0x80000000u
#define _128MB_ 0x08000000u

/* globals */
uint32_t parent;
uint32_t child;
uint32_t *window_contents = (uint32_t *)_2GB_;

point_t parent_orig = {200, 350};
point_t child_orig  = {50, 50};
point_t parent_size = {500, 500};
point_t child_size = {300, 250};
color_t parent_color = COLOR1;
color_t child_color = COLOR2;

/* This will eventually be done by some primordial "master" domain */
static uint32_t
probe_and_init_video(cap_t kr_winsys, cap_t kr_probe)
{
  struct pci_dev_data probe_result;

  uint32_t result;
  uint32_t device_id;

  uint32_t pci_bar;
  uint32_t probe_info[2];

  kprintf(KR_OSTREAM, "...calling pciprobe_initialize()...");
  result = pciprobe_initialize(kr_probe);

  if (result != RC_OK)
    return result;

  kprintf(KR_OSTREAM, "... calling pciprobe_firstdev()...");

  result = pciprobe_vendor_next(kr_probe, VMWareVENDORID, 
				0, &probe_result);

  if (result != RC_OK)
    return result;

  device_id = probe_result.device;
  pci_bar = probe_result.base_address[0];

  probe_info[0] = device_id;
  probe_info[1] = pci_bar;

  kprintf(KR_OSTREAM, "Test: Initializing window system...");
  result = eros_domain_winsys_master_initialize(kr_winsys, kr_probe);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: window_system_initialize returned 0x%08x\n",
	    result);
    return -1;
  }

  return result;
}

/* Convenient routine to clear the entire window to a predefined color */
void
win_clear(uint32_t offset, point_t win_size, color_t color)
{
  uint32_t x, y;
  uint32_t *fb = (uint32_t *)((uint8_t *)window_contents + 
			      (uint32_t)(offset * _128MB_));

  for (x = 0; x < win_size.x; x++)
    for (y = 0; y < win_size.y; y++)
      fb[x + y * win_size.x] = color;

}

int
main(void)
{
  /* Copy necessary keys from constituents node to this domain's key
     register set */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  node_extended_copy(KR_CONSTIT, KC_EVENT_MGR_C, KR_EVENT_MGR_C);
  node_extended_copy(KR_CONSTIT, KC_WINDOW_SYS_C, KR_WINDOW_SYS_C);
  node_extended_copy(KR_CONSTIT, KC_PCI_PROBE_C, KR_PCI_PROBE_C);

  kprintf(KR_OSTREAM, "Test domain says hi...\n");

  /* Set up this domain's local address space to support mapping both
  shared memory (the window contents buffers) */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, KR_NEW_SUBSPACE);

  /* Run through constructors... these should eventually be done by
     master domain */
  kprintf(KR_OSTREAM, "About to construct PCI probe domain...");
  if (constructor_request(KR_PCI_PROBE_C, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_PCI_PROBE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct pci probe...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "About to construct window system domain...");
  if (constructor_request(KR_WINDOW_SYS_C, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_WINDOW_SYS) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct window system...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "About to construct event manager domain...");
  if (constructor_request(KR_EVENT_MGR_C, KR_BANK, KR_SCHED, KR_WINDOW_SYS, 
			  KR_EVENT_MGR) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct event manager...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "All domains successfully constructed.\n");

  /* Initialize the video subsystem */
  if (probe_and_init_video(KR_WINDOW_SYS, KR_PCI_PROBE) == RC_OK) {

    spcbank_create_subbank(KR_BANK, KR_SUB_BANK);

    /* Now try to create a new session */
    if (eros_domain_winsys_master_get_session_creators(KR_WINDOW_SYS, 
				       KR_TRUSTED_SESSION_CREATOR,
				       KR_SESSION_CREATOR) == RC_OK) {
      uint32_t result;

      result = session_creator_new_session
	(KR_TRUSTED_SESSION_CREATOR,
	 KR_SUB_BANK,
	 KR_TRUSTED_SESSION);

      if (result != RC_OK) {
	kprintf(KR_OSTREAM, "Error: couldn't create new session; result=0x%x",
		result);
	return -1;
      }

      result = session_creator_new_session
	(KR_SESSION_CREATOR, KR_SUB_BANK, KR_SESSION);

      if (result != RC_OK) {
	kprintf(KR_OSTREAM, "Error: couldn't create new session; result=0x%x",
		result);
	return -1;
      }

#ifdef TRACE
      kprintf(KR_OSTREAM, "New trusted session created!");
      kprintf(KR_OSTREAM, "New untrusted session created!");
#endif

      /* Prepare to insert subspaces into ProcAddrSpace */
      process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);

      /* Create main window */
      result = session_new_window(KR_SESSION,
				  KR_SUB_BANK,
				  DEFAULT_PARENT,
				  parent_orig.x,
				  parent_orig.y,
				  parent_size.x,
				  parent_size.y,
				  true,
				  &parent,
				  KR_WINDOW);


      if (result != RC_OK) {
	kprintf(KR_OSTREAM, "Error: couldn't create main window; ret=0x%08x",
		result);
	return -1;
      }

      session_win_set_title(KR_SESSION, parent, "Parent Window");

      /* Now map main window's subspace to the local address space */
      node_swap(KR_SCRATCH, 16, KR_WINDOW, KR_VOID);

      /* Clear this window to a unique background color */
      win_clear(0, parent_size, COLOR1);

      /* Create child window of the main window */
      result = session_new_window(KR_SESSION,
				  KR_SUB_BANK,
				  parent,
				  child_orig.x,
				  child_orig.y,
				  child_size.x,
				  child_size.y,
				  true,
				  &child,
				  KR_WINDOW);


      if (result != RC_OK) {
	kprintf(KR_OSTREAM, "Error: couldn't create child window; ret=0x%08x",
		result);
	return -1;
      }

      session_win_set_title(KR_SESSION, child, "Child Window");

      /* Now map child window's subspace to the local address space */
      node_swap(KR_SCRATCH, 17, KR_WINDOW, KR_VOID);

      /* Clear this window to a unique background color */
      win_clear(1, child_size, COLOR2);

      /* Tell the window system that we want to display our new
	 windows */
      session_win_map(KR_SESSION, child);
      session_win_map(KR_SESSION, parent);

      /* Now just wait for and process window system events */
      for (;;) {
	Event evt;

	/* Events are dispatched on Sessions... */
	result = session_next_event(KR_SESSION, &evt);
	if (result != RC_OK)
	  kprintf(KR_OSTREAM, "** ERROR: session_next_event() "
		  "result=%u", result);
	else {
#ifdef TRACE
	  kprintf(KR_OSTREAM, "Received %s event!",
		   (evt.type == Mouse ? "MOUSE" : 
		    (evt.type == Key ? "KEY" : "RESIZE")));
#endif

	  if (IS_RESIZE_EVENT(evt)) {

	    if (evt.window_id == parent) {
	      /* Update the window's origin/size accordingly */
	      parent_orig.x = evt.data[0];
	      parent_orig.y = evt.data[1];
	      parent_size.x = evt.data[2];
	      parent_size.y = evt.data[3];

	      win_clear(0, parent_size, COLOR1);
#ifdef TRACE
	      kprintf(KR_OSTREAM, "TEST: calling redraw for PARENT...\n");
#endif
	      session_win_redraw(KR_SESSION, parent, 0, 0,
				 parent_size.x, parent_size.y);

	    }
	    else if (evt.window_id == child) {
	      child_orig.x = evt.data[0];
	      child_orig.y = evt.data[1];
	      child_size.x = evt.data[2];
	      child_size.y = evt.data[3];

	      win_clear(1, child_size, COLOR2);
#ifdef TRACE
	      kprintf(KR_OSTREAM, "TEST: calling redraw for CHILD...\n");
#endif
	      session_win_redraw(KR_SESSION, child, 0, 0,
				 child_size.x, child_size.y);

	    }
	    else 
              kprintf(KR_OSTREAM, "No such window id: 0x%08x\n",
		      evt.window_id);
	  }
	}
      }
    }
  }
  return 0;
}

