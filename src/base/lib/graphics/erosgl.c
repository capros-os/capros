/*
 * Copyright (C) 2003, Jonathan S. Shapiro.
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library,
 * and is derived from the EROS Operating System runtime library.
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

/* Simple graphics library */
#include <eros/target.h>
#include <stdlib.h>
#include <string.h>

#include "erosgl.h"

#undef DEBUG
#ifdef DEBUG
#include <domain/domdbg.h>
extern cap_t ostream;
#endif

#define xmalloc(size) malloc(size)
#define xfree(p) (void)0

#ifndef min
 #define min(a,b) ((a) <= (b) ? (a) : (b))
#endif

#ifndef max
 #define max(a,b) ((a) >= (b) ? (a) : (b))
#endif

#define DRAW_LINE(gc, pt, widthx, widthy) \
      if (width == 1) { \
        set_pixel(gc, pt); \
      } \
      else {\
        rect_t r; \
        r.topLeft.x = pt.x; \
        r.topLeft.y = pt.y; \
        r.bottomRight.x = pt.x + widthx; \
        r.bottomRight.y = pt.y + widthy; \
        erosgl_rectfill(gc, r); \
      } 

enum {TOP = 0x1, BOTTOM = 0x2, RIGHT = 0x4, LEFT = 0x8};

static void
update_damage(GLContext *gc, rect_t rect)
{
  rect_t result;

  if (gc == NULL)
    return;

  rect_union(&(gc->cumulative_damage_area), &rect, &result);
  gc->cumulative_damage_area = result;

#ifdef DEBUG
  kprintf(ostream, "EROSGL: updating damage: in rect is [(%d,%d)(%d,%d)]\n"
	  "          and result is [(%d,%d)(%d,%d)]\n",
	  rect.topLeft.x, rect.topLeft.y, rect.bottomRight.x, 
	  rect.bottomRight.y,
	  result.topLeft.x, result.topLeft.y, result.bottomRight.x,
	  result.bottomRight.y);
#endif

}

/* Returns *twice* the area of a triangle. */
static inline
uint32_t
areaTri(point_t a, point_t b, point_t c)
{
  int32_t area;

  area = a.x * ( b.y - c.y ) - b.x * ( a.y - c.y )
       + c.x * ( a.y - b.y );

  return abs(area);
}

static bool
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

static void 
set_pixel(GLContext *gc, point_t pixel)
{
  uint32_t offset;
  rect_t clipRegion;

  if (gc == NULL)
    return;

  clipRegion = gc->clip_region;

  /* Clip the point to the current clipping region */
  if ( (pixel.x < clipRegion.topLeft.x) || 
       (pixel.x > clipRegion.bottomRight.x) )
    return;

  if ( (pixel.y < clipRegion.topLeft.y) || 
       (pixel.y > clipRegion.bottomRight.y) )
    return;

  offset = pixel.x + gc->dimensions.x * pixel.y;  

  ((color_t *)gc->base)[offset] = gc->color;
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
lineClip(line_t *l, rect_t clip)
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

static void
doLineDraw(GLContext *gc, line_t line)
{
  uint32_t width;
  int dx, dy, delta, yincr, xincr;  
  rect_t clipRegion;
  rect_t new_clipRegion;
  rect_t damage;

  if (gc == NULL)
    return;

  clipRegion = gc->clip_region;
  width = gc->line_width;
  /* The following variables were originally fed to DRAW_LINE, which ignored them.
   * if we get to fix this code, we'll need to consider this carefully.
   *
   * color = gc->color;
   * rop = gc->raster_op;
   */

#ifdef DEBUG
  kprintf(ostream, "EROSGL: doLineDraw w/line (%d,%d) to (%d,%d)\n"
	  "        and clipping [(%d,%d)(%d,%d)]\n",
	  line.pt[0].x, line.pt[0].y, line.pt[1].x, line.pt[1].y,
	  clipRegion.topLeft.x, clipRegion.topLeft.y,
	  clipRegion.bottomRight.x, clipRegion.bottomRight.y);
#endif

  new_clipRegion.topLeft.x = clipRegion.topLeft.x - width + 1;
  new_clipRegion.topLeft.y = clipRegion.topLeft.y - width + 1;
  new_clipRegion.bottomRight.x = clipRegion.bottomRight.x + width - 1;
  new_clipRegion.bottomRight.y = clipRegion.bottomRight.y + width - 1;

  if (lineClip( &line, new_clipRegion) == 0) 
    return;

#ifdef DEBUG
  kprintf(ostream, "EROSGL: doLineDraw: line not clipped!\n");
#endif

  /* Determine damaged area */
  damage.topLeft.x = min(line.pt[0].x, line.pt[1].x);
  damage.topLeft.y = min(line.pt[0].y, line.pt[1].y);
  damage.bottomRight.x = max(line.pt[0].x, line.pt[1].x);
  damage.bottomRight.y = max(line.pt[0].y, line.pt[1].y);

  if (abs(line.pt[1].x - line.pt[0].x) >= 
      abs(line.pt[1].y - line.pt[0].y)) {
    dx = line.pt[1].x - line.pt[0].x;
    dy = line.pt[1].y - line.pt[0].y;
    if (dy > 0)
      yincr = 1;
    else
      yincr = -1;
    dy = abs(dy);
    delta = (dy << 1) - dx;
    for ( ; line.pt[0].x <= line.pt[1].x; ++line.pt[0].x) {
      DRAW_LINE(gc, line.pt[0], 1, width);
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
      DRAW_LINE(gc, line.pt[0], width, 1);
      if (delta > 0) {
	line.pt[0].x += xincr;
	delta -= dy << 1;
      }
      delta += dx << 1;
    }
  }
  update_damage(gc, damage);
}

/* Fill the intersection of triangle pt and rectangle r. */
static void
doTriFill(GLContext *gc, point_t *pt, rect_t r, uint32_t depth)
{
#if 0	// this is broken!
  point_t a_pt;
  rect_t s;

  int inside = 0;

  if (gc == NULL)
    return;

  if (pt == NULL)
    return;

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
    erosgl_rectfill(gc, s);
  }
#else
  insideTri(pt, pt[0]);// avoid a compiler warning
#endif
}

/* User must have a graphics context to do erosgl primitives.  Pass in
   the base address of the area of memory onto which drawing will occur. */
GLContext 
*erosgl_new_context(uint32_t *base, UpdateFn update)
{
  GLContext *gc;

  if (update == NULL)
    return NULL;

  gc = (GLContext *)xmalloc(sizeof(GLContext));

  memset(gc, 0, sizeof(GLContext));

  gc->base = base;
  gc->update = update;

  return gc;
}

void 
erosgl_free_context(GLContext *gc)
{
  xfree(gc);
}

void 
erosgl_gc_set_color(GLContext *gc, color_t color)
{
  if (gc == NULL)
    return;

  gc->color = color;
}

void 
erosgl_gc_set_line_width(GLContext *gc, uint32_t width)
{
  if (gc == NULL)
    return;

  gc->line_width = width;
}

void 
erosgl_gc_set_dimensions(GLContext *gc, int32_t x, int32_t y)
{
  if (gc == NULL)
    return;

  gc->dimensions.x = x;
  gc->dimensions.y = y;
}

void 
erosgl_gc_set_clipping(GLContext *gc, rect_t clip)
{
  rect_t r = { {0,0}, gc->dimensions };

  if (gc == NULL)
    return;

  /* Clip area can't be any larger than the graphics context size */
  if (clipRect(&clip, &r))
    gc->clip_region = clip;
}

void 
erosgl_gc_set_raster_op(GLContext *gc, uint32_t rop)
{
  if (gc == NULL)
    return;

  gc->raster_op = rop;
}

void 
erosgl_line(GLContext *gc, line_t line)
{
  int start;
  uint32_t width;

  if (gc == NULL)
    return;

#ifdef DEBUG
  kprintf(ostream, "EROSGL: erosgl_line.\n");
#endif

  width = gc->line_width;

  if (width & 1)
    start = width >> 1;
  else
    start = (width >> 1) - 1;

  if (abs(line.pt[1].x - line.pt[0].x) >= 
      abs(line.pt[1].y - line.pt[0].y)) {

    if (line.pt[0].x == line.pt[1].x) {
      rect_t r;    

      r.topLeft.x = line.pt[0].x - start;
      r.topLeft.y = (line.pt[0].y < line.pt[1].y) ? 
	line.pt[0].y : line.pt[1].y;

      r.bottomRight.x = line.pt[1].x + (width >> 1) + 1;
      r.bottomRight.y = (line.pt[0].y < line.pt[1].y) ? 
	line.pt[1].y+1 : line.pt[0].y+1;
    
      erosgl_rectfill(gc, r);
      return;
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

      r.topLeft.x = (line.pt[0].x < line.pt[1].x) ? 
	line.pt[0].x : line.pt[1].x;

      r.topLeft.y = line.pt[0].y - start;
      r.bottomRight.x = (line.pt[0].x < line.pt[1].x) ? 
	line.pt[1].x+1 : line.pt[0].x+1;

      r.bottomRight.y = line.pt[1].y + (width >> 1) + 1;
	  
      erosgl_rectfill(gc, r);
      return;
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
  
  doLineDraw(gc, line);
}

void 
erosgl_rectfill(GLContext *gc, rect_t rect)
{
  rect_t screenRect; 

  if (gc == NULL)
    return;

#ifdef DEBUG
  kprintf(ostream, "EROSGL: erosgl_rectfill.\n");
#endif

  screenRect.topLeft.x = 0;
  screenRect.topLeft.y = 0;
  screenRect.bottomRight = gc->dimensions;

  /* Clip against clipRegion and then against the screen */
  if (clipRect(&rect, &(gc->clip_region))) {
    if (clipRect(&rect, &screenRect)) {

      uint32_t x;
      uint32_t y;
      point_t pt;

      /* Set each pixel by hand */
      for (x = rect.topLeft.x; x < rect.bottomRight.x; x++)
	for (y = rect.topLeft.y; y < rect.bottomRight.y; y++) {
	  pt.x = x;
	  pt.y = y;
	  set_pixel(gc, pt);
	}

      /* Finally, update the damaged region */
      update_damage(gc, rect);
    }
  }
}

void 
erosgl_rectfillborder(GLContext *gc, rect_t rect, color_t border)
{
  line_t line;
  color_t save_color;

  if (gc == NULL)
    return;

  save_color = gc->color;

  erosgl_rectfill(gc, rect);

  erosgl_gc_set_color(gc, border);

  line.pt[0]   = rect.topLeft;
  line.pt[1].x = rect.bottomRight.x;
  line.pt[1].y = rect.topLeft.y;
  erosgl_line(gc, line);

  line.pt[0] = rect.bottomRight;
  erosgl_line(gc, line);

  line.pt[1].x = rect.topLeft.x;
  line.pt[1].y = rect.bottomRight.y;
  erosgl_line(gc, line);

  line.pt[0] = rect.topLeft;
  erosgl_line(gc, line);

  erosgl_gc_set_color(gc, save_color);
}

void 
erosgl_trifill(GLContext *gc, point_t pt1, point_t pt2, point_t pt3)
{
  rect_t r;
  point_t points[3] = { pt1, pt2, pt3 };

  if (gc == NULL)
    return;

  r.topLeft.x = ( points[0].x < points[1].x ) ? points[0].x : points[1].x;
  if ( r.topLeft.x > points[2].x )
    r.topLeft.x = points[2].x;

  r.topLeft.y = ( points[0].y < points[1].y ) ? points[0].y : points[1].y;
  if ( r.topLeft.y > points[2].y )
    r.topLeft.y = points[2].y;

  r.bottomRight.x = ( points[0].x > points[1].x ) ? points[0].x : points[1].x;
  if ( r.bottomRight.x < points[2].x )
    r.bottomRight.x = points[2].x;

  r.bottomRight.y = ( points[0].y > points[1].y ) ? points[0].y : points[1].y;
  if ( r.bottomRight.y < points[2].y )
    r.bottomRight.y = points[2].y;

  doTriFill(gc, points, r, 0);
}

void
erosgl_clear(GLContext *gc, color_t color)
{
  uint32_t x, y;
  rect_t damage = { {0,0}, gc->dimensions};

  if (gc == NULL)
    return;

  for (x = 0; x < gc->dimensions.x; x++)
    for (y = 0; y < gc->dimensions.y; y++)
      gc->base[x + gc->dimensions.x * y] = color;

  update_damage(gc, damage);
}

void
erosgl_update(GLContext *gc)
{
  gc->update(gc->cumulative_damage_area);
}
