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

/* The "DrawableRequest()" method is this driver's implementation of
   the Drawable interface. */

#include <stddef.h>
#include <eros/target.h>
#include <eros/Invoke.h>

#include <idl/capros/key.h>

#include <domain/DrawableKey.h>
#include <domain/domdbg.h>

#include <stdlib.h>

#include "DrawableRequest.h"
#include "debug.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

/* FIX: Once we have a Drawable struct, change this: */
#define drawable_from_message(m) NULL

/* Receive buffer for invocations */
extern uint32_t receive_buffer[];

bool
DrawableRequest(uint32_t consoleKey, Message *msg)
{
  /* Use these two variables to ensure that the message wasn't
     truncated! */
  uint32_t expect;
  uint32_t got;
  Drawable *d = drawable_from_message(msg);

  switch (msg->rcv_code) {

  case OC_Drawable_SetClipRegion:
    {
      rect_t clipRect = { {receive_buffer[0], receive_buffer[1]},
			  {receive_buffer[2], receive_buffer[3]} };

      expect = 4 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_capros_key_RequestError;
	return true;
      }

      msg->snd_code = drawable_SetClipRegion(d, clipRect);
    }
    break;

  case OC_Drawable_SetPixel:
    {
      point_t pt = {receive_buffer[0], receive_buffer[1]};
      uint32_t color = receive_buffer[2];

      expect = 3 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_capros_key_RequestError;
	return true;
      }

      msg->snd_code = drawable_SetPixel(d, pt, color);
    }
    break;

  case OC_Drawable_LineDraw:
    {
      line_t line;
      uint32_t width = receive_buffer[4];
      uint32_t color = receive_buffer[5];
      uint32_t raster_op = receive_buffer[6];

      line.pt[0].x = receive_buffer[0];
      line.pt[0].y = receive_buffer[1];
      line.pt[1].x = receive_buffer[2];
      line.pt[1].y = receive_buffer[3];

      expect = 7 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_capros_key_RequestError;
	return true;
      }

      DEBUG(drawable_cmds) kprintf(consoleKey, "DrawableRequest: calling "
				   "drawable_LineDraw() with line = [(%u,%u),"
				   " (%u, %u)], color = 0x%08x and width=0x%08x\n",
				   line.pt[0].x, line.pt[0].y,
				   line.pt[1].x, line.pt[1].y,
				   color, width);
      msg->snd_code = drawable_LineDraw(d, line, width, color, raster_op);
    }
    break;

  case OC_Drawable_RectFill:
    {
      rect_t rect = { {receive_buffer[0], receive_buffer[1]},
		      {receive_buffer[2], receive_buffer[3]} };
      uint32_t color = receive_buffer[4];
      uint32_t raster_op = receive_buffer[5];

      expect = 6 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_capros_key_RequestError;
	return true;
      }

      DEBUG(drawable_cmds) kprintf(consoleKey, "DrawableRequest: calling "
				   "drawable_RectFill() with rect = [(%u,%u),"
				   " (%u, %u)] and color = 0x%08x",
				   rect.topLeft.x, rect.topLeft.y,
				   rect.bottomRight.x, rect.bottomRight.y,
				   color);

      msg->snd_code = drawable_RectFill(d, rect, color, raster_op);
    }
    break;

  case OC_Drawable_RectFillBorder:
    {
      rect_t rect = { {receive_buffer[0], receive_buffer[1]},
		      {receive_buffer[2], receive_buffer[3]} };
      uint32_t color = receive_buffer[4];
      uint32_t border_color = receive_buffer[5];
      uint32_t raster_op = receive_buffer[6];

      expect = 7 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_capros_key_RequestError;
	return true;
      }

      DEBUG(drawable_cmds) kprintf(consoleKey, "DrawableRequest: calling "
				   "drawable_RectFillBorder() with rect = [(%u,%u),"
				   " (%u, %u)] and color = 0x%08x,"
				   " color_border = 0x%08x",
				   rect.topLeft.x, rect.topLeft.y,
				   rect.bottomRight.x, rect.bottomRight.y,
				   color, border_color);

      msg->snd_code = drawable_RectFillBorder(d, rect, color, border_color, raster_op);
    }
    break;

  case OC_Drawable_TriDraw:
    {
       point_t pt[] = { { receive_buffer[0], receive_buffer[1] },
                        { receive_buffer[2], receive_buffer[3] },
			{ receive_buffer[4], receive_buffer[5] } };

       bool brd[] = { receive_buffer[6], 
		      receive_buffer[7],
		      receive_buffer[8] };

       uint32_t color = receive_buffer[9];

       uint32_t raster_op = receive_buffer[10];
       
       expect = 11 * sizeof(uint32_t);
       got = min(msg->rcv_limit, msg->rcv_sent);
       
       if (expect != got) {
	   DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				    "expect = %u and got = %u.\n", expect, got);
	   msg->snd_code = RC_capros_key_RequestError;
	   return true;
       }

       DEBUG(drawable_cmds) kprintf(consoleKey, "DrawableRequest: calling "
				    "drawable_TriDraw() with tri = [(%u,%u),"
				    " (%u, %u), (%u, %u)], border = [ %u, %u, %u ]"
				    " and color = 0x%08x",
				    pt[0].x, pt[0].y, pt[1].x, pt[1].y,
				    pt[2].x, pt[2].y, brd[0], brd[1], brd[2],
				    color);

       msg->snd_code = drawable_TriDraw(d, pt, brd, color, raster_op);
      }
      break;
      
  case OC_Drawable_TriFill:
      {
        point_t pt[] = { { receive_buffer[0], receive_buffer[1] },
                         { receive_buffer[2], receive_buffer[3] },
	 		 { receive_buffer[4], receive_buffer[5] } };

	uint32_t color = receive_buffer[6];
	
	uint32_t raster_op = receive_buffer[7];
	
	expect = 8 * sizeof(uint32_t);
	got = min(msg->rcv_limit, msg->rcv_sent);
	
	//	if (expect != got) {
	//	    DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
	//				     "expect = %u and got = %u.\n", expect, got);
	//	    msg->snd_code = RC_capros_key_RequestError;
	//	    return true;
	//	}
	
	DEBUG(drawable_cmds) kprintf(consoleKey, "DrawableRequest: calling "
				     "drawable_TriFill() with tri = [(%u,%u),"
				     " (%u, %u), (%u, %u)]"
				     " and color = 0x%08x",
				     pt[0].x, pt[0].y, pt[1].x, pt[1].y,
				     pt[2].x, pt[2].y, color);
	
	msg->snd_code = drawable_TriFill(d, pt, color, raster_op);
      }
      break;
	
  case OC_Drawable_Redraw:
    {
      rect_t rect = { {receive_buffer[0], receive_buffer[1]},
		      {receive_buffer[2], receive_buffer[3]} };

      expect = 4 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_capros_key_RequestError;
	return true;
      }

      msg->snd_code = drawable_Redraw(d, rect);
    }
    break;

  case OC_Drawable_Clear:
    {
      msg->snd_code = drawable_Clear(d, msg->rcv_w3);
    }
    break;

  case OC_Drawable_BitBlt:
    {
      uint32_t total_pixels = receive_buffer[3];
      uint32_t height;
      uint32_t width;
      rect_t rect;

#ifdef ALWAYS_USE_RECTFILL
      uint32_t x = 0;
      uint32_t y = 0;
      uint32_t count = 0;
      point_t pt, size;
      uint32_t current_val = 0;
      uint32_t rec_width = 0;
#endif

      if (total_pixels+3 >= 1024) {
	kprintf(consoleKey, "** ERROR: DrawableRequest(): BitBlt with too"
		" many datapoints!\n");
	return true; 
      }
 
      expect = (4 + total_pixels) * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(consoleKey, "DrawableRequest:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_capros_key_RequestError;
	return true;
      }

      width  = receive_buffer[2];
      height = (total_pixels / width);

#ifdef ALWAYS_USE_RECTFILL
      /* Use rect_fill() instead of set_pixel(): First stab is just to
      scan horizontally (ie. in the x-direction) and monitor the pixel
      value to be rendered.  When the pixel value changes, issue a
      rect_fill for the "last" pixel value.  */
      size.y = 1;

      for (y = receive_buffer[1]; y < receive_buffer[1] + height; y++) {
	rec_width = 0;
	pt.x = receive_buffer[0];
	pt.y = y;
	current_val = receive_buffer[4+count];

	for (x = receive_buffer[0]; x < receive_buffer[0] + width; x++) {
 
	  /* If pixel value hasn't changed, bump counters and continue */
	  if (receive_buffer[4+count] == current_val) {
	    rec_width++;
	    count++;
	    continue;
	  }

	  /* Once the pixel value has changed, draw stored values and
	     reset counters: */
	  size.x = rec_width;
	  rect_assemble(pt, size, &rect);
	  drawable_RectFill(d, rect, current_val, ROP_COPY);

	  pt.x = x;
	  pt.y = y;
	  current_val = receive_buffer[4+count];
	  rec_width = 1;
	  count++;
	}

	/* If we didn't issue a rect_fill for the end of the
	   previous horizontal scan line, do so now */
	if (pt.x < x) {
	  size.x = rec_width;
	  rect_assemble(pt, size, &rect);
	  drawable_RectFill(d, rect, current_val, ROP_COPY);
	}
      }

      msg->snd_code = RC_OK;
#else
      rect.topLeft.x = receive_buffer[0];
      rect.topLeft.y = receive_buffer[1];
      rect.bottomRight.x = rect.topLeft.x + width;
      rect.bottomRight.y = rect.topLeft.y + height;
      msg->snd_code = drawable_BitBlt(d, rect, &receive_buffer[4]);
#endif

    }
    break;

  case 9999:
    {
      msg->snd_code = drawable_BigBitBltTest(d);
    }
    break;

    /* FIX: For now, ignore this command.  Proper way to destroy
       Drawable is to shut down driver, so use OC_Video_Shutdown
       instead.  */
  case OC_Drawable_Destroy:
    {
      msg->snd_code = RC_capros_key_RequestError;
    }
    break;

  default:
    {
      DEBUG(video_cmds) kprintf(consoleKey, "No such command: 0x%04x.\n",
				msg->rcv_code);
      msg->snd_code = RC_capros_key_UnknownRequest;
      break;
    }
  }
  return true;
}

