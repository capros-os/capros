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

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/eros/key.h>

#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/DrawableKey.h>

#include "DrawableRequest.h"
#include "svga_reg.h"
#include "fifo.h"
#include "vmware_io.h"

#define abs(x) ((x) < 0 ? -(x) : (x))

/* globals */
rect_t clipRegion = {{0, 0}, {0,0}};
#define TEST_VAL_W 750
#define TEST_VAL_H 500
uint32_t bits[TEST_VAL_W * TEST_VAL_H];

/* These externs are declared in vmware_svga.c */
extern uint32_t *fifo;

extern uint32_t ostream_key;

/* This is the video driver's version of the server-side drawable
   implementation.  It will necessarily be different from other
   implementations! NOTE: The Drawable argument "d" is ignored here
   since the video driver acts on the entire screen. */

/* These are declared in vmware_svga.c */
extern uint32_t svga_regs[SVGA_REG_TOP];
extern uint32_t fb_size;
extern uint32_t *fifo;
extern uint32_t *framebuffer;
extern uint32_t card_functionality;


uint32_t 
drawable_RectFill(Drawable *d, rect_t rect, color_t color, uint32_t raster_op);

/* Need to validate raster operations */
#define RETURN_IF_BAD_ROP(rop) \
      if (rop < SVGA_ROP_CLEAR || rop > SVGA_ROP_SET) {  \
	return RC_eros_key_RequestError; \
      }

#ifdef USE_SET_PIXEL
#define DRAW_LINE(pt, widthx, widthy, color, raster_op) \
      if (width == 1) { \
        set_pixel(pt, color); \
      } \
      else {\
        rect_t r; \
        r.topLeft.x = pt.x; \
        r.topLeft.y = pt.y; \
        r.bottomRight.x = pt.x + widthx; \
        r.bottomRight.y = pt.y + widthy; \
        drawable_RectFill(d, r, color, raster_op); \
      } 
#else
#define DRAW_LINE(pt, widthx, widthy, color, raster_op) \
        { \
	   rect_t r; \
           r.topLeft.x = pt.x; \
           r.topLeft.y = pt.y; \
           r.bottomRight.x = pt.x + widthx; \
           r.bottomRight.y = pt.y + widthy; \
           drawable_RectFill(d, r, color, raster_op); \
        } 
#endif

static
inline
void
cacheLineZero(void *ptr)
{
  // no-op
}


static bool
clipRect(rect_t *rect, rect_t *clipRegion)
{
  rect_t out;
  int result = 0;

  if (rect == NULL || clipRegion == NULL)
    return false;

  result = rect_intersect(rect, clipRegion, &out);
  if (result)
    *rect = out;

  return (bool)result;
}

uint32_t 
set_pixel(point_t pixel, color_t color)
{
  uint32_t offset;
  uint32_t displaywidth;

  /* Clip the point to the current clipping region */
  if ( (pixel.x < clipRegion.topLeft.x) || 
       (pixel.x > clipRegion.bottomRight.x) )
    return RC_OK;

  if ( (pixel.y < clipRegion.topLeft.y) || 
       (pixel.y > clipRegion.bottomRight.y) )
    return RC_OK;

  /* Now clip it to the actual screen (since this device will do
     "wrap-around" on pixels if we don't clip them) */
  if (pixel.x > svga_regs[SVGA_REG_WIDTH] ||
      pixel.y > svga_regs[SVGA_REG_HEIGHT])
    return RC_OK;

  displaywidth = (svga_regs[SVGA_REG_BYTES_PER_LINE] * 8) / 
    ((svga_regs[SVGA_REG_BITS_PER_PIXEL] + 7) & ~7);

/* This line has NOT BEEN TESTED WITH ANY DEPTH < 32 BITS!! */ 
  offset = pixel.x + displaywidth * pixel.y;  

  /* FIX: Can the following occur if the clipping above is correct? */
  if (offset >= fb_size)
    return RC_eros_key_RequestError;

  if (svga_regs[SVGA_REG_BITS_PER_PIXEL] == 16) {
    uint16_t converted = makeColor16(color,
					svga_regs[SVGA_REG_RED_MASK],
					svga_regs[SVGA_REG_GREEN_MASK],
					svga_regs[SVGA_REG_BLUE_MASK]);

    ((uint16_t *)framebuffer)[offset] =  converted;
  }
  else
    ((uint32_t *)framebuffer)[offset] = color;

  return RC_OK;
}

uint32_t 
drawable_SetClipRegion(Drawable *d, rect_t clipRect)
{
  clipRegion = clipRect;

  return RC_OK;
}

uint32_t 
drawable_SetPixel(Drawable *d, point_t pixel, color_t color)
{
  return set_pixel(pixel, color);
}

enum {TOP = 0x1, BOTTOM = 0x2, RIGHT = 0x4, LEFT = 0x8};

static uint32_t
lineClipOutCode (int32_t x, int32_t y, rect_t r)
{
  uint32_t c = 0;

  if (y > r.bottomRight.y)  c |= TOP;
  else if (y < r.topLeft.y) c |= BOTTOM;

  if (x > r.bottomRight.x)  c |= RIGHT;
  else if (x < r.topLeft.x) c |= LEFT;

  return c;
}

static uint32_t
lineClip (line_t *l, rect_t clip)
{
  uint32_t out0, out1, code;
  int32_t x, y;
	
  out0 = lineClipOutCode (l->pt[0].x, l->pt[0].y, clip);
  out1 = lineClipOutCode (l->pt[1].x, l->pt[1].y, clip);
	
  for (;;) {
    if ((out0 | out1) == 0) return 1;
    if ((out0 & out1) != 0) return 0;
		
    code = out0 ? out0 : out1;
    if (code & TOP) {
      x = l->pt[0].x + (l->pt[1].x - l->pt[0].x) * (clip.bottomRight.y - l->pt[0].y) / (l->pt[1].y - l->pt[0].y);
      y = clip.bottomRight.y;

    } else if (code & BOTTOM) {
      x = l->pt[0].x + (l->pt[1].x - l->pt[0].x) * (clip.topLeft.y - l->pt[0].y)  / (l->pt[1].y - l->pt[0].y);
      y = clip.topLeft.y;

    } else if (code & RIGHT) {
      x = clip.bottomRight.x;
      y = l->pt[0].y + (l->pt[1].y - l->pt[0].y) * (clip.bottomRight.x - l->pt[0].x) / (l->pt[1].x - l->pt[0].x);

    } else {
      x = clip.topLeft.x;
      y = l->pt[0].y + (l->pt[1].y - l->pt[0].y) * (clip.topLeft.x - l->pt[0].x) / (l->pt[1].x - l->pt[0].x);
    }
		
    if (code == out0) {
      l->pt[0].x = x; l->pt[0].y = y;
      out0 = lineClipOutCode (l->pt[0].x, l->pt[0].y, clip);
    } else {
      l->pt[1].x = x; l->pt[1].y = y;
      out1 = lineClipOutCode (l->pt[1].x, l->pt[1].y, clip);
    }
  }
}


static uint32_t
doDrawableLineDraw(Drawable *d, line_t line, uint32_t width, color_t color, uint32_t raster_op)
{
  int dx, dy, delta, yincr, xincr;  
  rect_t new_clipRegion;

  new_clipRegion.topLeft.x = clipRegion.topLeft.x - width + 1;
  new_clipRegion.topLeft.y = clipRegion.topLeft.y - width + 1;
  new_clipRegion.bottomRight.x = clipRegion.bottomRight.x + width - 1;
  new_clipRegion.bottomRight.y = clipRegion.bottomRight.y + width - 1;

  if (lineClip( &line, new_clipRegion) == 0) 
    return RC_OK;
  
  if (abs(line.pt[1].x - line.pt[0].x) >= 
      abs(line.pt[1].y - line.pt[0].y)) {\
    dx = line.pt[1].x - line.pt[0].x;
    dy = line.pt[1].y - line.pt[0].y;
    if (dy > 0)
      yincr = 1;
    else
      yincr = -1;
    dy = abs(dy);
    delta = (dy << 1) - dx;
    for ( ; line.pt[0].x <= line.pt[1].x; ++line.pt[0].x) {
      DRAW_LINE( line.pt[0], 1, width, color, raster_op);
      if (delta > 0) {
	line.pt[0].y += yincr;
	delta -= dx << 1;
      }
      delta += dy << 1;
    }
  }
  else {
    dx = line.pt[1].x - line.pt[0].x;
    dy = line.pt[1].y - line.pt[0].y;

    if (dx > 0)  xincr = 1;
    else         xincr = -1;
    dx = abs(dx);
    delta = (dx << 1) - dy;

    for ( ; line.pt[0].y <= line.pt[1].y; ++line.pt[0].y){
      DRAW_LINE( line.pt[0], width, 1, color, raster_op );
      if (delta > 0) {
	line.pt[0].x += xincr;
	delta -= dy << 1;
      }
      delta += dx << 1;
    }
  }

  return RC_OK;
}

uint32_t
drawable_LineDraw(Drawable *d, line_t line, uint32_t width, color_t color, uint32_t raster_op)
{
  int start;

  if (width & 1)
    start = width >> 1;
  else
    start = (width >> 1) - 1;

  if (abs(line.pt[1].x - line.pt[0].x) >= 
      abs(line.pt[1].y - line.pt[0].y)) {

    if (line.pt[0].x == line.pt[1].x) {
      rect_t r;    
      r.topLeft.x = line.pt[0].x - start;
      r.topLeft.y = (line.pt[0].y < line.pt[1].y)     ? line.pt[0].y : line.pt[1].y;

      r.bottomRight.x = line.pt[1].x + (width >> 1) + 1;
      r.bottomRight.y = (line.pt[0].y < line.pt[1].y) ? line.pt[1].y+1 : line.pt[0].y+1;
    
      return drawable_RectFill(d, r, color, raster_op);
    }

  if (line.pt[0].x > line.pt[1].x) {
      uint32_t tmp;
      tmp = line.pt[0].x; line.pt[0].x = line.pt[1].x;  line.pt[1].x = tmp;
      tmp = line.pt[0].y; line.pt[0].y = line.pt[1].y;  line.pt[1].y = tmp;
  }

    line.pt[0].y -= start;
    line.pt[1].y -= start;
   
  }
  else {

    if (line.pt[0].y == line.pt[1].y) {
      rect_t r;
      r.topLeft.x = (line.pt[0].x < line.pt[1].x) ? line.pt[0].x : line.pt[1].x;
      r.topLeft.y = line.pt[0].y - start;
      r.bottomRight.x = (line.pt[0].x < line.pt[1].x) ? line.pt[1].x+1 : line.pt[0].x+1;
      r.bottomRight.y = line.pt[1].y + (width >> 1) + 1;
	  
      return drawable_RectFill(d, r, color, raster_op);
    }    
    else {
      if (line.pt[0].y > line.pt[1].y) {
	uint32_t tmp;
	tmp = line.pt[0].x; line.pt[0].x = line.pt[1].x; line.pt[1].x = tmp;
	tmp = line.pt[0].y; line.pt[0].y = line.pt[1].y; line.pt[1].y = tmp;
      }
      line.pt[0].x -= start;
      line.pt[1].x -= start;
    }
  }
  
  return doDrawableLineDraw(d, line, width, color, raster_op);
}



uint32_t 
drawable_RectFill(Drawable *d, rect_t rect, color_t color, uint32_t raster_op)
{
  rect_t screenRect = { {0,0}, 
    			{svga_regs[SVGA_REG_WIDTH], 
    			 svga_regs[SVGA_REG_HEIGHT]}};

  RETURN_IF_BAD_ROP(raster_op);

  /* Clip against clipRegion and then against the screen */
  if (clipRect(&rect, &clipRegion)) {
      if (clipRect(&rect, &screenRect)) {

      /* If this card doesn't have an accelerated version of rectfill we
	 have to do it by hand */
	if ((card_functionality & SVGA_CAP_RECT_FILL) == 0) {

	uint32_t x;
	uint32_t y;
	point_t pt;

	/* Wait on any outstanding FIFO commands to complete */
	fifo_sync(fifo);

	/* Remove the cursor before modifying the framebuffer */
	VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_REMOVE_FROM_FB);

	/* Set each pixel by hand */
	for (x = rect.topLeft.x; x < rect.bottomRight.x; x++)
	  for (y = rect.topLeft.y; y < rect.bottomRight.y; y++) {
	    pt.x = x;
	    pt.y = y;
	    set_pixel(pt, color);
	  }

	/* Restore the cursor after modifying the framebuffer */
	VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_RESTORE_TO_FB);

	/* Then redraw the screen */
	fifo_insert(fifo,SVGA_CMD_UPDATE);
	fifo_insert(fifo, rect.topLeft.x);
	fifo_insert(fifo, rect.topLeft.y);
	fifo_insert(fifo, rect.bottomRight.x);
	fifo_insert(fifo, rect.bottomRight.y);
      }
      else {

	fifo_insert(fifo,SVGA_CMD_RECT_ROP_FILL);

	if (svga_regs[SVGA_REG_BITS_PER_PIXEL] == 16) {
	  uint16_t converted = makeColor16(color,
					svga_regs[SVGA_REG_RED_MASK],
					svga_regs[SVGA_REG_GREEN_MASK],
					svga_regs[SVGA_REG_BLUE_MASK]);

	  fifo_insert(fifo, converted);
	}
	else
	  fifo_insert(fifo,color);

	fifo_insert(fifo,rect.topLeft.x);
	fifo_insert(fifo,rect.topLeft.y);
	fifo_insert(fifo,rect.bottomRight.x-rect.topLeft.x);
	fifo_insert(fifo,rect.bottomRight.y-rect.topLeft.y);
	fifo_insert(fifo,raster_op);
      }
    }
  }
  return RC_OK;
}

uint32_t 
drawable_RectFillBorder( Drawable *d, rect_t rect, color_t color, color_t border_color, uint32_t raster_op)
{
  line_t line;

  drawable_RectFill(d, rect, color, raster_op);

  line.pt[0]   = rect.topLeft;
  line.pt[1].x = rect.bottomRight.x;
  line.pt[1].y = rect.topLeft.y;
  drawable_LineDraw(d, line, 1, border_color, raster_op);

  line.pt[0] = rect.bottomRight;
  drawable_LineDraw(d, line, 1, border_color, raster_op);

  line.pt[1].x = rect.topLeft.x;
  line.pt[1].y = rect.bottomRight.y;
  drawable_LineDraw(d, line, 1, border_color, raster_op);

  line.pt[0] = rect.topLeft;
  drawable_LineDraw(d, line, 1, border_color, raster_op);

  return RC_OK;
}



uint32_t
drawable_Redraw(Drawable *d, rect_t area)
{
  rect_t screenRect = { {0,0}, 
			{svga_regs[SVGA_REG_WIDTH], 
			 svga_regs[SVGA_REG_HEIGHT]}};

  /* Clip the requested update region to the clipping region of the
     Drawable first. */
  if (clipRect(&area, &clipRegion))

    /* Then clip it to the screen */
    if (clipRect(&area, &screenRect)) {

      /* Request an update on anything's that left */
      fifo_insert(fifo,SVGA_CMD_UPDATE);
      fifo_insert(fifo, area.topLeft.x);
      fifo_insert(fifo, area.topLeft.y);
      fifo_insert(fifo, (area.bottomRight.x-area.topLeft.x));
      fifo_insert(fifo, (area.bottomRight.y-area.topLeft.y));
    }
  return RC_OK;
}

uint32_t
drawable_BitBlt(Drawable *d, rect_t area, uint32_t *data)
{
  uint32_t x, y;
  uint32_t count = 0;
  point_t pt;

  /* Wait on any outstanding FIFO commands to complete */
  fifo_sync(fifo);

  /* Remove the cursor before modifying the framebuffer */
  VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_REMOVE_FROM_FB);

  for (y = area.topLeft.y; y < area.bottomRight.y; y++) {
    for (x = area.topLeft.x; x < area.bottomRight.x; x++) {
      pt.x = x;
      pt.y = y;
      set_pixel(pt, data[count]);
      count++;
    }
  }

  /* Restore the cursor after modifying the framebuffer */
  VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_RESTORE_TO_FB);

  /* Request an update */
  fifo_insert(fifo, SVGA_CMD_UPDATE);
  fifo_insert(fifo, area.topLeft.x);
  fifo_insert(fifo, area.topLeft.y);
  fifo_insert(fifo, area.bottomRight.x);
  fifo_insert(fifo, area.bottomRight.y);

  return RC_OK;
}

/* Temporary: */
uint32_t
drawable_BigBitBltTest(Drawable *d)
{
  uint32_t x, y;
  uint32_t color = 0;
  uint32_t count = 0;
  rect_t area = { {0, 0}, {TEST_VAL_W, TEST_VAL_H} };

  for (y = 0; y < TEST_VAL_H; y++)
    for (x = 0; x < TEST_VAL_W; x++) {
      bits[count++] = color;
      color += (x*y);
    }

  drawable_BitBlt(d, area, bits);

  return RC_OK;
}

uint32_t 
drawable_Clear(Drawable *d, color_t color)
{
  if (card_functionality & SVGA_CAP_RECT_FILL) {

    fifo_insert(fifo, SVGA_CMD_RECT_FILL);

    if (svga_regs[SVGA_REG_BITS_PER_PIXEL] == 16)
      fifo_insert(fifo, makeColor16(color, 
				    svga_regs[SVGA_REG_RED_MASK],
				    svga_regs[SVGA_REG_GREEN_MASK],
				    svga_regs[SVGA_REG_BLUE_MASK]));

    else 
      fifo_insert(fifo, color);

    fifo_insert(fifo, 0);
    fifo_insert(fifo, 0);
    fifo_insert(fifo, svga_regs[SVGA_REG_WIDTH]);
    fifo_insert(fifo, svga_regs[SVGA_REG_HEIGHT]);

  }
  else {
    uint32_t x;
    uint32_t y;
    point_t pt;

    /* Wait on any outstanding FIFO commands to complete */
    fifo_sync(fifo);

    /* Remove the cursor before modifying the framebuffer */
    VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_REMOVE_FROM_FB);

    /* Set each pixel by hand */
    for (x = 0; x < svga_regs[SVGA_REG_WIDTH]; x++)
      for (y = 0; y < svga_regs[SVGA_REG_HEIGHT]; y++) {
	pt.x = x;
	pt.y = y;
	set_pixel(pt, color);
      }

    /* Then redraw the screen */
      /* Request an update on anything's that left */
      fifo_insert(fifo,SVGA_CMD_UPDATE);
      fifo_insert(fifo, 0);
      fifo_insert(fifo, 0);
      fifo_insert(fifo, svga_regs[SVGA_REG_WIDTH]);
      fifo_insert(fifo, svga_regs[SVGA_REG_HEIGHT]);
  }
  return RC_OK;
}

uint32_t
drawable_TriDraw(Drawable *d, point_t pt[], bool brd[], 
		 color_t color, uint32_t raster_op)
{

  line_t l;
  rect_t r = { {0, 0}, {100, 100}};

  drawable_RectFill(d, r, 0, ROP_COPY);


  if (brd[0]) {
    l.pt[0] = pt[0];
    l.pt[1] = pt[1];
    drawable_LineDraw(d, l, 1, color, raster_op);
  }
  
  if (brd[1]) {
    l.pt[0] = pt[1];
    l.pt[1] = pt[2];
    drawable_LineDraw(d, l, 1, color, raster_op);
  }
  
  if (brd[2]) {
    l.pt[0] = pt[2];
    l.pt[1] = pt[0];
    drawable_LineDraw(d, l , 1, color, raster_op);
  }
  return RC_OK;
}

static
inline
uint32_t
areaTri(point_t a, point_t b, point_t c)
{
  int32_t area;

  area = a.x * ( b.y - c.y ) - b.x * ( a.y - c.y )
       + c.x * ( a.y - b.y );

  return abs(area);
}

static
bool
insideTri(point_t *pt, point_t a)
{
  if ((areaTri( pt[0], pt[1], a ) + 
       areaTri( pt[0], pt[2], a ) +
       areaTri( pt[1], pt[2], a )) <=
      areaTri( pt[0], pt[1], pt[2] ))
    return true;
  else
    return false;
}

static
uint32_t
doTriFill(Drawable *d, point_t *pt, color_t color, 
	  uint32_t raster_op, rect_t r, uint32_t depth )
{
  point_t a_pt;
  rect_t s;

  int inside = 0;

  for (a_pt.y = r.topLeft.y; a_pt.y < r.bottomRight.y; a_pt.y++) {
    inside = 0;
    for (a_pt.x = r.topLeft.x; a_pt.x < r.bottomRight.x; a_pt.x++) {
      
      if (insideTri(pt, a_pt)){
	if(!inside) {
	  s.topLeft=a_pt;
	  inside = 1;
        }
      }
      else if (inside) {
	break;
      }
    }
    s.bottomRight = a_pt;
    s.bottomRight.y++;
    drawable_RectFill(d, s, color, raster_op);
  }

  return RC_OK;
}

uint32_t
drawable_TriFill(Drawable *d, point_t *pt,
		 color_t color, uint32_t raster_op)
{

  rect_t r;

  r.topLeft.x = ( pt[0].x < pt[1].x ) ? pt[0].x : pt[1].x;
  if ( r.topLeft.x > pt[2].x )
    r.topLeft.x = pt[2].x;

  r.topLeft.y = ( pt[0].y < pt[1].y ) ? pt[0].y : pt[1].y;
  if ( r.topLeft.y > pt[2].y )
    r.topLeft.y = pt[2].y;

  r.bottomRight.x = ( pt[0].x > pt[1].x ) ? pt[0].x : pt[1].x;
  if ( r.bottomRight.x < pt[2].x )
    r.bottomRight.x = pt[2].x;

  r.bottomRight.y = ( pt[0].y > pt[1].y ) ? pt[0].y : pt[1].y;
  if ( r.bottomRight.y < pt[2].y )
    r.bottomRight.y = pt[2].y;

  return doTriFill(d, pt, color, raster_op, r, 0);

  
}
