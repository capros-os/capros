/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
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

/* A simple text echoing client.  Note we don't even interpret
   backspace or new line... just pure echoing of "printable" characters. */
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/cap-instr.h>

#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>

#include <stdlib.h>

#include <domain/SpaceBankKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <domain/SessionCreatorKey.h>
#include <domain/SessionKey.h>

#include <graphics/color.h>
#include <graphics/rect.h>
#include <graphics/fonts/Font.h>

#include <addrspace/addrspace.h>

#include "constituents.h"

/* The largest size any domain currently has for sharing or otherwise
mapping memory is 1GB.  This domain maps an array of 32-bit values
into that region and the offset into this region starts at zero. */
#define _1GB_ 0x40000000u
#define MAX_BUFFER_OFFSET ((_1GB_ / sizeof(uint32_t)) - 1)

#define KR_OSTREAM          KR_APP(0) /* For debugging output via kprintf*/

#define KR_SESSION_CREATOR  KR_APP(1)
#define KR_NEW_SESSION   KR_APP(2)
#define KR_SCRATCH       KR_APP(3)
#define KR_NEW_WINDOW    KR_APP(4)
#define KR_NEW_SUBSPACE  KR_APP(5)

#define KR_SUB_BANK      KR_APP(6)

/* globals */
uint32_t main_width = 500;
uint32_t main_height = 350;
uint32_t main_window_id = 0;
uint32_t *main_content = NULL;
point_t cursor = {5, 5};
FontType *myFont = NULL;

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

/* Echo a single character to the buffer that represents the main
window's contents.  The global variable "cursor" maintains the current
(x,y) offset into the buffer. */
static void
echo(uint8_t c)
{
  uint8_t str[2] = {c, '\0'};
  rect_t area = { {cursor.x, cursor.y}, {cursor.x + myFont->width, 
					 cursor.y+myFont->height} };

#ifdef TRACE
  kprintf(KR_OSTREAM, "Echoer: echoing [%s] at (%d,%d) w/area [(%d,%d)(%d,%d)]\n", 
	  str, cursor.x, cursor.y, area.topLeft.x, area.topLeft.y,
	  area.bottomRight.x, area.bottomRight.y);
#endif

  font_render(myFont,(uint8_t *)main_content, main_width,
	      cursor, WHITE, BLACK, FONT_NORMAL, str);

  update(area);

  cursor.x += myFont->width;
  if (cursor.x > main_width-myFont->width) {
    cursor.x = 5;
    cursor.y += myFont->height;
  }

  if (cursor.y >= main_height-myFont->height) {
    rect_t area = { {0, 0}, {main_width, main_height} };
    clear();
    update(area);
    cursor.x = 5;
    cursor.y = 5;
  }
}

int
main(void)
{
  uint32_t result;
  uint32_t u;
  uint32_t dpy_width;
  uint32_t dpy_height;

  /* A key to a session creator is passed via this domain's
     constructor.  Grab it here and stash it for later use. */
  COPY_KEYREG(KR_ARG(0), KR_SESSION_CREATOR);

  /* Copy the debugging key from the constituent's node to the keyreg
     set. */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  /* Get the default font */
  myFont = font_find_font("Sun12x22");
  if (myFont == NULL) {
    kprintf(KR_OSTREAM, "**ERROR: echoer couldn't get default font!\n");
    return -1;
  }

  /* Prepare our default address space for mapping in additional
     memory. */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, KR_NEW_SUBSPACE);

  /* Create a subbank to use for window creation */
  if (spcbank_create_subbank(KR_BANK, KR_SUB_BANK) != RC_OK) {
    kprintf(KR_OSTREAM, "Echoer failed to create sub bank.\n");
    return -1;
  }

  /* Ask for an initial (untrusted) session */
  if (session_creator_new_session(KR_SESSION_CREATOR, KR_SUB_BANK, 
				  KR_NEW_SESSION) != RC_OK) {
    kprintf(KR_OSTREAM, "Echoer failed to create new session.\n");
    return -1;
  }

  /* Find out how big current display is */
  if (session_container_size(KR_NEW_SESSION, &dpy_width, &dpy_height) 
      != RC_OK) {
    kprintf(KR_OSTREAM, "Echoer failed with call to session_display_size()\n");
    return -1;
  }

  kprintf(KR_OSTREAM, "Echoer: dpy_width = %u and dpy_height = %u.\n",
	  dpy_width, dpy_height);

  /* Create a window via the session key */
    result = session_new_window(KR_NEW_SESSION,
				KR_BANK,
				DEFAULT_PARENT,
				dpy_width / 5,
				dpy_height / 5,
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
			  "/usr/bin/empty/shell");

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

    /* Set the entire window contents to one color */ 
    clear();

    /* Tell the window system that we want to display our new
       window */
    session_win_map(KR_NEW_SESSION, main_window_id);

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

	/* Since this domain only has one window, if the received
	   event somehow doesn't pertain to this one window, ignore it. */
	if (evt.window_id != main_window_id)
	  continue;

	/* Mouse events */

	if (IS_MOUSE_EVENT(evt)) {
	  /* Ignore */
	}

	/* Keyboard events */

	else if (IS_KEY_EVENT(evt)) {

#ifdef TRACE 
	  kprintf(KR_OSTREAM, "Key event: %d\n", (int32_t)evt.data[0]);
#endif
	  echo((uint8_t)evt.data[0]);

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
  return 0;
}

