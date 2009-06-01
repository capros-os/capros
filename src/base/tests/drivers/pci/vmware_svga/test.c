/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/NodeKey.h>
#include <idl/capros/Process.h>
#include <idl/capros/Sleep.h>

#include <stdlib.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>

/* Need the Drawable interface */
#include <domain/DrawableKey.h>

/* Need the VideoDriverKey interface */
#include <domain/drivers/VideoDriverKey.h>

/* Need the PS/2 interface */
#include <idl/capros/Ps2.h>

/* Don't forget the PCI probe interface */
#include <domain/drivers/PciProbeKey.h>

#include "constituents.h"
#include "pixmap.h"
#include "f117_cursor.h"
#include "kangaroo.h"
#include "paint_instructions.h"

#define OBSERVER_SLEEP 2000
/*  This is a test program which demonstrates the fundamentals of
    using the VMWare SVGA driver.  This was designed and has only been
    tested with VMWare Workstation 3.0.0 (build 1455) running on
    RedHat 7.3 on a PIII box.  When running this demo, be careful
    about switching the VMWare window to full-screen.  If you want to
    do that, make sure the graphics have already been initialized
    before you toggle to full-screen, otherwise VMWare may hang.  

    If you want to view the serial output from this program
    (everything that's printed via "kprintf"), redirect CON1 of your
    VMWare to either a file or a TTY.  If to a TTY, you can use "minicom"
    in a shell window to view the output.

    This domain acts as two virtual domains.  The first is a "device
    manager" domain, which must identify (via bus probing, for
    example) the available video device(s) and then initialize the
    device appropriately.  The second virtual domain is a "graphics
    client" domain, which issues drawing commands on a Drawable
    object.
 */

#define LINEDRAW            0x002u
#define PIXMAP              0x004u
#define RECTFILL            0x008u

#define DESIGN              0x020u
#define CURSOR              0x040u
#define RESOLUTION          0x080u
#define KANGAROO            0x100u
#define PAINT               0x200u

#define LEFT   0x1u
#define RIGHT  0x2u
#define MIDDLE 0x4u

/* Define which tests you want to see.  Don't try the RESOLUTION test
   in full-screen mode! */
// #define test_matrix (PIXMAP | RECTFILL | CURSOR | DESIGN | KANGAROO | PAINT)
// #define test_matrix (PIXMAP | RECTFILL | CURSOR)
#define test_matrix    (LINEDRAW | PIXMAP | RECTFILL | CURSOR | KANGAROO | PAINT)
//#define test_matrix ( PAINT )


#define KR_OSTREAM       KR_APP(0) /* For debugging output via kprintf*/
#define KR_SVGA_DRIVER_C KR_APP(1) /* The constructor for the video driver */
#define KR_SVGA_DRIVER   KR_APP(2) /* The start key for the video driver */
#define KR_DRAWABLE      KR_APP(3) /* The start key for the Drawable */
#define KR_SLEEP         KR_APP(4) /* A capability for sleeping */
#define KR_PS2READER     KR_APP(5) /* The constructor for the PS/2 driver */
#define KR_PS2MOUSE_HELPER   KR_APP(6) /* The start key for the PS/2
					  mouse helper domain */
#define KR_PS2KEY_HELPER     KR_APP(7) /* The start key for the PS/2
					  key helper domain */
#define KR_START         KR_APP(8) /* Start key to this domain */
#define KR_PCI_PROBE_C   KR_APP(9) /* Constructor for pci probe domain */
#define KR_PCI_PROBE     KR_APP(10) /* Start key for pci probe */

/* Some sample colors (in aRGB format) */
#define BLACK 0x0
#define RED   0xFF0000
#define GREEN 0x00FF00
#define BLUE  0x0000FF
#define WHITE 0xFFFFFF

#define VMWareVENDORID 0x15AD

/* globals */
uint32_t screen_width  = 0;
uint32_t screen_height = 0;
uint32_t screen_depth  = 0;
uint32_t cursor_x      = 0;
uint32_t cursor_y      = 0;
bool cursor_defined    = false;
uint32_t cursor;

void
send_key(cap_t target, cap_t key, uint32_t opcode)
{
  Message m;

  memset(&m, 0, sizeof(m));
  m.snd_invKey = target;
  m.snd_code = opcode;
  m.rcv_rsmkey = KR_VOID;
  m.snd_key0 = key;
  CALL(&m);
}

static bool
show_card_functionality(fixreg_t key)
{
  uint32_t result;
  uint32_t flags;

  result = video_functionality(key, &flags);

  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: show_card_functionality: result = %u\n",
	    result);
    return false;
  } 
  else {
    kprintf(KR_OSTREAM, "Card functionality = 0x%08x:\n", flags);

    if (flags & VIDEO_RECT_FILL)
      kprintf(KR_OSTREAM,"  Rectangle Fill\n");

    if (flags & VIDEO_RECT_COPY)
      kprintf(KR_OSTREAM,"  Rectangle Copy\n");

    if (flags & VIDEO_RECT_PAT_FILL)
      kprintf(KR_OSTREAM,"  Rectangle Pattern Fill\n");

    if (flags & VIDEO_OFFSCREEN)
      kprintf(KR_OSTREAM,"  Offscreen\n");

    if (flags & VIDEO_RASTER_OP)
      kprintf(KR_OSTREAM,"  Raster Ops\n");

    if (flags & VIDEO_HW_CURSOR)
      kprintf(KR_OSTREAM,"  HW Cursor\n");

    if (flags & VIDEO_ALPHA_CURSOR)
      kprintf(KR_OSTREAM,"  Alpha Cursor\n");
  }
  return true;
}

static bool
get_resolution(fixreg_t video, uint32_t *w, uint32_t *h, uint32_t *d)
{
  uint32_t result;

  result = video_get_resolution(video, w, h, d);

  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "*** ERROR: get_resolution() result=%u\n", result);
    return false;
  }

  kprintf(KR_OSTREAM, "Video driver says resolution is  %ux%ux%u...\n", 
	  *w, *h, *d);

  return true;
}

static bool
pixel_designs(fixreg_t drawable)
{
  uint32_t p,q;
  uint32_t color = 0x0;

  kprintf(KR_OSTREAM, "test:: drawing...");

  for (p = 1; p < screen_width/4; p=p+3)
    for (q = 1; q < screen_height; q=q+3) {
      if (drawable_set_pixel(drawable, p, q, color) != RC_OK)
	return false;

      color += (p*q);

      /* Update the screen.  Note that I'm just updating the latest
	 pixel.  It's amazing how much slower this is if you update
	 the entire screen each time you draw a pixel! */
      if (drawable_redraw(drawable, p, q, p+1, q+1) != RC_OK)
	return false;
    }

  return true;
}

/* Update the (x, y) of the cursor based on the deltas and then
   display the specified cursor at the new location.*/
static bool
update_cursor_position(fixreg_t driver, uint32_t cursor_id, 
		       int8_t x_delta, int8_t y_delta)
{
  int32_t check_x = cursor_x + x_delta;
  int32_t check_y = cursor_y - y_delta;

  if (check_x < 1) 
    cursor_x = 1;
  else { 
    if (check_x < screen_width)
      cursor_x = check_x;
    else
      cursor_x = screen_width-1;
  }

  if (check_y < 1)
    cursor_y = 1;
  else {
    if (check_y < screen_height)
      cursor_y = check_y;
    else
      cursor_y = screen_height-1;
  }

  return video_show_cursor_at(driver, cursor_id, cursor_x, cursor_y);
}

static bool
paint_pixel(fixreg_t drawable, int16_t x_in, int16_t y_in)
{
  uint32_t x, y;

  /* Draw a 1-pixel-radius circle around the selected pixel */
  for (x = x_in-1; x < x_in+1; x++) {
    if (drawable_set_pixel(drawable, x, y_in, WHITE) != RC_OK)
      return false;
  }

  for (y = y_in-1; y < y_in+1; y++) {
    if (drawable_set_pixel(drawable, x_in, y, WHITE) != RC_OK)
      return false;
  }

  /* update screen */
  return drawable_redraw(drawable, x_in-2, y_in-2, x_in+2, y_in+2);
}

static bool
define_cursor(fixreg_t driver)
{
  uint32_t result;

  result = video_define_cursor(driver, f117_cursor_id,
			       f117_cursor_hot_x,
			       f117_cursor_hot_y,
			       f117_cursor_width,
			       f117_cursor_height,
			       f117_cursor_depth,
			       f117_cursor_bits,
			       f117_cursor_mask_bits);

  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "*** ERROR: define_cursor() result=%u\n", result);
    return false;
  }
  cursor = f117_cursor_id;
  cursor_defined = true;
  return true;
}

static bool
cursor_test(fixreg_t drawable, fixreg_t driver)
{
  uint32_t result = RC_capros_key_UnknownRequest; /* until proven otherwise */

  if (!define_cursor(driver))
    return false;

  /* Give it an initial position */
  result = video_show_cursor_at(driver, cursor, 10, 10);

  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "***ERROR cursor_test() show_cursor_at() result=%u",
	    result);
    return false;
  }
  /* Now draw a red box in the framebuffer to ensure that cursor moves
     "over" it correctly. */
  {
    uint32_t x, y;

    /* Draw manually, just to test driver */
    for (x = 40; x < 80; x++) {
      for (y = 40; y < 80; y++) {
	result = drawable_set_pixel(drawable, x, y, RED);
	if (result != RC_OK) {
	  kprintf(KR_OSTREAM, "*** ERROR: drawable_set_pixel() result=%u",
		  result);
	  return false;
	}
      }
    }

    /* update screen */
    result = drawable_redraw(drawable, 40, 40, 80, 80);
    if (result != RC_OK) {
      kprintf(KR_OSTREAM, "*** ERROR: drawable_redraw() result=%u", result);
      return false;
    }

    /* now move cursor (NOTE: this part of the test does not use a
       mouse driver:  it manually moves the cursor.) */
    for (x = 10; x < 200; x++) {
      if (video_show_cursor_at(driver, cursor, x, x) != RC_OK)
	return false;

      capros_Sleep_sleep(KR_SLEEP, 50);
    }
  }
  return true;
}


static void line_test(fixreg_t drawable)
{
  uint32_t x;
  uint32_t repeat;
  uint32_t pt;

  // Multi color Burst and Wide Lines

  drawable_rectfill(drawable, 300, 300, 700, 700, BLACK, ROP_COPY);
  drawable_set_clip_region( drawable, 100, 100, 300, 700);

#if 1
  for (pt = 0; pt < 400; pt+=25)
    drawable_linedraw(drawable, 200, 200, pt, 0, 1, RED, ROP_COPY);
  for (pt = 0; pt < 400; pt+=25)
    drawable_linedraw(drawable, 200, 200, 400, pt, 1, BLUE, ROP_COPY);
  for (pt = 400; pt > 0; pt-=25)
    drawable_linedraw(drawable, 200, 200, pt, 400, 1, GREEN, ROP_COPY);
  for (pt = 400; pt > 0; pt-=25)
    drawable_linedraw(drawable, 200, 200, 0, pt, 1, WHITE, ROP_COPY);
#else
  for (pt = 0; pt < 400; pt+=25)
    drawable_linedraw(drawable, 200, 200, pt, 0, 4, RED, ROP_COPY);
  for (pt = 0; pt < 400; pt+=25)
    drawable_linedraw(drawable, 200, 200, 400, pt, 4, BLUE, ROP_COPY);
  for (pt = 400; pt > 0; pt-=25)
    drawable_linedraw(drawable, 200, 200, pt, 400, 4, GREEN, ROP_COPY);
  for (pt = 400; pt > 0; pt-=25)
    drawable_linedraw(drawable, 200, 200, 0, pt, 4, WHITE, ROP_COPY);
#endif

  drawable_linedraw(drawable, 450, 50, 450, 150, 3, RED, ROP_COPY);
  drawable_linedraw(drawable, 450, 50, 500, 50, 3, GREEN, ROP_COPY);

  for (pt = 0; pt < 20; pt += 1){
    drawable_linedraw(drawable, pt*20 + 50, 500, pt*20+30, 550, 17, RED, ROP_COPY);
    drawable_linedraw(drawable, pt*20, 600, pt*20 + 20, 650, 17, GREEN, ROP_COPY);
  }

  capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP * 4);

  // Rectangles with Borders

  drawable_set_clip_region( drawable, 0, 0, 700, 700);  
  drawable_rectfill(drawable, 0, 0, 700, 700, BLACK, ROP_COPY);

  drawable_rectfillborder(drawable, 100, 100, 150, 150, RED,   GREEN, ROP_COPY);
  drawable_rectfillborder(drawable, 100, 175, 200, 190, BLUE,  RED,   ROP_COPY);
  drawable_rectfillborder(drawable, 100, 200, 300, 210, GREEN, BLUE,  ROP_COPY);
  drawable_rectfillborder(drawable, 250, 250, 290, 290, BLUE,  GREEN, ROP_COPY);

  capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP);

  // Moving triangles

  drawable_rectfill(drawable, 0, 0, 700, 700, BLACK, ROP_COPY);
  drawable_set_clip_region( drawable, 150, 0, 350, 700);  

  for (repeat = 0; repeat < 2; repeat ++) {
    for( x=0; x < 400; x++) {
      drawable_trifill(drawable, 500-x, x, 525-x, 25+x, 500-x, 25+x, RED, ROP_COPY);
      drawable_tridraw(drawable, x, x, 25+x, 25+x, x, 25+x, 1, 1, 1, RED, ROP_COPY);
      capros_Sleep_sleep(KR_SLEEP, 20);
      drawable_tridraw(drawable, x, x, 25+x, 25+x, x, 25+x, 1, 1, 1, BLACK, ROP_COPY);
      drawable_trifill(drawable, 500-x, x, 525-x, 25+x, 500-x, 25+x, BLACK, ROP_COPY);

    }
#if 0    
    for (x=500;x>0;x--) {
      drawable_trifill(drawable, 500-x, x, 525-x, 25+x, 500-x, 25+x, RED, ROP_COPY);
      drawable_tridraw(drawable, x, x, 25+x, 25+x, x, 25+x, 1, 1, 1, RED, ROP_COPY);
      capros_Sleep_sleep(KR_SLEEP, 20);
      drawable_tridraw(drawable, x, x, 25+x, 25+x, x, 25+x, 1, 1, 1, BLACK, ROP_COPY);
      drawable_trifill(drawable, 500-x, x, 525-x, 25+x, 500-x, 25+x, BLACK, ROP_COPY);
    }
#else
    for (x=500;x>0;x--) {
      drawable_trifill(drawable, 500-x, x, 550-x, 50+x, 475-x, 25+x, RED, ROP_COPY);

      drawable_tridraw(drawable, x, x, 25+x, 25+x, x, 25+x, 1, 1, 1, RED, ROP_COPY);
      capros_Sleep_sleep(KR_SLEEP, 20);
      drawable_tridraw(drawable, x, x, 25+x, 25+x, x, 25+x, 1, 1, 1, BLACK, ROP_COPY);

      drawable_trifill(drawable, 500-x, x, 550-x, 50+x, 475-x, 25+x, BLACK, ROP_COPY);
    }
#endif
  }

  // Tower of Triangles

  drawable_rectfill(drawable, 0, 0, 700, 700, BLACK, ROP_COPY);
  drawable_set_clip_region( drawable, 0, 0, 700, 700);

  for (x=0;x<500;x+=30) {
    uint32_t color = RED;
    switch ((x/30) % 6){
    case 0: color = RED; break;
    case 1: color = RED | BLUE; break;
    case 2: color = BLUE; break;
    case 3: color = BLUE | GREEN; break;
    case 4: color = GREEN; break;
    case 5: color = GREEN | RED; break;
    }
    drawable_trifill(drawable, 350, x, 350-x, 10+x, 370+x, 10+x, color, ROP_COPY);
  }

  capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP);

}

static void paint(fixreg_t drawable, fixreg_t video)
{
  uint32_t result = RC_capros_key_UnknownRequest; /* until proven otherwise */

  if (!cursor_defined) {
    if (!define_cursor(video))
      return;
  }

  drawable_clear(drawable, BLACK);

  /* Give cursor an initial position */
  video_show_cursor_at(video, cursor, 10, 10);

  /* We don't have fancy fonts yet, so use a pixmap of text data ... */
  result = video_define_pixmap(video, 5, paint_width, paint_height, 
			       paint_depth, paint_data);
  if (result == RC_OK) {
    result = video_render_pixmap(video, 5, 0, 0, paint_width, paint_height);
    if (result == RC_OK) {
      Message msg;

      /* Send the helpers the start key to this domain */
      send_key(KR_PS2MOUSE_HELPER, KR_START, 0);
      send_key(KR_PS2KEY_HELPER, KR_START, 0);

      msg.snd_invKey = KR_RETURN;
      msg.snd_key0   = KR_VOID;
      msg.snd_key1   = KR_VOID;
      msg.snd_key2   = KR_VOID;
      msg.snd_rsmkey = KR_RETURN;
      msg.snd_data = 0;
      msg.snd_len  = 0;
      msg.snd_code = 0;
      msg.snd_w1 = 0;
      msg.snd_w2 = 0;
      msg.snd_w3 = 0;

      msg.rcv_key0   = KR_VOID;
      msg.rcv_key1   = KR_VOID;
      msg.rcv_key2   = KR_VOID;
      msg.rcv_rsmkey = KR_RETURN;
      msg.rcv_data = 0;
      msg.rcv_limit  = 0;
      msg.rcv_w1 = 0;
      msg.rcv_w2 = 0;
      msg.rcv_w3 = 0;
  
      /* Now allow user to move mouse cursor by staying in a loop,
	 processing mouse events. */
      for (;;) {
	RETURN(&msg);

	/* NOTE: This is a hack-job:  we're basically blindly
	   accepting any invocation as valid! */
#if 0
	kprintf(KR_OSTREAM, "*** MOUSEEVENT:  %d  %d  %d\n", 
		(int8_t)msg.rcv_w1, (int8_t)msg.rcv_w2, 
		(int8_t)msg.rcv_w3);
#endif

	update_cursor_position(video, cursor, 
			       (int8_t)msg.rcv_w2, 
			       (int8_t)msg.rcv_w3);

	/* Make left mouse paint pixels.  (Just like a Paint program!) */
	if (msg.rcv_w1 & LEFT)
	  paint_pixel(drawable, cursor_x, cursor_y);

	/* A right mouse click will reset the background,
	   essentially erasing all the painting you've done. */
	if (msg.rcv_w1 & RIGHT) {
	  if (drawable_clear(drawable, BLACK) == RC_OK)
	    video_render_pixmap(video, 5, 0, 0, paint_width, paint_height);
	}
      }
    }
  }
}

static bool
get_max_resolution(fixreg_t video, uint32_t *w, uint32_t *h, uint32_t *d)
{
  uint32_t result = video_max_resolution(video, w, h, d);

  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "*** ERROR: get_max_resolution() result=%u\n", result);
    return false;
  }

  kprintf(KR_OSTREAM, "Video driver says max resolution is  %ux%ux%u...\n", 
	  *w, *h, *d);

  return true;
}

/* Again, don't do this test in full-screen VMWare mode on RedHat.  It
   will probably lock up your display. */
static bool
set_resolution(fixreg_t video, uint32_t width, uint32_t height, uint32_t depth)
{
  uint32_t result;

  result = video_set_resolution(video, width, height, depth); 
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "*** ERROR: set_resolution() result=%u\n", result);
    return false;
  }

  kprintf(KR_OSTREAM, "Setting resolution to  %ux%ux%u...\n", width, height,
	  depth);

  return get_resolution(video, &screen_width, &screen_height, 
			&screen_depth);
}

/* Temporary: Call this routine if you want to see how long it takes
to bitblt 750x500 pixels to the framebuffer directly.  This test is
here to support on-going discussion concerning set_pixel, bitblt, and
accelerated commands... */
void
framebuffer_test(uint32_t drawable_key)
{
  Message m;
  m.snd_invKey = drawable_key;
  m.snd_code = 9999;
  m.snd_key0 = KR_VOID;
  m.snd_key1 = KR_VOID;
  m.snd_key2 = KR_VOID;
  m.snd_rsmkey = KR_VOID;
  m.snd_data = 0;
  m.snd_len = 0;
  m.snd_w1 = 0;
  m.snd_w2 = 0;
  m.snd_w3 = 0;

  m.rcv_rsmkey = KR_VOID;
  m.rcv_key0 = KR_VOID;
  m.rcv_key1 = KR_VOID;
  m.rcv_key2 = KR_VOID;
  m.rcv_data = 0;
  m.rcv_limit = 0;
  m.rcv_code = 0;
  m.rcv_w1 = 0;
  m.rcv_w2 = 0;
  m.rcv_w3 = 0;

  CALL(&m);
}

int
main(void)
{
  struct pci_dev_data probe_result;
  uint32_t result;
  uint32_t probe_info[2];
  bool success = false;

  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_SLEEP, KR_SLEEP);
  node_extended_copy(KR_CONSTIT, KC_SVGA_DRIVER_C, KR_SVGA_DRIVER_C);
  node_extended_copy(KR_CONSTIT, KC_PS2READER, KR_PS2READER);
  node_extended_copy(KR_CONSTIT, KC_PS2MOUSE_HELPER, KR_PS2MOUSE_HELPER);
  node_extended_copy(KR_CONSTIT, KC_PS2KEY_HELPER, KR_PS2KEY_HELPER);
  node_extended_copy(KR_CONSTIT, KC_PCI_PROBE_C, KR_PCI_PROBE_C);

  /* Fabricate a start key to self */
  if (capros_Process_makeStartKey(KR_SELF, 0, KR_START) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't fabricate a start key "
	    "to myself...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "TEST SAYS HI!\n");

  /* First construct the PS2 reader */
  if (constructor_request(KR_PS2READER, KR_BANK, KR_SCHED, KR_VOID,
			  KR_PS2READER) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: eventmgr unable to construct PS2 reader\n");
    return -1;
  }

  /* Initialize the PS2 reader.  If this fails, we obviously don't
     need to construct the PS2 helpers. */
  result = capros_Ps2_initPs2(KR_PS2READER);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: ps2 reader wouldn't initialize: 0x%08x\n",
	    result);
    return -1;
  }

  /* Construct the helpers and pass them key to PS2 reader */
  if (constructor_request(KR_PS2MOUSE_HELPER, KR_BANK, KR_SCHED, KR_PS2READER, 
			  KR_PS2MOUSE_HELPER) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: test unable to construct PS2 mouse "
	    "helper!\n");
    return -1;
  }

  if (constructor_request(KR_PS2KEY_HELPER, KR_BANK, KR_SCHED, KR_PS2READER, 
			  KR_PS2KEY_HELPER) != RC_OK) {
    kprintf(KR_OSTREAM, "**ERROR: test unable to construct PS2 key "
	    "helper!\n");
    return -1;
  }

  kprintf(KR_OSTREAM, "About to construct video driver domain...");
  if (constructor_request(KR_SVGA_DRIVER_C, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_SVGA_DRIVER) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct svga driver...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  kprintf(KR_OSTREAM, "About to construct PCI probe domain...");
  if (constructor_request(KR_PCI_PROBE_C, KR_BANK, KR_SCHED, KR_VOID, 
			  KR_PCI_PROBE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't construct pci probe...\n");
    return -1;   /* FIX:  really need to terminate gracefully */
  }

  /* Probe the PCI bus for the VMWare video device */
  if (pciprobe_initialize(KR_PCI_PROBE) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: pci probe failed.");
    return -1;
  }

  /* Get the base address register for the VMWare video device */
  if (pciprobe_vendor_next(KR_PCI_PROBE, VMWareVENDORID, 
			   0, &probe_result) != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: couldn't find VMWare device.");
    return -1;
  }

  /* Initialize the video driver */
   probe_info[0] = probe_result.device;
   probe_info[1] = probe_result.base_address[0];

   result = video_initialize(KR_SVGA_DRIVER,
			     sizeof(probe_info) / sizeof(uint32_t),
			     probe_info);

   switch (result) {
  case RC_OK:
    {
      kprintf(KR_OSTREAM, "SUCCESS...\n");
      success = true;
    }
    break;
  case RC_Video_BusError:
    {
      kprintf(KR_OSTREAM, "*** HW ERROR: BAD PCI BAR ***\n");
    }
    break;
  case RC_Video_HWError:
    {
      kprintf(KR_OSTREAM, "*** HW ERROR: BAD VERSION OF VMWARE ***\n");
    }
    break;
  case RC_Video_MemMapFailed:
    {
      kprintf(KR_OSTREAM, "*** HW ERROR: FAILED TO MAP FRAMEBUFFER OR FIFO ***\n");
    }
    break;
  case RC_Video_HWInitFailed:
    {
      kprintf(KR_OSTREAM, "*** HW ERROR: HW INIT FAILED ***\n");
    }
    break;
  case RC_Video_AccelError:
    {
      kprintf(KR_OSTREAM, "*** HW ERROR: FIFO INIT FAILED ***\n");
    }
    break;
  default:
    kprintf(KR_OSTREAM, "*** Unknown Error ***\n");
    break;
  };

  if (success) {
    uint32_t max_w;
    uint32_t max_h;
    uint32_t max_d;

    get_max_resolution(KR_SVGA_DRIVER, &max_w, &max_h, &max_d);
    success = set_resolution(KR_SVGA_DRIVER, max_w, max_h, max_d);
  }

  success = show_card_functionality(KR_SVGA_DRIVER);

  if (success)
    result = video_get_drawable_key(KR_SVGA_DRIVER, KR_DRAWABLE);

  /* Now here's where we test some Drawable commands: */
  if (result == RC_OK) {

    if (drawable_clear(KR_DRAWABLE, BLACK) == RC_OK) {

      if (test_matrix & PIXMAP) {
	uint32_t result = RC_capros_key_UnknownRequest; /* until proven otherwise */

	kprintf(KR_OSTREAM, "TEST:: now defining logo pixmap...\n");
	result = video_define_pixmap(KR_SVGA_DRIVER, 0, LOGO_WIDTH,
				     LOGO_HEIGHT, LOGO_DEPTH, LOGO_DATA);
	if (result == RC_OK) {

	  /* Tile the pixmap to the entire screen */
	  video_render_pixmap(KR_SVGA_DRIVER, 0, 0, 0, screen_width, 
			      screen_height);

	  capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP);
	}
      }

      /* Try a RECTFILL */
      if (test_matrix & RECTFILL) {
	uint32_t result = RC_capros_key_UnknownRequest; /* until proven otherwise */

	kprintf(KR_OSTREAM, "TEST:: now trying a rectfill...\n");
	result = drawable_rectfill(KR_DRAWABLE, 100, 100, 150, 150, GREEN,
				   ROP_COPY);
	if (result == RC_OK)
	  capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP);
	/* clear the rectangles by redrawing the logo pixmap */
	if (test_matrix & PIXMAP) {
	  video_render_pixmap(KR_SVGA_DRIVER, 0, 0, 0, screen_width, 
			      screen_height);
	  capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP);
	}
      }

      if (test_matrix & LINEDRAW) {
	
	kprintf(KR_OSTREAM, "TEST:: now trying a linedraw...\n");
	line_test(KR_DRAWABLE);

	capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP);

	/* clear the lines by redrawing the logo pixmap */
	if (test_matrix & PIXMAP) {
	  video_render_pixmap(KR_SVGA_DRIVER, 0, 0, 0, screen_width, 
			      screen_height);
	  capros_Sleep_sleep(KR_SLEEP, OBSERVER_SLEEP);
	}
      }


      if (test_matrix & CURSOR) {
	kprintf(KR_OSTREAM, "TEST:: now trying cursor test...\n");
	cursor_test(KR_DRAWABLE, KR_SVGA_DRIVER);
      }

      if (test_matrix & DESIGN) {
	kprintf(KR_OSTREAM, "TEST:: now drawing pixels...\n");
	if (drawable_clear(KR_DRAWABLE, BLACK) == RC_OK)
	  pixel_designs(KR_DRAWABLE);
      }

      if (test_matrix & RESOLUTION) {
	kprintf(KR_OSTREAM, "TEST:: now switching resolution...\n");
	if (drawable_clear(KR_DRAWABLE, BLACK) == RC_OK) {
	  set_resolution(KR_SVGA_DRIVER, 760, 480, 32);
	  pixel_designs(KR_DRAWABLE);
	}
      }

      /* Cute kangaroo in jail followed by rectfills... */
      if (test_matrix & KANGAROO) {
	uint32_t x,y;
	uint32_t rop = ROP_CLEAR;
	uint32_t result = RC_capros_key_UnknownRequest; /* until proven otherwise */

	kprintf(KR_OSTREAM, "TEST:: now defining kangaroo pixmap...\n");
	result = video_define_pixmap(KR_SVGA_DRIVER, 1, ROO_WIDTH, ROO_HEIGHT,
				     ROO_DEPTH, ROO_DATA);
	if (result != RC_OK) {
	  kprintf(KR_OSTREAM, "*** ERROR: video_define_pixmap() result=%u",
		  result);
	} else {
	  if (video_render_pixmap(KR_SVGA_DRIVER, 1, 0, 0, screen_width, 
			    screen_height) == RC_OK) {

	    kprintf(KR_OSTREAM, "TEST:: now trying a bunch of rectfill "
		    "rop's...\n");

	    for (x = 0; x < screen_width; x=x+ROO_WIDTH)
	      for (y = 0; y < screen_height; y=y+ROO_HEIGHT) {

		drawable_rectfill(KR_DRAWABLE, x, y, x+ROO_WIDTH, y+ROO_HEIGHT,
				      0x3a4f * y + x, rop);
		rop++;
		rop = (rop > ROP_SET) ? ROP_CLEAR : rop;

		capros_Sleep_sleep(KR_SLEEP, 50);
	      }
	  }
	}
      }

      /* Here's the paint program: */
      if (test_matrix & PAINT)
	paint(KR_DRAWABLE, KR_SVGA_DRIVER);
    }
  }
  return 0;
}

