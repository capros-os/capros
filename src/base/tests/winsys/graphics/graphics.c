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

/* A simple test of the erosgl library (drawing graphics primitives) */
#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <eros/KeyConst.h>
#include <eros/cap-instr.h>

#include <stdlib.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Include the needed interfaces */
#include <domain/SpaceBankKey.h>
#include <domain/SessionCreatorKey.h>
#include <domain/SessionKey.h>
#include <idl/capros/Process.h>

#include <graphics/erosgl.h>

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
uint32_t main_width = 700;
uint32_t main_height = 550;
uint32_t main_window_id = 0;
uint32_t *main_content = NULL;

cap_t ostream = KR_OSTREAM;

#define TIME_TO_QUIT(evt) ((uint8_t)evt.data[0] == 'q')

static void
return_to_vger()
{
  Message msg;

  memset(&msg, 0, sizeof(msg));
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_invKey = KR_RETURN;

  SEND(&msg);
}

static void
return_to_void()
{
  Message msg;

  memset(&msg, 0, sizeof(msg));

  RETURN(&msg);
}

static void
doExit()
{
  /* Ask window system to kill out window */
  session_win_kill(KR_NEW_SESSION, main_window_id);

  return_to_void();
}

/* Ask window system to redraw our window contents */
static void
myUpdate(rect_t area)
{
  (void) session_win_redraw(KR_NEW_SESSION, main_window_id,
			    area.topLeft.x, area.topLeft.y,
			    area.bottomRight.x, area.bottomRight.y);
}

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

static void
doResize(Event evt)
{
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
  myUpdate(area);
}

static void
lines_demo(GLContext *gc)
{
  rect_t clip = { {10,10}, {400,400} };
  uint32_t pt;
  point_t center = {100, 100};

  erosgl_gc_set_raster_op(gc, ROP_COPY);
  erosgl_gc_set_dimensions(gc, main_width, main_height);
  erosgl_gc_set_clipping(gc, clip);
  erosgl_gc_set_line_width(gc, 1);

  erosgl_gc_set_color(gc, RED);
  for (pt = 0; pt < 200; pt+=10) {
    line_t ln;

    ln.pt[0] = center;
    ln.pt[1].x = pt;
    ln.pt[1].y = 0;

    erosgl_line(gc, ln);
    erosgl_update(gc);
  }

  erosgl_gc_set_color(gc, BLUE);
  for (pt = 0; pt < 200; pt+=10) {
    line_t ln;

    ln.pt[0] = center;
    ln.pt[1].x = 200;
    ln.pt[1].y = pt;

    erosgl_line(gc, ln);
    erosgl_update(gc);
  }

  erosgl_gc_set_color(gc, GREEN);
  for (pt = 200; pt > 0; pt-=10) {
    line_t ln;

    ln.pt[0] = center;
    ln.pt[1].x = pt;
    ln.pt[1].y = 200;

    erosgl_line(gc, ln);
    erosgl_update(gc);
  }

  erosgl_gc_set_color(gc, WHITE);
  for (pt = 200; pt > 0; pt-=10) {
    line_t ln;

    ln.pt[0] = center;
    ln.pt[1].x = 0;
    ln.pt[1].y = pt;

    erosgl_line(gc, ln);
    erosgl_update(gc);
  }
}

static void
rects_demo(GLContext *gc)
{
  rect_t clip = { {0,0}, {500, 500} };
  rect_t r1 = { {80,80}, {150,150} };
  rect_t r2 = { {100,175}, {200,190} };
  rect_t r3 = { {100,200}, {300,210} };
  rect_t r4 = { {250,250}, {290,290} };

  if (gc == NULL)
    return;

  erosgl_gc_set_clipping(gc, clip);

  erosgl_gc_set_color(gc, RED);
  erosgl_rectfillborder(gc, r1, GREEN);
  erosgl_update(gc);

  erosgl_gc_set_color(gc, BLUE);
  erosgl_rectfillborder(gc, r2, RED);
  erosgl_update(gc);

  erosgl_gc_set_color(gc, GREEN);
  erosgl_rectfillborder(gc, r3, BLUE);
  erosgl_update(gc);

  erosgl_gc_set_color(gc, BLUE);
  erosgl_rectfillborder(gc, r4, GREEN);
  erosgl_update(gc);
}

void
triangle_demo(GLContext *gc)
{
  uint32_t repeat;
  uint32_t x;

  for (repeat = 0; repeat < 2; repeat ++) {
    for( x=0; x < 400; x++) {
      point_t pt1 = {500-x, x};
      point_t pt2 = {525-x, 25+x};
      point_t pt3 = {500-x, 25+x};

      erosgl_gc_set_color(gc, RED);
      erosgl_trifill(gc, pt1, pt2, pt3);
      erosgl_update(gc);

      erosgl_gc_set_color(gc, BLACK);
      erosgl_trifill(gc, pt1, pt2, pt3);
      erosgl_update(gc);
    }

    for (x=500; x>0; x--) {
      point_t pt1 = {500-x, x};
      point_t pt2 = {550-x, 50+x};
      point_t pt3 = {475-x, 25+x};

      erosgl_gc_set_color(gc, RED);
      erosgl_trifill(gc, pt1, pt2, pt3);
      erosgl_update(gc);

      erosgl_gc_set_color(gc, BLACK);
      erosgl_trifill(gc, pt1, pt2, pt3);
      erosgl_update(gc);
    }
  }
}

 void
tower_of_triangles(GLContext *gc)
{
  uint32_t x;
  color_t color = RED;

  if (gc == NULL)
    return;

  for (x=0; x<500; x+=30) {

    point_t pt1 = {350, x};
    point_t pt2 = {350-x, 10+x};
    point_t pt3 = {370+x, 10+x};

    switch ((x/30) % 6){
    case 0: color = RED; break;
    case 1: color = RED | BLUE; break;
    case 2: color = BLUE; break;
    case 3: color = BLUE | GREEN; break;
    case 4: color = GREEN; break;
    case 5: color = GREEN | RED; break;
    }

    erosgl_gc_set_color(gc, color);
    erosgl_trifill(gc, pt1, pt2, pt3);
    erosgl_update(gc);
  }

}

static void
press_key_to_continue(GLContext *gc)
{
  for (;;) {
    Event evt;
    result_t result;

    result = session_next_event(KR_NEW_SESSION, &evt);
    if (result != RC_OK)
      continue;

    if (evt.window_id != main_window_id)
      continue;

    if (IS_RESIZE_EVENT(evt)) {
      doResize(evt);
      erosgl_gc_set_dimensions(gc, main_width, main_height);
      continue;
    }

    if (IS_KEY_EVENT(evt))
      if (TIME_TO_QUIT(evt))
	doExit();
      break;
  }
}

static void
graphics_demo(GLContext *gc)
{
  if (gc == NULL) {
    kprintf(KR_OSTREAM, "**ERROR: Graphics: couldn't create erosGC.\n");
    return;
  }

  erosgl_clear(gc, BLACK);
  erosgl_update(gc);

  lines_demo(gc);

  press_key_to_continue(gc);

  erosgl_clear(gc, BLACK);
  erosgl_update(gc);

  rects_demo(gc);

  press_key_to_continue(gc);

  erosgl_clear(gc, BLACK);
  erosgl_update(gc);

#if 0
  /* DON'T DEMO THIS ONE: IT MAKES THE CURSOR FLICKER TOO MUCH.. VANDY
     NEEDS TO DEBUG THE CURSOR CODE */
  triangle_demo(gc);

  press_key_to_continue(gc);

  erosgl_clear(gc, BLACK);
  erosgl_update(gc);
#endif

  tower_of_triangles(gc);
}

int
main(void)
{
  uint32_t result;
  uint32_t u;
  uint32_t dpy_width;
  uint32_t dpy_height;

  GLContext *gc;

  /* A key to a session creator is passed via this domain's
     constructor.  Grab it here and stash it for later use. */
  COPY_KEYREG(KR_ARG(0), KR_SESSION_CREATOR);

  /* Copy the debugging key from the constituent's node to the keyreg
     set. */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);

  return_to_vger();

  /* Prepare our default address space for mapping in additional
     memory. */
  addrspace_prep_for_mapping(KR_SELF, KR_BANK, KR_SCRATCH, KR_NEW_SUBSPACE);

  /* Create a subbank to use for window creation */
  if (spcbank_create_subbank(KR_BANK, KR_SUB_BANK) != RC_OK) {
    kprintf(KR_OSTREAM, "Graphics failed to create sub bank.\n");
    return -1;
  }

  /* Ask for an initial (untrusted) session */
  if (session_creator_new_session(KR_SESSION_CREATOR, KR_SUB_BANK, 
				  KR_NEW_SESSION) != RC_OK) {
    kprintf(KR_OSTREAM, "Graphics failed to create new session.\n");
    return -1;
  }

  /* Find out how big current display is */
  if (session_container_size(KR_NEW_SESSION, 
			     &dpy_width, &dpy_height) != RC_OK) {
    kprintf(KR_OSTREAM, 
	    "Graphics failed with call to session_container_size()\n");
    return -1;
  }

  kprintf(KR_OSTREAM, "Graphics: dpy_width = %u and dpy_height = %u.\n",
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
			  "ErosGL demo");

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

    /* Tell the window system that we want to display our new
       window */
    session_win_map(KR_NEW_SESSION, main_window_id);

    /* Create a graphics context and initialize it appropriately */
    gc = erosgl_new_context(main_content, myUpdate);

    graphics_demo(gc);

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
	  if (TIME_TO_QUIT(evt))
	    doExit();

	  /* else, start demo over */
	  graphics_demo(gc);
	}

	/* Resize events */
	else if (IS_RESIZE_EVENT(evt)) {
	  doResize(evt);
	  erosgl_gc_set_dimensions(gc, main_width, main_height);
	}
      }
    }

    erosgl_free_context(gc);

  return 0;
}

