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
#include <string.h>
#include <stdlib.h>

#include <eros/target.h>
#include <eros/stdarg.h>
#include <eros/Invoke.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/cap-instr.h>

#include <idl/capros/key.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>
#include <domain/drivers/PciProbeKey.h>

/* Need the base interface for this domain */
#include <idl/capros/winsys/master.h>

/* Need the Drawable interface */
#include <domain/DrawableKey.h>

/* Need the Session Creator interface */
#include <domain/SessionCreatorKey.h>

/* Need the Session interface */
#include <idl/capros/winsys/session.h>
#include "sessionmgr/SessionRequest.h"

/* Need the scancode converter */
#include "keytrans/keytrans.h"

#include <forwarder.h>

#include <addrspace/addrspace.h>

#include "sessionmgr/Session.h"
#include "coordxforms.h"

#include "global.h"

#include "constituents.h"
#include "debug.h"

/* process keeper support */
#include "keeper/winsys-keeper.h"

/* video driver support */
#include "fbmgr/drivers/video.h"
#include "fbmgr/drivers/drivers.h"

/* rbtree support */
#include "id_tree.h"

/* cursors */
#include "pixmaps/ids.h"
#include "cursors/f117_cursor.h"
#include "cursors/f117_alpha_cursor.h"

#include "winsyskeys.h"

/* Window specific code */
#include "Window.h"
#include "Invisible.h"
#include "xclipping.h"

/* Copy/Paste buffer support. FIX: These defines are only valid until
   we change our key translator code! */
#define COPY_INDICATOR  0x0003
#define PASTE_INDICATOR 0x0016
#include "PasteBuffer.h"
PasteBufferState  pastebuffer_state;

/* external globals */
extern unsigned fbfault_received(); /* declared in fbfault_recovery.c */

/* globals */
Window *Root = NULL;
static bool dragging = false;	/* for dragging the cursor */

point_t cursor = { 0, 0 };
uint32_t current_cursor_id = 0;
uint32_t root_width = 0;
uint32_t root_height = 0;
uint32_t root_depth = 0;

cap_t kr_ostream = KR_OSTREAM;

/* virtual address(es) for shared memory */
uint32_t *client_space;
#define CLIENT_MAX     0x00800000u /* 8 MB max window: 2048x1024x32 */
#define CLIENT_OFFSET  0x08000000u /* 128 MB between LSS 4 slots */

uint32_t receive_buffer[1024];
uint32_t send_buffer[10];
Event send_event;

/* global value to keep track of which window has focus */
Window *focus = NULL;

/* pixmap ids */
#define WINDOWS_BACKGROUND_ID  0
#define WINDOWS_LOGO_ID        1

const uint32_t __rt_stack_pages = 4;

static uint32_t
winsys_map_lss_three_layer(cap_t kr_self, cap_t kr_bank,
			   cap_t kr_scratch, cap_t kr_node, uint32_t next_slot)
{
  uint32_t slot;
  uint32_t lss_three = 3;
  uint32_t result;

  process_copy(kr_self, ProcAddrSpace, kr_scratch);
  for (slot = next_slot; slot < EROS_NODE_SIZE; slot++) {
    result = addrspace_new_space(kr_bank, lss_three, kr_node);
    if (result != RC_OK)
      return result;

    result = node_swap(kr_scratch, slot, kr_node, KR_VOID);
    if (result != RC_OK)
      return result;
  }
  return RC_OK;
}

/* Update the (x, y) of the cursor based on the deltas from
   the input device driver.*/
static void
update_cursor(int8_t x_delta, int8_t y_delta)
{
  int32_t check_x = cursor.x + x_delta;
  int32_t check_y = cursor.y - y_delta;

  if (check_x < 1) 
    cursor.x = 1;
  else { 
    if (check_x < Root->size.x)
      cursor.x = check_x;
    else
      cursor.x = Root->size.x-1;
  }

  if (check_y < 1)
    cursor.y = 1;
  else {
    if (check_y < Root->size.y)
      cursor.y = check_y;
    else
      cursor.y = Root->size.y-1;
  }

  video_show_cursor_at(current_cursor_id, cursor);
}

/* Here is the logic for handling changing focus from one window to
   the next. The 'focus' variable is global. */
void
winsys_change_focus(Window *from, Window *to)
{
  /* Never give an invisible window focus! */
  if (to->type == WINTYPE_INVISIBLE)
    return;

  DEBUG(focus) kprintf(KR_OSTREAM, "winsys_change_focus() from [0x%08x] "
		       "to [0x%08x]\n", ADDRESS(from), ADDRESS(to));

  focus = to;
  from->set_focus(from, false);
  to->set_focus(to, true);
}

/* Based on the cursor position, find the first (with respect to
   depth) containing window.  Note that it doesn't matter what kind of
   window it is!  Each window knows how to handle focus. */
Window *
find_containing_window(Link *head, point_t *cursor, uint32_t *copy_paste)
{
  Link *item = head->next;	/* sibchain */
  Window *w = NULL;
  Window *tmp = NULL;

  DEBUG(recursion) kprintf(KR_OSTREAM, "Inside find_containing_window() with "
		       "head = 0x%08x", ADDRESS(head));

  *copy_paste = IS_NORMAL;	/* until determined otherwise */

  while (ADDRESS(item) != ADDRESS(head)) {
    DEBUG(recursion) kprintf(KR_OSTREAM, "   ... item = 0x%08x", 
			     ADDRESS(item));
    w = (Window *)item;
    if (w->mapped) {
      rect_t r;

      /* Always check the children chain first */
      DEBUG(recursion) 
        kprintf(KR_OSTREAM, "    ... checking children windows");

      tmp = find_containing_window(&(w->children), cursor, copy_paste);
      if (!IS_ROOT(tmp)) {
	DEBUG(recursion) 
          kprintf(KR_OSTREAM, "   ... returning tmp [0x%08x]\n",
		  ADDRESS(tmp));
	return tmp;
      }

      /* Make sure window is clipped to its parent! */
      window_clip_to_ancestors(w, &r);
      if (rect_contains_pt(&r, cursor)) {
	DEBUG(recursion) kprintf(KR_OSTREAM, "   return w [0x%08x]\n",
				 ADDRESS(w));

	/* Never return an INVISIBLE window as the "cursor window"
	because invisible windows *never* get events.  However, do
	check if the invisible window is a designated copy or paste
	region and set a flag accordingly. */
	if (w->type == WINTYPE_INVISIBLE) {
	  Invisible *inv = (Invisible *)w;
	  *copy_paste = inv->qualifier;
	}
	else 
	  return w;
      }
    }
    item = item->next;
  }

  /* Worst case, Root gets returned */
  DEBUG(recursion) kprintf(KR_OSTREAM, "    ... returning Root");
  return Root;
}

void
dump_windows(Window *win)
{
  Link *l = Root->children.next;
  Window *w = NULL;

  kprintf(KR_OSTREAM, "Window of interest: 0x%08x\n", ADDRESS(win));
  kprintf(KR_OSTREAM, "(dump) Root=0x%08x Root->children=0x%08x", 
	  ADDRESS(Root), ADDRESS(&(Root->children)));
  while (ADDRESS(l) != ADDRESS(&(Root->children))) {
    w = (Window *)l;
    if (w == NULL) kprintf(KR_OSTREAM, "(dump): *** NULL WINDOW! ***\n");
    kprintf(KR_OSTREAM, "(dump): winid = 0x%08x", w->window_id);
    kprintf(KR_OSTREAM, "      : children=0x%08x  .next=0x%08x .prev=0x%08x",
	    ADDRESS(&(w->children)), ADDRESS(w->children.next),
	    ADDRESS(w->children.prev));
    kprintf(KR_OSTREAM, "      : siblings=0x%08x  .next=0x%08x .prev=0x%08x",
	    ADDRESS(&(w->sibchain)), ADDRESS(w->sibchain.next),
	    ADDRESS(w->sibchain.prev));
    l = l->next;
  }
}

static bool
SessionCreatorRequest(Message *m)
{
  DEBUG(session_creator_cmds) kprintf(KR_OSTREAM, "winsys:: received session "
			      "creator request");

  switch(m->rcv_code) {
  case OC_SessionCreator_NewSession:
    {
      uint32_t result = 
	/* until proven otherwise */
	RC_SessionCreator_NoSessionAvailable; 

      /* Container window is embedded in SessionCreator forwarder key */
      Window *container = (Window *)m->rcv_w3;

      Session *new_session = session_create(container);

      if (m->rcv_keyInfo == TRUSTED_SESSION_CREATOR_INTERFACE) {

	/*Note: The client must provide the space bank key!!! */
	result = forwarder_create(KR_CLIENT_BANK, KR_NEW_SESSION, KR_NEW_NODE, 
		              KR_TRUSTED_SESSION_TYPE,
                              capros_Forwarder_sendCap | capros_Forwarder_sendWord,
			      ADDRESS(new_session) );

	if (result != RC_OK) {
	  m->snd_code = result;
	  return true;
	}

	m->snd_key0 = KR_NEW_SESSION;
	new_session->trusted = true;
      }
      else {

	/*Note: The client must provide the space bank key!!! */
	result = forwarder_create(KR_CLIENT_BANK, KR_NEW_SESSION, KR_NEW_NODE, 
			      KR_SESSION_TYPE,
                              capros_Forwarder_sendCap | capros_Forwarder_sendWord,
			      ADDRESS(new_session) );

	if (result != RC_OK) {
	  m->snd_code = result;
	  return true;
	}

	m->snd_key0 = KR_NEW_SESSION;
	new_session->trusted = false;
      }

      /* Stash the container window for the new session */
      new_session->container = container;

      m->snd_code = RC_OK;

      /* Before we return, stash the client's space bank key in an
         unused slot of the forwarder.  When the session is closed,
         we'll make an attempt to return storage to the space bank on
         behalf of the client. */
      capros_Forwarder_swapSlot(KR_NEW_NODE, STASH_CLIENT_BANK, KR_CLIENT_BANK,
                              KR_VOID);
    }
    break;

  default:
    {
      m->snd_code = RC_capros_key_UnknownRequest;
    }
    break;
  }
  return true;
}

/* Refresh the display, usually after changing the resolution */
static void
winsys_refresh()
{
  rect_t r = { {0,0}, Root->size };

  window_draw(Root, r);
}

static bool
probe_for_video(cap_t kr_probe, uint32_t vendor, 
		/* out */ struct pci_dev_data *probe_info)
{
  if (probe_info == NULL)
    return false;

  if (pciprobe_vendor_next(kr_probe, vendor, 0, probe_info) != RC_OK)
    return false;

  return true;
}

/* FIX: This is a *bad* hack for the demo only! */
static uint32_t
key_encode(uint8_t c, bool special)
{
  uint32_t t = c;

  if (special)
    t += 255;

  return t;
}

static bool
DomainRequest(Message *m)
{
  switch (m->rcv_code) {
  case OC_eros_domain_winsys_master_initialize:
    {
      uint32_t result;
      uint32_t next_slot = 0;
      struct pci_dev_data probe_result;

      DEBUG(domain_cmds) kprintf(KR_OSTREAM, 
			     "winsys:: OC_WindowSystem_Initialize\n");

      pciprobe_initialize(KR_ARG(0));
/*#define USE_OLD_DRIVER_SELECTION_CODE */
#ifdef USE_OLD_DRIVER_SELECTION_CODE

/* Supported video (FIX: probably should go in a header file?) */
#define VMWARE 0x15AD
#define BOCHS  0xFFFF

      current_graphics_driver = graphics_driver_table;

      /* Should have received the cap to the pci probing domain in
	 this request.  Use it to find the first supported video device */
      if (probe_for_video(KR_ARG(0), VMWARE, &probe_result)) {
	uint32_t init_info[2];

	/* FIX: Eric, we probably want to make video_initialize() just
	   take a pci_dev_data structure directly, yes?  */
	init_info[0] = probe_result.device;
	init_info[1] = probe_result.base_address[0];

	/* Do a VMWare-specific initialization: VMWare needs two init
	   parameters: the pci base address reg and the device id */
	result = video_initialize(2, init_info, &next_slot);
	if (result != RC_OK) {
	  kprintf(KR_OSTREAM, "**ERROR: winsys failed video_initialize() with "
		  "return code 0x%08x\n", result);
	  return false;
	}
      }
      else if (probe_for_video(KR_ARG(0), BOCHS, &probe_result)) {
	kprintf(KR_OSTREAM, "** ERROR: No bochs support yet!\n");
	return -1;		/* FIX: commit proper sepuku */
      }
      else {
	kprintf(KR_OSTREAM, "** ERROR: winsys couldn't find any video!\n");
	return -1;		/* FIX: commit proper sepuku */
      } 

#else      

      /* Should have received the cap to the pci probing domain in
	 this request.  Use it to find the first supported video device */
      for (current_graphics_driver = graphics_driver_table;
	   current_graphics_driver->video_initialize;
	   current_graphics_driver++) {
	if (probe_for_video(KR_ARG(0), current_graphics_driver->vendor_id, 
			    &probe_result)) {
	  uint32_t init_info[2];

	  DEBUG(domain_cmds)
	    kprintf(KR_OSTREAM, 
		    "winsys:: probing found a %s, initializing\n",
		    current_graphics_driver->name_string);


	  /* FIX: Eric, we probably want to make video_initialize() just
	     take a pci_dev_data structure directly, yes?
	     yes, but that can wait for another day.
	  */
	  init_info[0] = probe_result.device;
	  init_info[1] = probe_result.base_address[0];
	    
	  /* Do driver-specific initialization */
	  result = video_initialize(2, init_info, &next_slot);
	  if (result != RC_OK) {
	    kprintf(KR_OSTREAM, "**ERROR: winsys failed video_initialize() with "
		    "return code 0x%08x\n", result);
	    return false;
	  }
	  break;
	}
      }
      if (! current_graphics_driver->video_initialize) {
	  kprintf(KR_OSTREAM, "** ERROR: winsys couldn't find any video!\n");
	  return -1;		/* FIX: commit proper sepuku */
      }
#endif /* USE_OLD_DRIVER_SELECTION_CODE */

      /* Set up client shared subspace */
      kprintf(KR_OSTREAM, "Window System calling map_lss_three_layer "
	      "with next slot=%u...\n", next_slot);

      if (winsys_map_lss_three_layer(KR_SELF, KR_BANK, KR_SCRATCH, 
				     KR_NEW_NODE, next_slot) != RC_OK)
	kdprintf(KR_OSTREAM, "**ERROR: winsys call to map lss three "
		 "failed!\n");

      /* assign virtual address for client window content subspace;
	 this value depends on how many slots were used to map the
	 framebuffer and fifo */
      client_space = (uint32_t *)(next_slot * CLIENT_OFFSET);

      kprintf(KR_OSTREAM, "Window System calling video_define_cursor()...\n");

      /* If the driver supports it, use an alpha hardware cursor */
      if (video_functionality() & VIDEO_ALPHA_CURSOR ) {
	point_t hotspot = { f117_alpha_cursor_hot_x, f117_alpha_cursor_hot_y };
	point_t size = { f117_alpha_cursor_width, f117_alpha_cursor_height };
	point_t start = {10, 10};

	result = video_define_alpha_cursor(DefaultCursorID, hotspot, size, 
					   alpha_cursor_bits);

	if (result != RC_OK) {
	  kprintf(KR_OSTREAM, "**ERROR: winsys failed call to "
		  "video_define_alpha_cursor(): 0x%08x\n", result);
	  return false;
	}

	current_cursor_id = DefaultCursorID;
	video_show_cursor_at(current_cursor_id, start);
      }
      /* Define a default cursor and display it */
      else if (video_functionality() & VIDEO_HW_CURSOR) {
	point_t hotspot = { f117_cursor_hot_x, f117_cursor_hot_y };
	point_t size = { f117_cursor_width, f117_cursor_height };
	point_t start = {10, 10};

	result = video_define_cursor(DefaultCursorID, hotspot, size, 
				     f117_cursor_depth, f117_cursor_bits,
				     f117_cursor_mask_bits);
	if (result != RC_OK) {
	  kprintf(KR_OSTREAM, "**ERROR: winsys failed call to "
		  "video_define_cursor(): 0x%08x\n", result);
	  return false;
	}

	current_cursor_id = DefaultCursorID;
	video_show_cursor_at(current_cursor_id, start);
      }
      else {
	/* FIX: Do we need to support a software-only cursor? */
	kprintf(KR_OSTREAM, "**ERROR: winsys can't define a cursor!\n");
	return -1;
      }

      /* Fabricate a "root" window that will be the mother of all
	 windows */
      video_get_resolution(&root_width, &root_height, &root_depth);
      Root = root_create(root_width, root_height);
      if (Root == NULL) {
	kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate root window.");
	m->snd_code = RC_capros_key_RequestError;
	return true;
      }

      /* Root window starts out with focus */
      focus = Root;

      /* Draw the default root window */
      Root->draw(Root, Root->userClipRegion);

      /* Finally, now that Root exists, wrap the primordial
      SessionCreator keys so that they have Root as their container
      window */
      if (forwarder_create(KR_BANK, KR_SCRATCH, KR_NEW_NODE,
			  KR_SESSION_CREATOR,
			  capros_Forwarder_sendWord,
			  ADDRESS(Root) ) != RC_OK) {
	kprintf(KR_OSTREAM, "** ERROR: couldn't wrap SessionCreator key.\n");
	m->snd_code = RC_capros_key_RequestError;
	return true;
      }
      COPY_KEYREG(KR_SCRATCH, KR_SESSION_CREATOR);

      if (forwarder_create(KR_BANK, KR_SCRATCH, KR_NEW_NODE,
			  KR_TRUSTED_SESSION_CREATOR,
			  capros_Forwarder_sendWord,
			  ADDRESS(Root) ) != RC_OK) {
	kprintf(KR_OSTREAM, "** ERROR: couldn't wrap trusted "
		"SessionCreator key.\n");
	m->snd_code = RC_capros_key_RequestError;
	return true;
      }
      COPY_KEYREG(KR_SCRATCH, KR_TRUSTED_SESSION_CREATOR);

      /* Also, initialize the paste buffer state */
      pastebuffer_initialize(&pastebuffer_state);

      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_winsys_master_get_resolution:
    {
      m->snd_w1 = root_width;
      m->snd_w2 = root_height;
      m->snd_w3 = root_depth;
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_winsys_master_set_resolution:
    {
      m->snd_code = video_set_resolution(m->rcv_w1, m->rcv_w2, m->rcv_w3);
      if (m->snd_code == RC_OK) {
	(void) video_get_resolution(&root_width, &root_height, &root_depth);
	Root->size.x = root_width;
	Root->size.y = root_height;
	winsys_refresh();
      }
    }
    break;

  case OC_eros_domain_winsys_master_get_session_creators:
    {
      m->snd_key0 = KR_TRUSTED_SESSION_CREATOR;
      m->snd_key1 = KR_SESSION_CREATOR;
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_winsys_master_keybd_event:
    {
      uint8_t c;
      uint32_t canonical;
      bool special = false;

      /* First, convert raw scancode to a key.  That way the window
	 system can intercept any "special" keypresses.  Then, send a
	 keyevent only if there is a window that currently has
	 focus. */
      if (convert_scancode(m->rcv_w1, &c, &special)) {

	/* FIX: Here's where the window system can check for special
	   key events that it wants to act on... */
	canonical = key_encode(c, special);

	/* Intercept copy/paste key codes here */
	if (!WINDOWS_EQUAL(Root, focus)) {
	  if (canonical == COPY_INDICATOR) {
	    Session *s = (Session *)(focus->session);

	    s->cut_seq = pastebuffer_do_cut(&pastebuffer_state);

	    kprintf(KR_OSTREAM, "Winsys intercepted CTRL-C and "
		    "returned [%d]\n", s->cut_seq);
	  }
	  else if (canonical == PASTE_INDICATOR) {
	    Session *s = (Session *)(focus->session);

	    s->paste_seq = pastebuffer_do_paste(&pastebuffer_state);

	    kprintf(KR_OSTREAM, "Winsys intercepted CTRL-V and "
		    "returned [%d].\n", s->paste_seq);
	  }
	}

	/* Deliver key event to window */
	if (focus)
	  focus->deliver_key_event(focus, canonical);
      }
      m->snd_code = RC_OK;
    }
    break;

  case OC_eros_domain_winsys_master_mouse_event:
    {
      Window *cursor_win = NULL;
      uint32_t copy_paste = IS_NORMAL;
      uint32_t buttons = m->rcv_w1;
      point_t deltas = {(int32_t)m->rcv_w2, (int32_t)m->rcv_w3};

      DEBUG(drag) kprintf(KR_OSTREAM, "winsys: dragging = %d", dragging);

      /* First, update cursor position in Root window */
      update_cursor(deltas.x, deltas.y);

      /* Determine the window "under" the mouse position */
      cursor_win = find_containing_window(&(Root->children),
					  &cursor, &copy_paste);

      if (!dragging) {

	/* Use the left mouse as an indicator for changing focus.*/
	if (buttons & MOUSE_LEFT) {

	  /* First intercept any selects in a designated COPY or PASTE
	     invisible window. */
	  if (!WINDOWS_EQUAL(Root, cursor_win)) {
	    if (copy_paste & IS_COPY) {
	      Session *s = (Session *)(cursor_win->session);

	      s->cut_seq = pastebuffer_do_cut(&pastebuffer_state);

	      kprintf(KR_OSTREAM, "Winsys intercepted COPY menu item and "
		      "returned [%d]\n", s->cut_seq);
	    }
	    else if (copy_paste & IS_PASTE) {
	      Session *s = (Session *)(cursor_win->session);

	      s->paste_seq = pastebuffer_do_paste(&pastebuffer_state);

	      kprintf(KR_OSTREAM, "Winsys intercepted PASTE menu item and "
		      "returned [%d].\n", s->paste_seq);
	    }
	  }

	  DEBUG(focus)
	    kprintf(KR_OSTREAM, "winsys: comparing focus [0x%08x] w/cursor_win"
		    " [0x%08x]\n", ADDRESS(focus), ADDRESS(cursor_win));

	  if (!WINDOWS_EQUAL(focus, cursor_win))
	    winsys_change_focus(focus, cursor_win);
	}
      }

	/* Determine whether user is dragging pointer */
      dragging = (buttons & MOUSE_LEFT) == 0x0 ? false : true;

      /* Send event to client */
      focus->deliver_mouse_event(focus, buttons, cursor, dragging);

      m->snd_code = RC_OK;
    }
    break;

  default:
    {
      m->snd_code = RC_capros_key_UnknownRequest;
    }
    break;
  }
  return true;
}

static bool
ProcessRequest(Message *msg)
{
  /* Dispatch the request on the appropriate interface */
  switch (msg->rcv_keyInfo) {
  case SESSION_CREATOR_INTERFACE:
  case TRUSTED_SESSION_CREATOR_INTERFACE:
    {
      return SessionCreatorRequest(msg);
    }
    break;

  case SESSION_INTERFACE:
  case TRUSTED_SESSION_INTERFACE:
    {
      return SessionRequest(msg);
    }
    break;

  case WINDOW_SYSTEM_INTERFACE:
    {
      return DomainRequest(msg);
    }
    break;

  default:
    {
      msg->snd_code = RC_capros_key_UnknownRequest;
    }
    break;
  }
  return true;
}

/* Map a newly created subspace into this domain's address space,
   making this subspace shared memory.  This is the memory where a
   client will store framebuffer data and the window system will copy
   to the framebuffer. */
bool
winsys_map_addrspace(uint32_t winid, uint32_t kr_slot)
{
  uint32_t winid_map = winid - 1;
  uint32_t tmp = 0;

  assert(winid > 0);

  if (process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH) != RC_OK)
    return false;

  /* FIX: for now, insert the new subspace into the lss 3 layer of
     nodes.  The winid starts at 1 (zero is reserved) so to map it
     easily subtract 1 from it.  Take result and integer divide by 16,
     which is the number of window subspaces per lss 3 node.  That
     result gives us the slot number in ProcAddrSpace.  Then mod the
     winid-1 by 16 to get the slot within the lss 3 node in which to
     insert the key to the new subspace. */
  tmp = winid_map / 16;
  if (node_copy(KR_SCRATCH, tmp+16, KR_SCRATCH) != RC_OK)
    return false;

  if (node_swap(KR_SCRATCH, winid_map % 16, kr_slot, KR_VOID) != RC_OK)
    return false;

  kprintf(KR_OSTREAM, "winsys_map_addrspace(): inserted subspace for id [%u]"
	  " into slot [%u]\n of lss three node [%u]\n",
	  winid, winid_map % 16, tmp+16);

  return true;
}

/* FIX: This is temporarily here just to test invocation of the
   winsys's process keeper */
bool
winsys_demap_addrspace(uint32_t winid)
{
  uint32_t winid_map = winid - 1;
  uint32_t tmp = 0;

  assert(winid > 0);

  if (process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH) != RC_OK)
    return false;

  tmp = winid_map / 16;
  if (node_copy(KR_SCRATCH, tmp+16, KR_SCRATCH) != RC_OK)
    return false;

  if (node_swap(KR_SCRATCH, winid_map % 16, KR_VOID, KR_VOID) != RC_OK)
    return false;

  kprintf(KR_OSTREAM, "winsys_demap_addrspace(): bashed subspace for id [%u]"
	  " into slot [%u]\n of lss three node [%u]\n",
	  winid, winid_map % 16, tmp+16);

  return true;
}

bool
winsys_dump_contents(uint32_t winid)
{
  if (fbfault_received()) {
    return false;
  }
  else {
    kprintf(KR_OSTREAM, "winsys_dump_contents: 0x%08x and 0x%08x\n",
	    client_space[0], client_space[1]);
  }
  return true;
}

int
main(void)
{
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_ZSC, KR_ZSC);
  node_extended_copy(KR_CONSTIT, KC_PKEEPER, KR_PKEEPER);
  node_extended_copy(KR_CONSTIT, KC_DEVPRIVS, KR_DEVPRIVS);
  node_extended_copy(KR_CONSTIT, KC_PHYSRANGE, KR_PHYSRANGE);
  node_extended_copy(KR_CONSTIT, KC_MEMMAP_C, KR_MEMMAP);

  kprintf(KR_OSTREAM, "Window System says 'hi'!\n");


  if (addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, 
				 KR_NEW_NODE) != RC_OK)
    kdprintf(KR_OSTREAM, "**ERROR: winsys call to prep addrspace failed!\n");
  
#if 1
  kprintf(KR_OSTREAM, "Calling fbfault_init()...\n");
  /* Initialization for this domain's process keeper */
  if (fbfault_init(KR_PKEEPER, KR_BANK, KR_SCHED) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't initialize the process keeper.\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }
#endif

  /* Move the DEVPRIVS key to the ProcIoSpace slot so this domain can
     do port-io calls */
  process_swap(KR_SELF, ProcIoSpace, KR_DEVPRIVS, KR_VOID);

  /* Fabricate a generic start key to self */
  if (process_make_start_key(KR_SELF, WINDOW_SYSTEM_INTERFACE, 
			     KR_START) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate a start key "
	    "to myself...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Fabricate a start key for the session creator interface.  This
     key will get wrapped and handed out and then used by clients to
     create unique sessions. */
  if (process_make_start_key(KR_SELF, SESSION_CREATOR_INTERFACE, 
			     KR_SESSION_CREATOR) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate a start key "
	    "for session creator interface...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Do the same thing, but for "trusted" sessions. */
  if (process_make_start_key(KR_SELF, TRUSTED_SESSION_CREATOR_INTERFACE, 
			     KR_TRUSTED_SESSION_CREATOR) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate a start key "
	    "for trusted session creator interface...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Fabricate a start key for the session interface.  This key will
     be wrapped in each unique session wrapper key that's handed out
     to clients. */
  if (process_make_start_key(KR_SELF, SESSION_INTERFACE, 
			     KR_SESSION_TYPE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate a start key "
	    "for session interface...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Do same for trusted session interface */
  if (process_make_start_key(KR_SELF, TRUSTED_SESSION_INTERFACE, 
			     KR_TRUSTED_SESSION_TYPE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate a start key "
	    "for trusted session interface...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Create a wrapper key for "parking" client keys that are waiting
     for user input */
  if (forwarder_create(KR_BANK, KR_PARK_WRAP, KR_PARK_NODE, KR_START,
		       0, 0 ) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't create a wrapper for parking "
	    "waiting-client keys...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* We want the "parking" key to be blocked. */
  if (capros_Forwarder_setBlocked(KR_PARK_NODE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't block a wrapper for parking "
	    "waiting-client keys...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Now enter processing loop */
  {
    Message m;

    memset(&m, 0, sizeof(Message));

    m.snd_invKey = KR_RETURN;

    /* Make sure we can return to the caller */
    m.rcv_rsmkey = KR_RETURN;

    /* Send back the generic start key */
    m.snd_key0 = KR_START;

    /* Store any received keys in temp key registers */
    m.rcv_key0 = KR_ARG(0);
    m.rcv_key1 = KR_ARG(1);
    m.rcv_key2 = KR_ARG(2);

    /* Use a RETURN invocation unless explicitly overridden */
    m.invType = IT_PReturn;

    do {
      m.rcv_data = receive_buffer;
      m.rcv_limit = sizeof(receive_buffer);
      INVOKECAP(&m);
      m.invType = IT_PReturn;
    } while (ProcessRequest(&m));
  }

  return 0;
}
