/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System distribution,
 * and is derived from the EROS Operating System distribution.
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

/* A simple paint program that runs as an untrusted client of the
   window system.  Use left mouse to draw pixels and right mouse to
   clear drawn pixels. */

#undef TRACE
#undef SHAP			/* Shap doesn't have *this* working (yet) */

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>

#include <idl/capros/Process.h>
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

#include <graphics/color.h>
#include <graphics/rect.h>
#include <graphics/fonts/Font.h>

#include <addrspace/addrspace.h>

#include "constituents.h"

#define VMWareVENDORID 0x15AD

/* The largest size any domain currently has for sharing or otherwise
mapping memory is 1GB.  This domain maps an array of 32-bit values
into that region and the offset into this region starts at zero. */
#define _1GB_ 0x40000000u
#define MAX_BUFFER_OFFSET ((_1GB_ / sizeof(uint32_t)) - 1)

/*  This test domain acts like the primordial "master" domain whose
    responsibilities include probing for devices, initializing
    appropriate drivers, constructing the display manager
    domain and passing it the keys to the initialized drivers, and
    then constructing the window system and passing it the appropriate
    key to the display manager. 

    If you want to view the serial output from this program
    (everything that's printed via "kprintf"), redirect CON1 of your
    VMWare to either a file or a TTY.  If to a TTY, you can use "minicom"
    in a shell window to view the output.
 */

#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/

#define KR_SLEEP         KR_APP(1) /* A capability for sleeping */
#define KR_PS2_DRIVER_C  KR_APP(2) /* The constructor key for PS/2 driver */
#define KR_INPUT_DRIVER  KR_APP(3) /* The start key for the PS/2 driver */
#define KR_EVENT_MGR_C   KR_APP(4) /* The constructor key for event mgr */
#define KR_EVENT_MGR     KR_APP(5) /* The start key for the event mgr */
#define KR_WINDOW_SYS_C  KR_APP(6) /* The constructor key for window
				      system */
#define KR_WINDOW_SYS    KR_APP(7) /* The start key for the window
				       system */

#define KR_SESSION_CREATOR         KR_APP(8)
#define KR_TRUSTED_SESSION_CREATOR KR_APP(9)
#define KR_PCI_PROBE_C             KR_APP(10)
#define KR_PCI_PROBE               KR_APP(11)

#define KR_NEW_SESSION   KR_APP(12)
#define KR_SCRATCH       KR_APP(13)
#define KR_NEW_WINDOW    KR_APP(14)
#define KR_NEW_SUBSPACE  KR_APP(15)

#define KR_SUB_BANK      KR_APP(16)

/* globals */
uint32_t main_width = 500;
uint32_t main_height = 350;
uint32_t main_window_id = 0;
uint32_t *main_content = NULL;
FontType *myFont = NULL;

cap_t kr_ostream = KR_OSTREAM;

#ifdef SHAP
static void
session_release_mapping(cap_t kr_sess, uint32_t winid)
{
  Message m;

  memset(&m, 0, sizeof(Message));
  m.snd_invKey = kr_sess;
  m.snd_code = 1999;
  m.rcv_rsmkey = KR_VOID;
  m.snd_w2 = winid;
  CALL(&m);
}
#endif

/* Do a user-friendly resize of the current picture.  This example
   only preserves the current drawing at the current aspect ratio.  It
   does not attempt to *scale* the current drawing.  This is merely
   here as an example of one way of handling a resize event. */
static void
resize(uint32_t old_width, uint32_t old_height)
{
  uint32_t x;
  uint32_t y;

  /* Use the very "end" of the mapped area as a temporary buffer for
  storing the curren picture.  Note: this won't work once we start
  using this offscreen memory for things like a gl-command buffer! */
  uint32_t offscreen = MAX_BUFFER_OFFSET - (old_width * old_height);

  /* Make sure there's enough room to do this maneuver. (i.e. make
     sure we're truly using offscreen memory, based on the new window
     dimensions. */
  if (offscreen < (main_width * main_height))
    return;

  /* Copy the current picture to the offscreen area of memory */
  for (y = 0; y < old_height; y++)
    for (x = 0; x < old_width; x++) {
      main_content[offscreen + (x + old_width * y)] = 
	main_content[x + old_width * y];
    } 

  /* Now copy the picture back to the main canvas, using the new
     dimensions */
  for (y = 0; y < main_height; y++)
    for (x = 0; x < main_width; x++) {
      if (x < old_width && y < old_height)
        main_content[x + main_width * y] = 
	  main_content[offscreen + (x + old_width * y)];
      else
	main_content[x + main_width * y] = BLACK;
    }
}

/* Ask window system to redraw our window contents */
static void
update(rect_t area)
{
  (void) session_win_redraw(KR_NEW_SESSION, main_window_id,
			    area.topLeft.x, area.topLeft.y,
			    area.bottomRight.x, area.bottomRight.y);
}

/* Clear a specified rectangular region of our window to the specified
   color */
static void
doClear(rect_t area, color_t color)
{
  uint32_t x, y;

  for (x = area.topLeft.x; x < area.bottomRight.x; x++)
    for (y = area.topLeft.y; y < area.bottomRight.y; y++)
       main_content[x + y * main_width] = color;
}

/* Clear the entire window to a predetermined color */
static void
clear(void)
{
  rect_t area = { {0,0}, {main_width, main_height} };

  doClear(area, BLACK);
}

/* Use a crude font library to render the painting instructions in the
   upper left corner of the window */
static void
draw_instructions(void)
{
  point_t location = {10,10};

  font_render(myFont,(uint8_t *)main_content, main_width, 
	      location, WHITE, BLACK, FONT_NORMAL,
	      "Use Left Mouse to draw, "
	      "Right Mouse to clear...");
}

/* Paint a filled 1-pixel-radius circle (or cross depending on how you alias)
   of the specified color using a center specified by "pixel". */ 
static void
paint_pixel(point_t pixel, color_t color)
{
  uint32_t x, y;

#ifdef TRACE
  kprintf(KR_OSTREAM, "Test: paint_pixel() with (%d,%d) and 0x%x\n",
	  pixel.x, pixel.y, color);
#endif

  /* Draw a 1-pixel-radius circle around the selected pixel */
  for (x = pixel.x-1; x < pixel.x+1; x++) {
    main_content[x + pixel.y * main_width] = color;
  }

  for (y = pixel.y-1; y < pixel.y+1; y++) {
    main_content[pixel.x + y * main_width] = color;
  }
}

/* This is primordial stuff that a real window system client won't
   have to worry about. */
static uint32_t
probe_and_init_video(cap_t kr_winsys, cap_t kr_probe)
{
  kprintf(KR_OSTREAM, "Paint: Initializing window system...");
  return eros_domain_winsys_master_initialize(kr_winsys, kr_probe);
}

int
main(void)
{
  uint32_t result;
  uint32_t u;

  /* Get needed keys from our constituents node */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  node_extended_copy(KR_CONSTIT, KC_EVENT_MGR_C, KR_EVENT_MGR_C);
  node_extended_copy(KR_CONSTIT, KC_WINDOW_SYS_C, KR_WINDOW_SYS_C);
  node_extended_copy(KR_CONSTIT, KC_PCI_PROBE_C, KR_PCI_PROBE_C);

  kprintf(KR_OSTREAM, "Paint domain says hi ...\n");

  /* Use the default font */
  myFont = font_find_font("Default");
  if (myFont == NULL) {
    kprintf(KR_OSTREAM, "**ERROR: Test domain couldn't get default font!\n");
    return -1;
  }

  /* Prepare our default address space for mapping in additional
     memory. */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, KR_NEW_SUBSPACE);

  /* Create a subbank to use for window creation */
  if (spcbank_create_subbank(KR_BANK, KR_SUB_BANK) != RC_OK) {
    kprintf(KR_OSTREAM, "Test domain failed to create sub bank.\n");
    return -1;
  }

  /* Do some primoridial probing */
  kprintf(KR_OSTREAM, "About to construct PCI probe domain...");
  if (constructor_request(KR_PCI_PROBE_C, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_PCI_PROBE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct pci probe...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Do some more primordial stuff: construct the window system... */
  kprintf(KR_OSTREAM, "About to construct window system domain...");
  if (constructor_request(KR_WINDOW_SYS_C, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_WINDOW_SYS) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct window system...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* More primordial stuff: construct the window system's event
     manager... */
  kprintf(KR_OSTREAM, "About to construct event manager domain...");
  if (constructor_request(KR_EVENT_MGR_C, KR_BANK, KR_SCHED, KR_WINDOW_SYS, 
			  KR_EVENT_MGR) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct display manager...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "All domains successfully constructed.\n");

  /* Initialize the video subsystem */
  if (probe_and_init_video(KR_WINDOW_SYS, KR_PCI_PROBE) == RC_OK) {

    eros_domain_winsys_master_get_session_creators(KR_WINDOW_SYS, 
				   KR_TRUSTED_SESSION_CREATOR,
				   KR_SESSION_CREATOR);

    if (session_creator_new_session(KR_SESSION_CREATOR, KR_BANK, KR_NEW_SESSION) == RC_OK)
      kprintf(KR_OSTREAM, "Paint:: successfully created session!\n");



    /* Here's where the real window system client stuff begins... */
    result = session_new_window(KR_NEW_SESSION,
				KR_SUB_BANK,
				DEFAULT_PARENT,
				50,
				50,
				main_width,
				main_height,
				(WINDEC_TITLEBAR |
				 WINDEC_BORDER),
				&main_window_id,
				KR_NEW_WINDOW);

    if (result != RC_OK) {
      kprintf(KR_OSTREAM, "Error: couldn't create new window; result=%u",
	      result);
      return -1;
    }

    /* Change the default window title to something meaningful */	  
    session_win_set_title(KR_NEW_SESSION, main_window_id, 
			  "/usr/bin/X11/KangaPaint");

    kprintf(KR_OSTREAM, "Paint: created window id [%d]\n", main_window_id);

    /* Map the new addr space into this domain's space */
    capros_Process_getAddrSpace(KR_SELF, KR_SCRATCH);
    node_swap(KR_SCRATCH, 16, KR_NEW_WINDOW, KR_VOID);

    /* In order to access this mapped space, this domain needs a
    well-known address:  use the well-known address that corresponds
    to slot 16 :*/
    main_content = (uint32_t *)0x80000000u;

    /* In order to make full use of the entire 1GB space (for resizing
       or doing other "offscreen memory" tasks), we need
       local window keys in the rest of the available slots */
    for (u = 17; u < 24; u++)
      addrspace_insert_lwk(KR_SCRATCH, 16, u, EROS_ADDRESS_LSS);

#if 0
    kprintf(KR_OSTREAM, "Test: clearing mapped memory...\n");

    kprintf(KR_OSTREAM, "   ... trying 0x%08x:\n", &(main_content[0x0200000]));
    main_content[0x0200000] = BLACK;

    kprintf(KR_OSTREAM, "   ... trying 0x%08x:\n", &(main_content[0x0400000]));
    main_content[0x0400000] = BLACK;

    kprintf(KR_OSTREAM, "   ... trying 0x%08x:\n", &(main_content[0x0600000]));
    main_content[0x0600000] = BLACK;

    kprintf(KR_OSTREAM, "   ... trying 0x%08x:\n", &(main_content[0x0800000]));
    main_content[0x00800000] = BLACK;

    kprintf(KR_OSTREAM, "   ... trying 0x%08x:\n", &(main_content[0x1000000]));
    main_content[0x01000000] = BLACK;

    kprintf(KR_OSTREAM, "   ... trying 0x%08x:\n", &(main_content[0x2000000]));
    main_content[0x02000000] = BLACK;
#endif

    clear();
    draw_instructions();

    /* Tell the window system that we want to display our new
       window */
    session_win_map(KR_NEW_SESSION, main_window_id);

#ifdef SHAP
    kprintf(KR_OSTREAM, "Paint: about to destroy subbank...\n");

    if (spcbank_destroy_bank(KR_SUB_BANK, 1) != RC_OK)
      kprintf(KR_OSTREAM, "** Couldn't destroy subbank...\n");

    /* Temporary call to test process keeper invocation */
    session_release_mapping(KR_NEW_SESSION, main_window_id);

    kprintf(KR_OSTREAM, "Paint: after whacking sub bank:\n");
#endif

    /* Now just process events forever */
    for (;;) {
      Event evt;

      /* Events are dispatched on Sessions... */
      result = session_next_event(KR_NEW_SESSION, &evt);
      if (result != RC_OK)
	kprintf(KR_OSTREAM, "** ERROR: session_next_event() "
		"result=%u", result);
      else {
#ifdef TRACE
	kprintf(KR_OSTREAM, "Received %s event!",
		(evt.type == Resize ? "RESIZE" :
		 (evt.type == Mouse ? "MOUSE" : "KEY")));
#endif

	if (evt.window_id != main_window_id)
	  continue;

	/* Mouse events */

	if (IS_MOUSE_EVENT(evt)) {

	  if (EVENT_BUTTON_MASK(evt) & MOUSE_LEFT) {
	    point_t pt = { EVENT_CURSOR_X(evt), EVENT_CURSOR_Y(evt) };
	    rect_t area = { {pt.x-1, pt.y-1}, {pt.x+1, pt.y+1} };

	    paint_pixel(pt, WHITE);
	    update(area);
	  }
	  else if (EVENT_BUTTON_MASK(evt) & MOUSE_RIGHT) {
	    rect_t area = { {0,0}, {main_width, main_height} };

	    clear();
	    draw_instructions();
	    update(area);
	  }
	}

	/* Keyboard events */

	else if (IS_KEY_EVENT(evt)) {
#ifdef TRACE 
	  kprintf(KR_OSTREAM, "Key event: %d\n", evt.data[0]);
#endif
	}

	/* Resize events */

	else if (IS_RESIZE_EVENT(evt)) {
	  rect_t area;
	  uint32_t old_width = main_width;
	  uint32_t old_height = main_height;

	  main_width = EVENT_NEW_WIDTH(evt);
	  main_height = EVENT_NEW_HEIGHT(evt);

	  area.topLeft.x = 0;
	  area.topLeft.y = 0;
	  area.bottomRight.x = main_width;
	  area.bottomRight.y = main_height;

	  resize(old_width, old_height);
	  update(area);
	}
      }
    }
  }
  return 0;
}

