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

#include <stdlib.h>
#include <string.h>
#include <eros/target.h>
#include <domain/domdbg.h>
#include <graphics/fonts/Font.h>
#include <domain/DrawableKey.h>

#include "Button.h"
#include "Decoration.h"
#include "winsyskeys.h"
#include "coordxforms.h"

#include "fbmgr/drivers/video.h"
#include "sessionmgr/Session.h"

#include "pixmaps/ids.h"
#include "pixmaps/killbutton.h"
#include "pixmaps/killbutton_in.h"
#include "pixmaps/verticalborder.h"
#include "pixmaps/horizontalborder.h"
#include "pixmaps/topleftcorner.h"
#include "pixmaps/toprightcorner.h"
#include "pixmaps/bottomleftcorner.h"
#include "pixmaps/bottomrightcorner.h"
#include "pixmaps/bottomborder.h"

#include "debug.h"
#include "global.h"
#include "xclipping.h"

#define WIN_BORDER_WIDTH       (hori_height)
#define WIN_TITLEBAR_HEIGHT   20
#define WIN_MESSAGEBAR_HEIGHT  (bottomborder_height)
#define WIN_BOTTOMCORNER_WIDTH (bottomcorner_width)

/* Define a scratch pixel value buffer for drawing the title bar part
   of the decoration.  (Since window titles are dynamic we can't
   precompute the pixel pattern.) */
#define TITLEBAR_BUFFER_WIDTH (1024)
#define TITLEBAR_BUFFER_HEIGHT  (WIN_TITLEBAR_HEIGHT)
uint32_t tb_scratch[TITLEBAR_BUFFER_HEIGHT * TITLEBAR_BUFFER_WIDTH];

static bool pixmaps_loaded = false;

/* External declarations */
extern Window *focus;
extern void winsys_change_focus(Window *from, Window *to);
extern void window_unmap_pieces(Window *w, Window *parent, 
				clip_vector_t **pieces);

/* Forward declarations */
static void decoration_move_ghost(Decoration *dec, point_t new_origin);
static void decoration_draw_ghost(Decoration *dec, point_t orig_delta, 
				  point_t size_delta);
static void decoration_move(Window *w, point_t orig_delta, point_t size_delta);
static void compute_hot_spots(Decoration *dec);

#define decoration_reset_hotspot(dec) { \
    dec->offset_computed = false;    \
    dec->offset.x = 0;               \
    dec->offset.y = 0;               \
    dec->active_hotspot = -1;         } \

/* Action callback for kill button */
static void
action_kill(Window *w)
{
  if (w->type != WINTYPE_BUTTON || w->parent->type != WINTYPE_DECORATION)
    kdprintf(KR_OSTREAM, "Predicate failure in action_kill().\n");

  /* At this point, the kill button will have focus.  Before actually
  killing anybody, the kill button must relinquish focus in order for
  the kill sequence to succeed. */
  if (!WINDOWS_EQUAL(w, focus))
    kdprintf(KR_OSTREAM, "Predicate failure in action_kill(): kill button "
	     "doesn't have focus!\n");

  focus = w->parent;

  /* Kill the decoration, which will kill all its children */
  window_destroy(w->parent, true);  
}

/* Determine which region of the decoration window cursor is in.
   Location is in window coords. */
static int32_t
decoration_find_hotspot(Decoration *dec, point_t location)
{
  int32_t u;

  DEBUG(decoration) kprintf(KR_OSTREAM, "find hotspot...\n");

  for (u = 0; u < TOTAL_HOT_SPOTS; u++) {

    DEBUG(decoration)
      kprintf(KR_OSTREAM, "   Trying [%d] w/bounds [(%d,%d)(%d,%d)] "
	      "and loc (%d,%d)\n", u, dec->hotspots[u].bounds.topLeft.x,
	      dec->hotspots[u].bounds.topLeft.y,
	      dec->hotspots[u].bounds.bottomRight.x,
	      dec->hotspots[u].bounds.bottomRight.y,
	      location.x, location.y);

    if (rect_contains_pt(&(dec->hotspots[u].bounds), &location))
      return u;
  }

  /* The hotspots cover all possible areas of the decoration window.
     It's an error if no containing hotspot is found! */
  kdprintf(KR_OSTREAM, "ERROR: No dec hotspot found.\n");
  return -1;
}

/* A convenience wrapper for calling the actual hotspot execute
   callback. */
static void
decoration_execute_hotspot(Decoration *dec, uint32_t buttons, 
			   point_t location, bool dragging)
{
  if (dec->active_hotspot == - 1 || dec->active_hotspot >= TOTAL_HOT_SPOTS)
    kdprintf(KR_OSTREAM, 
	     "Predicate failure in decoration_execute_hotspot().\n");

  if (dragging && !dec->offset_computed) {
    dec->offset = location;
    dec->offset_computed = true;
    return;
  }

  dec->hotspots[dec->active_hotspot].execute(dec, buttons, location,
					     dragging);
}

/* Handler for decoration-related mouse events */
void
decoration_deliver_mouse_event(Window *w, uint32_t buttons, point_t location,
			       bool dragging)
{
  point_t cursor_win;
  Decoration *dec = (Decoration *)w;

  /* Xlate cursor location into window coords so we can see what
     hotspots to activate */
  if (!xform_point(w, location, ROOT2WIN, &cursor_win))
    return;

  DEBUG(decoration) 
   kprintf(KR_OSTREAM, "DEC: deliver mouse event. loc (%d,%d) dragging [%s] "
	    "active [%d]\n",
	    cursor_win.x, cursor_win.y, dragging ? "TRUE" : "FALSE",
	    dec->active_hotspot);

  /* If user is dragging cursor pointer, deliver this event to the
     current hotspot.  Note cursor location is more than likely
     outside the bounds of the active hotspot. */
  if (dragging && dec->active_hotspot > -1)
    decoration_execute_hotspot(dec, buttons, cursor_win, dragging);

  /* If user is not dragging but has pressed left mouse, find new
     hotspot and deliver the event */
  else if (dragging && dec->active_hotspot == -1) {
    dec->active_hotspot = decoration_find_hotspot(dec, cursor_win);
    decoration_execute_hotspot(dec, buttons, cursor_win, dragging);
  }

  /* This is the case where the cursor is either passing over a
     hotspot (with no left mouse pressed) OR it's the "left mouse up"
     after dragging. */
  else if (!(buttons & MOUSE_LEFT)) {
    if (dec->active_hotspot > -1)
      decoration_execute_hotspot(dec, buttons, cursor_win, dragging);
  }
}

/* Handler for decoration-related key events */
void
decoration_deliver_key_event(Window *w, uint32_t keycode)
{
}

/* Render titlebar 
 *
 * Note: the 'hasFocus' is a separate parameter because the title bar
 * needs to be rendered as if it had focus in the event that any of its
 * children have focus (which means that the decoration window itself
 * does NOT actually have focus:  w->hasFocus would be 'false'). 
 */
static void
decoration_renderTitleBar(Decoration *d, rect_t region, bool hasFocus)
{
  uint32_t u;
  point_t fb_start;
  point_t name_offset = {5,3};
  rect_t buffer_rect;
  rect_t region_c;
  Window *w = &(d->win);
  rect_t rect = { {0,0}, w->size}; 
  rect_t tmp;

  DEBUG(focus) {
    kprintf(KR_OSTREAM, "tb render: name [%s] w/focus [%s]\n", 
	    w->name, hasFocus ? "YES" : "NO");
  }

  /* Ensure clipping area doesn't exceed window dimensions */
  xform_rect(w, region, ROOT2WIN, &region_c);
  if (!rect_intersect(&rect, &region_c, &rect))
    return;

  /* Set user clip region to the intersection of title bar and
     current drawing boundary */
  if (rect_intersect(&(d->hotspots[TITLEBAR].bounds), &region_c, &tmp)) {
    w->userClipRegion = tmp;

    for (u = 0; 
	 u < ((w->size.x - 2*WIN_BORDER_WIDTH) * WIN_TITLEBAR_HEIGHT); u++)
      tb_scratch[u] = GRAY75;

    /* render window's name in titlebar */
    DEBUG(titlebar) { 
      kprintf(KR_OSTREAM, "decoration_draw() calling font_render w/[%s]\n",
	      w->name);
      kprintf(KR_OSTREAM, "   and clip region is [(%d,%d)(%d,%d)]\n",
	      tmp.topLeft.x, tmp.topLeft.y,
	      tmp.bottomRight.x, tmp.bottomRight.y);
      kprintf(KR_OSTREAM, "   and 'u' is %u\n", u);
    }

    font_render(&DefaultFont,(uint8_t *)tb_scratch, 
		w->size.x - 2*WIN_BORDER_WIDTH,
		name_offset,
		YELLOW, 
		GRAY75, FONT_NORMAL,
		w->name);

    /* For rendering without focus, lighten the titlebar a bit */
    if (!hasFocus) {
      int32_t x, y;

      for (y = 0; y < WIN_TITLEBAR_HEIGHT; y++) {
	if (y % 2)
	  for (x = 0; x < w->size.x - 2*WIN_BORDER_WIDTH; x=x+2)
	    tb_scratch[x + (w->size.x - 2*WIN_BORDER_WIDTH) * y] |= 0x00A0A0A0;
	else 
	  for (x = 1; x < w->size.x - 2*WIN_BORDER_WIDTH; x=x+2)
	    tb_scratch[x + (w->size.x - 2*WIN_BORDER_WIDTH) * y] |= 0x00A0A0A0;
      }
    }

    /* In the call to video_copy_to_fb() we need Root coordinates,
       so xform appropriately: */
    xform_point(w, w->userClipRegion.topLeft, WIN2ROOT, &fb_start);

    DEBUG(titlebar)
      kprintf(KR_OSTREAM, "Calling video_copy_to_fb() with "
	      "fb_start (%d,%d)\n", fb_start.x, fb_start.y);

    /* Since we blt'ed directly to framebuffer, need to tell it to
       update.  However, we need to make sure the user clip region
       is expressed in terms of the scratch buffer! */
    buffer_rect = w->userClipRegion;
    buffer_rect.topLeft.x -= WIN_BORDER_WIDTH;
    buffer_rect.topLeft.y -= WIN_BORDER_WIDTH;
    buffer_rect.bottomRight.x -= WIN_BORDER_WIDTH;
    buffer_rect.bottomRight.y -= WIN_BORDER_WIDTH;
    video_copy_to_fb(tb_scratch, buffer_rect, fb_start, 
		     w->size.x - 2*WIN_BORDER_WIDTH);
  }
}

static void
render_border_section(Window *w, rect_t bounds, rect_t requested_section,
		      uint32_t pixmap_id)
{
  rect_t r_conv;
  rect_t tmp;
  point_t offset = {0, 0};

  /* Only render the intersection between the maximum bounds for this
     section and the requested region */
  if (rect_intersect(&requested_section, &bounds, &tmp)) {

    if (tmp.topLeft.x < bounds.topLeft.x)
      kdprintf(KR_OSTREAM, "Predicate failure in render_border_section: "
	       "tmp.x=%d < bounds.x=%d\n", tmp.topLeft.x, bounds.topLeft.x);

    /* Compute the relative offset into the pixmap */
    if (tmp.topLeft.x > bounds.topLeft.x)
      offset.x = tmp.topLeft.x - bounds.topLeft.x;

    if (tmp.topLeft.y > bounds.topLeft.y)
      offset.y = tmp.topLeft.y - bounds.topLeft.y;

    /* Currently use pixmaps for window borders.  If you don't want
       to rely on pixmaps, use the following to just render a filled
       rectangle as a border: video_rectfill(r_conv, BLACK,
       ROP_COPY) */
    if (xform_rect(w, tmp, WIN2ROOT, &r_conv))
      video_show_pixmap(pixmap_id, offset, r_conv);
  }
}

  /* Render top and side borders and top corners */
static void
decoration_render_borders(Decoration *d, rect_t region)
{
  Window *w = &(d->win);
  rect_t region_c;

  /* Translate region into decoration window coords */
  xform_rect(w, region, ROOT2WIN, &region_c);

  render_border_section(w, d->hotspots[TOP].bounds, region_c,
			DefaultHoriBorderID);

  render_border_section(w, d->hotspots[LEFT].bounds, region_c,
			DefaultVertBorderID);

  render_border_section(w, d->hotspots[RIGHT].bounds, region_c,
			DefaultVertBorderID);

  render_border_section(w, d->hotspots[TOP_RIGHT].bounds, region_c,
			DefaultTopRightCornerID);

  render_border_section(w, d->hotspots[TOP_LEFT].bounds, region_c, 
			DefaultTopLeftCornerID);
}

static void
decoration_render_message_bar(Decoration *d, rect_t region)
{
  Window *w = &(d->win);
  rect_t region_c;

  /* Translate region into decoration window coords */
  xform_rect(w, region, ROOT2WIN, &region_c);

  render_border_section(w, d->hotspots[BOTTOM].bounds, region_c,
			DefaultBottomBorderID);

  render_border_section(w, d->hotspots[BOTTOM_LEFT].bounds, region_c,
			DefaultBottomLeftCornerID);

  render_border_section(w, d->hotspots[BOTTOM_RIGHT].bounds, region_c,
			DefaultBottomRightCornerID);
}

void
decoration_draw(Window *w, rect_t region)
{
  Decoration *d = (Decoration *)w;

  if(w->type != WINTYPE_DECORATION)
    kdprintf(KR_OSTREAM, "Predicate failure in decoration_draw().\n");

  DEBUG(decoration)
    kprintf(KR_OSTREAM, "decoration_draw(): with region = [(%d,%d)(%d,%d)]\n",
	    region.topLeft.x, region.topLeft.y, region.bottomRight.x,
	    region.bottomRight.y);

  /* Title bar is handled separately because it changes appearance
     depending on focus */
  if (w->hasFocus || (focus->parent && WINDOWS_EQUAL(focus->parent, w)))
    decoration_renderTitleBar(d, region, true);
  else
    decoration_renderTitleBar(d, region, false);

  /* Render top, sides and corners of window */
  decoration_render_borders(d, region);

  /* Render bottom of window.  Currently this is wide/tall enough to
     be a "message bar" and thus is handled separately.  To make the
     bottom border just like the top border, simply follow the logic
     in decoration_render_border() used to render the top border. */
  decoration_render_message_bar(d, region);
}

static point_t
getDecOrigin(point_t content_origin)
{
  point_t dec_orig = content_origin; 

  /* Decoration origin is a little to left and above content origin */
  dec_orig.x -= WIN_BORDER_WIDTH;
  dec_orig.y -= (WIN_TITLEBAR_HEIGHT + WIN_BORDER_WIDTH);

  return dec_orig;
}

static point_t
getDecSize(point_t content_size)
{
  point_t decsize = content_size;

  /* Width of decoration window is a little bigger than content */
  decsize.x += 2 * WIN_BORDER_WIDTH;

  /* Height needs to be adjusted to make room for top border, trusted
     banner (if needed), title bar, and lower border (message bar) */
  decsize.y += WIN_TITLEBAR_HEIGHT + WIN_MESSAGEBAR_HEIGHT + WIN_BORDER_WIDTH;

  return decsize;
}

void
decoration_set_focus(Window *w, bool hasFocus)
{
  w->hasFocus = hasFocus;

  if (hasFocus)
    window_bring_to_front(w);
  
  /* Make sure the entire titlebar is rendered appropriately (not just
     the pieces that were obstructed..) But, don't do this if
     decoration is the parent of the current focus! (because that will
     undo the proper visual effect) */
  if (!WINDOWS_EQUAL(focus->parent, w))
    w->render_focus(w, hasFocus);
}

void
decoration_render_focus(Window *w, bool hasFocus)
{
  uint32_t u;
  clip_vector_t *dec_pieces = NULL;
  Decoration *d = (Decoration *)w;

  if(w->type != WINTYPE_DECORATION)
    kdprintf(KR_OSTREAM, "Predicate failure in decoration_render_focus().\n");

  if (!w->mapped)
    return;

  if (!window_ancestors_mapped(w))
    return;

  dec_pieces = window_get_subregions(w, UNOBSTRUCTED);

  DEBUG(focus)
    kprintf(KR_OSTREAM, "dec render: name [%s] [%u] pieces w/focus [%s]\n", 
	    w->name, dec_pieces->len, hasFocus ? "YES" : "NO");

  for (u = 0; u < dec_pieces->len; u++) {
    decoration_renderTitleBar(d, dec_pieces->c[u].r, hasFocus);
  }
}

static void
load_pixmaps(void) 
{
  point_t vert_size = {vert_width, vert_height};
  point_t hori_size = {hori_width, hori_height};
  point_t corner_size = {corner_width, corner_height};
  point_t bottomcorner_size = {bottomcorner_width, bottomcorner_height};
  point_t bottomborder_size = {bottomborder_width, bottomborder_height};
  point_t kill_size = {kill_width, kill_height};

  video_define_pixmap(DefaultVertBorderID, vert_size, vert_depth, 
		      vertborder_pixmap);
  video_define_pixmap(DefaultHoriBorderID, hori_size, hori_depth, 
		      horiborder_pixmap);
  video_define_pixmap(DefaultTopLeftCornerID, corner_size, corner_depth, 
		      topleftcorner_pixmap);
  video_define_pixmap(DefaultTopRightCornerID, corner_size, corner_depth, 
		      toprightcorner_pixmap);
  video_define_pixmap(DefaultBottomLeftCornerID, bottomcorner_size, 
		      bottomcorner_depth, bottomleftcorner_pixmap);
  video_define_pixmap(DefaultBottomRightCornerID, bottomcorner_size, 
		      bottomcorner_depth, bottomrightcorner_pixmap);
  video_define_pixmap(DefaultBottomBorderID, bottomborder_size,
		      bottomborder_depth, bottomborder_pixmap);
  video_define_pixmap(DefaultKillButtonInID, kill_size, 
		      kill_depth, kill_in_pixmap);
  video_define_pixmap(DefaultKillButtonOutID, kill_size, 
		      kill_depth, kill_pixmap);
}

static void
titlebar_execute(Decoration *dec, uint32_t buttons, point_t location,
		 bool dragging)
{
  point_t delta;
  point_t null_delta = {0, 0};

  DEBUG(titlebar)
    kprintf(KR_OSTREAM, "Executing titlebar callback w/ loc (%d,%d) "
	  "buttons 0x%08x dragging [%s]\n", location.x, location.y,
	  buttons, dragging ? "TRUE" : "FALSE");

 
  delta.x = location.x - dec->offset.x;
  delta.y = location.y - dec->offset.y;

  /* Need to ensure that window movement is appropriately limited by
     parent's dimensions/location.  i.e. don't let user move a
     window completely outside of its parent window. Adjust these
     limits as needed to limit window movement even further. */
  if (delta.x < 0) {
    if (dec->win.origin.x + delta.x + dec->win.size.x < MIN_WINDOW_WIDTH)
      delta.x = -(dec->win.origin.x + dec->win.size.x - MIN_WINDOW_WIDTH);
  }

  if (delta.x > 0) {
    if (dec->win.origin.x + delta.x > 
	(dec->win.parent->size.x - MIN_WINDOW_WIDTH))
      delta.x = dec->win.parent->size.x - dec->win.origin.x - MIN_WINDOW_WIDTH;
  }

  if (delta.y < 0) {
    if (dec->win.origin.y + delta.y < 0)
      delta.y = -(dec->win.origin.y);
  }

  if (delta.y > 0) {
    if (dec->win.origin.y + delta.y > 
	dec->win.parent->size.y - MIN_WINDOW_HEIGHT)
      delta.y = dec->win.parent->size.y - dec->win.origin.y - 
	MIN_WINDOW_HEIGHT;
  }

  if (dragging) {
    /* If delta is not 0 */
    if (delta.x != 0 || delta.y != 0)
      /* Move ghost image */
      decoration_move_ghost(dec, delta);
  }
  else if (!dragging) {
    if (delta.x != 0 || delta.y != 0)
      decoration_move(&(dec->win), delta, null_delta);
    decoration_reset_hotspot(dec);
  }
}

/* Resize window via bottom left corner: this potentially changes the origin
   as well as the size of decoration. */
static void
bottom_left_execute(Decoration *dec, uint32_t buttons, point_t location,
		    bool dragging)
{
  point_t orig_delta;
  point_t size_delta;

  Window *resize_win = &(dec->win);

  /* The size_delta.x computation is opposite that of the bottom right
     corner! */
  size_delta.x = dec->offset.x - location.x;
  size_delta.y = location.y - dec->offset.y;

  /* We have to consider a change in the origin as well, but only in
     the x direction. */
  orig_delta.x = location.x - dec->offset.x;
  orig_delta.y = 0;

  if (size_delta.x == 0 && size_delta.y == 0) {
    decoration_reset_hotspot(dec);
    return;
  }

  /* Don't let user make window too small */
  if (resize_win->size.x + size_delta.x < MIN_WINDOW_WIDTH)
    size_delta.x = MIN_WINDOW_WIDTH - resize_win->size.x;

  if (resize_win->size.y + size_delta.y < MIN_WINDOW_HEIGHT)
    size_delta.y = MIN_WINDOW_HEIGHT - resize_win->size.y;

  /* One additional check to ensure above with regard to origin */
  if (orig_delta.x > resize_win->size.x - MIN_WINDOW_WIDTH)
    orig_delta.x = resize_win->size.x - MIN_WINDOW_WIDTH;

  if (dragging) {
    if (dec->undraw_old_ghost)
      decoration_draw_ghost(dec, dec->ghost_orig_delta, dec->ghost_size_delta);

    decoration_draw_ghost(dec, orig_delta, size_delta);
  }
  else {
    decoration_move(&(dec->win), orig_delta, size_delta);
    decoration_reset_hotspot(dec);
  }
}

/* Resize window via bottom right corner: this will not change origin
   of decoration, but will change the size. */
static void
bottom_right_execute(Decoration *dec, uint32_t buttons, point_t location,
		     bool dragging)
{
  point_t null_delta = {0, 0};
  point_t size_delta;

  Window *resize_win = &(dec->win);

  size_delta.x = location.x - dec->offset.x;
  size_delta.y = location.y - dec->offset.y;

  if (size_delta.x == 0 && size_delta.y == 0) {
    decoration_reset_hotspot(dec);
    return;
  }

  /* Don't let user make window too small */
  if (resize_win->size.x + size_delta.x < MIN_WINDOW_WIDTH)
    size_delta.x = MIN_WINDOW_WIDTH - resize_win->size.x;

  if (resize_win->size.y + size_delta.y < MIN_WINDOW_HEIGHT)
    size_delta.y = MIN_WINDOW_HEIGHT - resize_win->size.y;

  if (dragging) {
    if (dec->undraw_old_ghost)
      decoration_draw_ghost(dec, dec->ghost_orig_delta, dec->ghost_size_delta);

    decoration_draw_ghost(dec, null_delta, size_delta);
  }
  else {
    decoration_move(&(dec->win), null_delta, size_delta);
    decoration_reset_hotspot(dec);
  }
}

static void
bottom_execute(Decoration *dec, uint32_t buttons, point_t location,
	       bool dragging)
{

  if (!dragging)
    decoration_reset_hotspot(dec);
}

static void
top_execute(Decoration *dec, uint32_t buttons, point_t location,
	    bool dragging)
{
  if (!dragging)
    decoration_reset_hotspot(dec);
}

static void
left_execute(Decoration *dec, uint32_t buttons, point_t location,
	     bool dragging)
{
  if (!dragging)
    decoration_reset_hotspot(dec);
}

static void
right_execute(Decoration *dec, uint32_t buttons, point_t location,
	      bool dragging)
{
  if (!dragging)
    decoration_reset_hotspot(dec);
}

static void
top_right_execute(Decoration *dec, uint32_t buttons, point_t location,
		  bool dragging)
{
  if (!dragging)
    decoration_reset_hotspot(dec);
}

static void
top_left_execute(Decoration *dec, uint32_t buttons, point_t location,
		  bool dragging)
{
  if (!dragging)
    decoration_reset_hotspot(dec);
}

static void
compute_hot_spots(Decoration *dec)
{
  Window *w = &(dec->win);

  rect_t blc = { {0, w->size.y - WIN_MESSAGEBAR_HEIGHT},
		 {WIN_BOTTOMCORNER_WIDTH, w->size.y} };

  rect_t brc = { {w->size.x - WIN_BOTTOMCORNER_WIDTH,
		  w->size.y - WIN_MESSAGEBAR_HEIGHT},
		 {w->size.x, w->size.y} };

  rect_t bb = { {WIN_BOTTOMCORNER_WIDTH, 
		 w->size.y - WIN_MESSAGEBAR_HEIGHT}, 
		{w->size.x - WIN_BOTTOMCORNER_WIDTH, 
		 w->size.y} };

  rect_t top = { {WIN_BORDER_WIDTH, 0}, 
		{w->size.x - WIN_BORDER_WIDTH, 
		 WIN_BORDER_WIDTH} };

  rect_t lb = { {0, WIN_BORDER_WIDTH}, 
		{WIN_BORDER_WIDTH, 
		 w->size.y - WIN_MESSAGEBAR_HEIGHT} };

  rect_t rb = { {w->size.x - WIN_BORDER_WIDTH,
		 WIN_BORDER_WIDTH},
		{w->size.x, 
		 w->size.y - WIN_MESSAGEBAR_HEIGHT} };

  rect_t tl = { {0,0}, {WIN_BORDER_WIDTH, WIN_BORDER_WIDTH} };

  rect_t tr = { {w->size.x - WIN_BORDER_WIDTH, 0},
		{w->size.x, WIN_BORDER_WIDTH} };

  rect_t tb = { {WIN_BORDER_WIDTH, WIN_BORDER_WIDTH}, 
		{w->size.x - WIN_BORDER_WIDTH - kill_width, 
		 WIN_TITLEBAR_HEIGHT + WIN_BORDER_WIDTH} };

  /* Now assign values to globals */
  dec->hotspots[BOTTOM_LEFT].bounds = blc;
  dec->hotspots[BOTTOM_RIGHT].bounds = brc;
  dec->hotspots[BOTTOM].bounds = bb;
  dec->hotspots[TOP].bounds = top;
  dec->hotspots[LEFT].bounds = lb;
  dec->hotspots[RIGHT].bounds = rb;
  dec->hotspots[TOP_LEFT].bounds = tl;
  dec->hotspots[TOP_RIGHT].bounds = tr;
  dec->hotspots[TITLEBAR].bounds = tb;
}

static void
initialize_hot_spots(Decoration *dec)
{
  /* No active hotspot yet */
  decoration_reset_hotspot(dec);

  /* Set up the execution callbacks */
  dec->hotspots[BOTTOM_LEFT].execute = bottom_left_execute;
  dec->hotspots[BOTTOM_RIGHT].execute = bottom_right_execute;
  dec->hotspots[BOTTOM].execute = bottom_execute;
  dec->hotspots[TOP].execute = top_execute;
  dec->hotspots[LEFT].execute = left_execute;
  dec->hotspots[RIGHT].execute = right_execute;
  dec->hotspots[TOP_LEFT].execute = top_left_execute;
  dec->hotspots[TOP_RIGHT].execute = top_right_execute;
  dec->hotspots[TITLEBAR].execute = titlebar_execute;

  /* Finally, compute the bounds of each hot spot based on current
     window size */
  compute_hot_spots(dec);
}

Decoration *
decoration_create(Window *parent, point_t content_origin,
		  point_t content_size, void *session)
{
  Decoration *dec = malloc(sizeof(Decoration));
  point_t dec_origin;
  point_t dec_size;
  rect_t orig_size = { {0,0}, {0,0} };

  dec_origin = getDecOrigin(content_origin);
  dec_size   = getDecSize(content_size);
  orig_size.bottomRight = dec_size;

  memset(dec, 0, sizeof(Decoration));
  window_initialize(&(dec->win), parent, dec_origin, dec_size, session,
		    WINTYPE_DECORATION);

  DEBUG(decoration) {
    kprintf(KR_OSTREAM, "decoration_create(): orig (%d,%d) size %dx%d\n",
	    dec_origin.x, dec_origin.y, dec_size.x, dec_size.y);
    kprintf(KR_OSTREAM, "decoration_create() "
	    "allocated dec Window at 0x%08x w/sess 0x%08x", 
	    ADDRESS(&(dec->win)),
	    ADDRESS(dec->win.session));
  }

  dec->win.deliver_mouse_event = decoration_deliver_mouse_event;
  dec->win.deliver_key_event = decoration_deliver_key_event;
  dec->win.draw = decoration_draw;
  dec->win.set_focus = decoration_set_focus;
  dec->win.render_focus = decoration_render_focus;
  dec->win.move = decoration_move;

  dec->undraw_old_ghost = false;
  dec->ghost_orig_delta.x = 0;
  dec->ghost_orig_delta.y = 0;
  dec->ghost_size_delta.x = 0;
  dec->ghost_size_delta.y = 0;

  /* Establish dimensions of "hot spots" of this decoration (ie. the
     titlebar, the corners, the borders) */
  initialize_hot_spots(dec);

    /* Load pixmaps */
  if (!pixmaps_loaded) {
    load_pixmaps();
    pixmaps_loaded = true;
  }

  /* Create decoration buttons (each as a separate Window) */
  {
    point_t b_size = {kill_width, kill_height};
    point_t b_orig = {dec->win.size.x - WIN_BORDER_WIDTH - b_size.x, 
		      WIN_BORDER_WIDTH};

    dec->killButton = button_create(&(dec->win), b_orig, b_size, session, 
				    &action_kill);
    dec->killButton->in_pixmap = DefaultKillButtonInID;
    dec->killButton->out_pixmap = DefaultKillButtonOutID;

    /* The kill button is always mapped, unless you want some funky
       kill button. */
    dec->killButton->win.mapped = true;
  }

  return dec;
}

static void
decoration_draw_ghost(Decoration *dec, point_t orig_delta, point_t size_delta)
{
  Window *win = &(dec->win);
  point_t new_origin = { win->origin.x + orig_delta.x,
			 win->origin.y + orig_delta.y };
  point_t new_size = { win->size.x + size_delta.x,
		       win->size.y + size_delta.y };
  rect_t ghost   = { new_origin, 
                    {new_origin.x + new_size.x, new_origin.y + new_size.y} };

  rect_t left    = { {ghost.topLeft.x, ghost.topLeft.y}, 
		     {ghost.topLeft.x+1, ghost.bottomRight.y} };
  rect_t right   = { {ghost.bottomRight.x-1, ghost.topLeft.y}, 
		     {ghost.bottomRight.x, ghost.bottomRight.y} };
  rect_t top     = { {ghost.topLeft.x+1, ghost.topLeft.y}, 
		     {ghost.bottomRight.x-1, ghost.topLeft.y+1} };
  rect_t bottom  = { {ghost.topLeft.x+1, ghost.bottomRight.y-1}, 
		     {ghost.bottomRight.x-1, ghost.bottomRight.y} };

  color_t clr = 0x00FFFFFFu;	/* to ensure that XOR trick works */
  rect_t conv;
  rect_t parent_root;

  /* Clip ghost to all of the window's ancestors! (since windows are
     clipped to their ancestors) */
  if (!window_clip_to_ancestors(win->parent, &parent_root))
    return;

  xform_rect(win->parent, left, WIN2ROOT, &conv);
  if (rect_intersect(&parent_root, &conv, &conv))
    video_rectfill(conv, clr, ROP_XOR);

  xform_rect(win->parent, right, WIN2ROOT, &conv);
  if (rect_intersect(&parent_root, &conv, &conv))
    video_rectfill(conv, clr, ROP_XOR);

  xform_rect(win->parent, top, WIN2ROOT, &conv);
  if (rect_intersect(&parent_root, &conv, &conv))
    video_rectfill(conv, clr, ROP_XOR);

  xform_rect(win->parent, bottom, WIN2ROOT, &conv);
  if (rect_intersect(&parent_root, &conv, &conv))
    video_rectfill(conv, clr, ROP_XOR);

  /* Remember where current image is */
  dec->ghost_orig_delta = orig_delta;
  dec->ghost_size_delta = size_delta;

  dec->undraw_old_ghost = true;
}

static void 
decoration_move_ghost(Decoration *dec, point_t delta)
{
  point_t null_delta = {0, 0};

  /* First erase previous ghost image */
  if (dec->undraw_old_ghost)
    decoration_draw_ghost(dec, dec->ghost_orig_delta, null_delta);

  /* Now render new ghost image */
  decoration_draw_ghost(dec, delta, null_delta);
}

static void 
decoration_move(Window *win, point_t orig_delta, point_t size_delta)
{
  Decoration *dec = (Decoration *)win;
  clip_vector_t *hand_draw_vec = make_clip_vector(0);
  clip_vector_t *tmp_vec = NULL;
  point_t new_origin = { win->origin.x + orig_delta.x,
			 win->origin.y + orig_delta.y };
  point_t new_size = {win->size.x + size_delta.x,
		      win->size.y + size_delta.y};
  rect_t new_rect = {new_origin, 
                    {new_origin.x + new_size.x, new_origin.y + new_size.y} };

  rect_t new_rect_root;
  rect_t parent_rect_root;
  rect_t src_clipped_win;

  bool src_visible = false;
  bool dst_visible = false;
  bool changing_size = (size_delta.x != 0 || size_delta.y != 0);
  bool do_optimized_copy = false;

  if (dec->undraw_old_ghost) {
    decoration_draw_ghost(dec, dec->ghost_orig_delta, dec->ghost_size_delta);
    dec->undraw_old_ghost = false;
  }

  /* Here's the plan: find all subregions of thisWindow that are *not*
     in the rectangle(s) represented by the new location. Unmap those
     subregions (without actually setting their status to "unmapped"),
     move/resize thisWindow, and ask window system to redraw
     the window. */
  {
    clip_vector_t *exposelist = NULL;

    xform_rect(win->parent, new_rect, WIN2ROOT, &new_rect_root);

    /* Generate the list of areas that will be newly exposed as a
       result of the movement of this window */
    exposelist = window_newly_exposed(win, new_rect_root);

    DEBUG(move) 
    {
      kprintf(KR_OSTREAM, "*window_move() about to call unmap with "
	      "exposelist:\n");
      vector_dump(exposelist);
    }

    /* If size is not changing (i.e. the window is being moved only),
    then an optimization is to ask the video subsystem to move this
    window directly by copying from a src region of the framebuffer to
    a dst region.  The optimization is that we don't have to call
    window_draw()! But, things get tricky if part of the window is
    clipped by an ancestor: in such cases the video subsystem can only
    move the visible part and must manually redraw the clipped
    part(s). At some point, attempting this optimization isn't worth
    it:  e.g. if the final location of the window results in a small
    number of visible pixels. So, first determine if the optimization
    is even worth trying by (arbitrarily) checking if 50% of the
    window will still be visible once it's moved. */
    if (!changing_size) {
      rect_t src_rect_root;
      rect_t src_clipped_root;
      rect_t dst_clipped_root;

      tmp_vec = make_clip_vector(0);

      /* Determine the rectangle describing the window. */
      xform_win2rect(win, WIN2ROOT, &src_rect_root);

      /* Determine how much of that is actually visible. If no part is
	 visible, then we don't have to worry about asking video
	 subsystem to copy any portion.  We just move to the logic for
	 determining what parts of the destination are visible. */
      src_visible = window_clip_to_ancestors(win, &src_clipped_root);

      /* Stash the result in window coordinates for later use */
      xform_rect(win, src_clipped_root, ROOT2WIN, &src_clipped_win);

      /* Need a temporary Window var to represent location of Window
	 after it's moved */
      {
	Window tmp;
	rect_t z;

	memcpy(&tmp, win, sizeof(Window));
	tmp.origin.x += orig_delta.x;
	tmp.origin.y += orig_delta.y;

	/* 'z' will represent the total possible area of the window
	   that will be visible once moved */
	dst_visible = window_clip_to_ancestors(&tmp, &z);

	/* Transform the 'src_clipped_win' to root coords using this
	temporary Window object. The result is the 'src_clipped_root'
	in root coords relative to the *moved* window.  One more step,
	though, is to clip that to all its ancestors! */
	xform_rect(&tmp, src_clipped_win, WIN2ROOT, &dst_clipped_root);

	/* Intersect this with 'z' above */
	if (!rect_intersect(&z, &dst_clipped_root, &dst_clipped_root)) {

	  /* Visible src is getting moved off screen, so just zero out
	     dst */
	  dst_clipped_root.topLeft.x = dst_clipped_root.topLeft.y = 0;
	  dst_clipped_root.bottomRight.x = dst_clipped_root.bottomRight.y = 0;
	  src_visible = false;
	}

	/* Last but not least: put that result back in 'tmp' window
           coords and xform it to root coords relative to the original
           window.  This will give us the no-kidding src region in
           root coords! */
	xform_rect(&tmp, dst_clipped_root, ROOT2WIN, &src_clipped_win);
	xform_rect(win, src_clipped_win, WIN2ROOT, &src_clipped_root); 
      }

      /* "src_clipped_root" is the region of the window currently
      visible.  Make sure that's at least 50% of the original window.
      "dst_clipped_root" is the region that will be visible once
      moved.  Make sure that's at least 50% of the original as well.
      Otherwise, this optimization probably isn't worth it and we'll
      just manually compute and redraw all visible regions later. */
      do_optimized_copy = 
	( src_visible && dst_visible &&
         (((src_clipped_root.bottomRight.x - src_clipped_root.topLeft.x) *
	   (src_clipped_root.bottomRight.y - src_clipped_root.topLeft.y)) >=
	  ((win->size.x * win->size.y)/2) ) &&
	  (((dst_clipped_root.bottomRight.x - dst_clipped_root.topLeft.x) *
	  (dst_clipped_root.bottomRight.y - dst_clipped_root.topLeft.y)) >=
	  ((win->size.x * win->size.y)/2)) );

      DEBUG(move)
        kprintf(KR_OSTREAM, "do_optimized_copy = [%s]\n",
	        do_optimized_copy == true ? "YES" : "NO");

      if (do_optimized_copy) {

	/* Now we determine what parts of the original source are
	   currently clipped. Those regions will potentially need to be
	   redrawn once the move is completed (as opposed to asking video
	   subsystem to copy them since they're not visible!). Of course,
	   we only need to do this if the destination will be visible. */
	Window *parent = win->parent;

	/* Initialize a vector with the entire source rectangle as its
	   only element. */
	vector_append_rect(&tmp_vec, &src_rect_root, true);

	/* Now, clip that with all ancestors.  Do this with the
	   clipping package as opposed to the "rect_intersect()" call
	   because we need to know the regions that are *not* in the
	   intersections! */
	while (parent) {
	  int32_t u;

	  xform_win2rect(parent, WIN2ROOT, &parent_rect_root);
	  tmp_vec = clip(&parent_rect_root, tmp_vec);

	  /* Iterate over the result and append any "OUT" regions to
	     the hand_draw_vec list.  This is the list that will
	     ultimately need to be redrawn by hand. */
	  for (u = 0; u < tmp_vec->len; u++) {
	    if (!tmp_vec->c[u].in)
	      vector_append_rect(&hand_draw_vec, &(tmp_vec->c[u].r), true);
	  }

	  /* Remove the rects we just added to 'hand_draw_vec'. (Keep
	     the other rects in 'tmp_vec' for the next iteration.) */
	  vector_remove_rects(&tmp_vec, false);

	  /* Bump the pointer for the next iteration */
	  parent = parent->parent;
	}

	/* Any rectangles in 'hand_draw_vec' will need to be redrawn by
	   hand, because those are the offscreen ones that can't be copied
	   directly by the video subsystem. However, to make matters worse,
	   those rectangles must "follow" the window movement: they must
	   each be translated back into window coords while the window is
	   moved, then translated back into Root coords in order to be
	   redrawn! */
	{
	  int32_t n;

	  for (n = 0; n < hand_draw_vec->len; n++)
	    xform_rect(win, hand_draw_vec->c[n].r, ROOT2WIN, 
		       &(hand_draw_vec->c[n].r));
	}

	/* Now we have the non-visible parts that will have to be
           redrawn manually.  Compute the visible region that the
           video subsystem can move directly. To do this, we need to
           compare 'src_clipped_root' and 'dst_clipped_root'.  Either
           they're the same size (which is preferable!) or
           'dst_clipped_root' is smaller. If the former, then we have
           the pieces we need and just call video_copy.  If the
           latter, then we need to carefully adjust 'src_clipped_root'
           as follows: if the topLeft of the 'dst' is greater in
           either axis then adjust the topLeft of 'src' accordingly
           and add the 'adjustment' to the list of regions that need
           to be redrawn manually. If the bottomRight of the 'dst' is
           less in either axis then adjust the bottomRight of 'src'
           accordingly and add the 'adjustment' to the hand drawn
           list. FIX: It occurs to me (vandy) that all this may not be
           worth it! Someone should do some performance measures to
           determine how much value this optimization adds (other than
           just being more visually pleasing). */

	DEBUG(move)
	  {
	    kprintf(KR_OSTREAM, "src_clipped_root: [(%d,%d)(%d,%d)] "
		    "%dx%d\n",
		    src_clipped_root.topLeft.x, src_clipped_root.topLeft.y,
		    src_clipped_root.bottomRight.x, 
		    src_clipped_root.bottomRight.y,
		    src_clipped_root.bottomRight.x - 
		    src_clipped_root.topLeft.x,
		    src_clipped_root.bottomRight.y - 
		    src_clipped_root.topLeft.y);

	    kprintf(KR_OSTREAM, "dst_clipped_root: [(%d,%d)(%d,%d)] "
		    "%dx%d\n",
		    dst_clipped_root.topLeft.x, dst_clipped_root.topLeft.y,
		    dst_clipped_root.bottomRight.x, 
		    dst_clipped_root.bottomRight.y,
		    dst_clipped_root.bottomRight.x - 
		    dst_clipped_root.topLeft.x,
		    dst_clipped_root.bottomRight.y - 
		    dst_clipped_root.topLeft.y);
	  }

	video_copy_rect(src_clipped_root, dst_clipped_root);
      }
    }

    /* Unmap the necessary pieces; In the process, the newly exposed
       areas underneath are redrawn. Note: I (vandy) had this call to
       "unmap" prior to the preceding logic for the call to
       "video_copy_rect".  Not good! */
    window_unmap_pieces(win, win->parent, &exposelist);

    /* Now update thisWindow's position (now that clipping is
       complete) */
    dec->win.origin = new_origin;

    /* If the size is also changing, we need to carefully handle the
    children.  Iterate over the children, and call each child's "move"
    method ONLY IF that child is a BUTTON or CLIENT. For client
    children, this will send appropriate expose events to the client
    sessions and for non-client children (like buttons) it will
    move/resize them appropriately. NOTE: resizing this decoration's
    children is *NOT* done recursively.  Any necessary recursion must
    be handled by each window's "move" method! For example, if the
    Decoration's client child has children, it's up to the application
    to propagate resizing to those children in accordance with
    whatever resize policy it has. */
    if (changing_size) {
      Link *child;

      child = win->children.next;

      while(ADDRESS(child) != ADDRESS(&(win->children))) {
	Window *w = (Window *)child;

	if (w->type == WINTYPE_BUTTON || w->type == WINTYPE_CLIENT)
	  w->move(w, orig_delta, size_delta);

	child = child->next;
      }

      /* Now adjust this Decoration's size */
      dec->win.size.x += size_delta.x;
      dec->win.size.y += size_delta.y;

      /* Recompute its hot spots */
      compute_hot_spots(dec);
    }
  }

  /* Finally, for changing size of window, ask video subsystem to
     redraw window contents. */
  if (!do_optimized_copy || changing_size) {
    uint32_t u;
    clip_vector_t *exposed = 
      window_get_subregions(&(dec->win), UNOBSTRUCTED);

    /* Use window_draw() here as opposed to just decoration_draw() to
       ensure that children of decoration window get drawn as well! */
    for (u = 0; u < exposed->len; u++)
      window_draw(&(dec->win), exposed->c[u].r);
  }
  else if (dst_visible && hand_draw_vec && hand_draw_vec->len > 0) {
    uint32_t u;
    rect_t rect_root;

    DEBUG(move)
      kprintf(KR_OSTREAM, "*** Hand draw %d regions.\n", hand_draw_vec->len);

    for (u = 0; u < hand_draw_vec->len; u++) {
      xform_rect(win, hand_draw_vec->c[u].r, WIN2ROOT, &rect_root);
      if (rect_intersect(&parent_rect_root, &rect_root, &rect_root))
	window_draw(win, rect_root);
    }
  }
}
