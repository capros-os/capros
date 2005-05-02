#ifndef __DRAWABLEKEY_H__
#define __DRAWABLEKEY_H__

/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime distribution.
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

/* This file defines the API for the Drawable interface.  The Drawable
   is an "output-only" interface, used for generating graphics.  (Note
   there is no "get_pixel" command.) Each command is defined by an
   opcode.  Comments for each command define the needed invocation
   arguments and the return values.
*/

/* Client commands */
enum DrawableCommands {
  OC_Drawable_SetClipRegion,
/* 
  Purpose: Modify the clipping dimensions of the Drawable.  When
  displaying contents of a Drawable, only those pixels within the
  clipping region are displayed.  The most upper left coordinate for a
  clipping region is (0,0), so all values for x and y must be
  nonnegative.

  Input:
    snd_data[0] = upper left x
    snd_data[1] = upper left y
    snd_data[2] = bottom right x
    snd_data[3] = bottom right y

  Returns:
    RC_OK 
    RC_RequestError
*/

  OC_Drawable_SetPixel,
/*
  Purpose: Set the pixel (x,y) to the specified color. Upper left
  corner of Drawable is (0,0) and x-axis increases to the right,
  y-axis increases downward.  All pixel values are subject to the
  current clipping region.  (So, you can set a pixel, but if it falls
  outside the clipping region you'll never see it!)

  Input:
   w1 = x 
   w2 = y 
   w3 = color   (currently aRGB format)

  Returns:
   RC_OK
*/

  OC_Drawable_RectFill,
/*
  Purpose:  Draw a filled rectangle within the Drawable.

  Input:
    snd_data[0] = upper left x of rectangle 
    snd_data[1] = upper left y of rectangle 
    snd_data[2] = width 
    snd_data[3] = height
    snd_data[4] = color (currently aRGB format)
    snd_data[5] = raster operation

  Returns:
    RC_OK
    RC_RequestError;
*/

  OC_Drawable_Redraw,
/* 
  Purpose: Render the current contents of the Drawable.  (Finish all
  your drawing first, then use this command to render all your mods.)
  Rectangular area withing the Drawable defined by (upper left x,
  upper left y) to (upper left x + width, upper left y + height) will
  be rendered.  Width and height will be clipped to Drawable
  clipping region, if necessary.

  Input:
   snd_data[0] = upper left x of the rectangular region you want rendered
   snd_data[1] = upper left y of the rectangular region you want rendered
   snd_data[2] = width 
   snd_data[3] = height

  Returns:
    RC_OK
    RC_RequestError
*/

  OC_Drawable_Clear,
/*
  Purpose:  Clear the Drawable to the specified color.

  Input:
    w1 = background color

  Returns:
    RC_OK
*/


  OC_Drawable_BitBlt,
  /*
   Purpose:  Copy several pixels at once to the framebuffer.

   Input:
     snd_data[0] = upper left x of start
     snd_data[1] = upper left y of start
     snd_data[2] = width of area
     snd_data[3] = total number of pixels to copy
     snd_data[4..4+total] = pixel data

   Returns:
     RC_OK
     RC_RequestError
  */


  OC_Drawable_Destroy,
  /*
  Purpose:  Destroy the Drawable.

  Returns:
    RC_OK
    RC_RequestError
  */

  OC_Drawable_LineDraw,

  OC_Drawable_TriDraw,

  OC_Drawable_TriFill,

  OC_Drawable_RectFillBorder, 

};

enum DrawableRasterOps {
  ROP_CLEAR          = 0x00,
  ROP_AND            = 0x01,
  ROP_AND_REVERSE    = 0x02,
  ROP_COPY           = 0x03,
  ROP_AND_INVERTED   = 0x04,
  ROP_NOOP           = 0x05,     /* dst */
  ROP_XOR            = 0x06,     /* src XOR dst */
  ROP_OR             = 0x07,     /* src OR dst */
  ROP_NOR            = 0x08,     /* NOT src AND NOT dst */
  ROP_EQUIV          = 0x09,     /* NOT src XOR dst */
  ROP_INVERT         = 0x0a,     /* NOT dst */
  ROP_OR_REVERSE     = 0x0b,     /* src OR NOT dst */
  ROP_COPY_INVERTED  = 0x0c,     /* NOT src */
  ROP_OR_INVERTED    = 0x0d,     /* NOT src OR dst */
  ROP_NAND           = 0x0e,     /* NOT src OR NOT dst */
  ROP_SET            = 0x0f,     /* 1 */
};

/* Stubs for capability invocations.  */
uint32_t drawable_clear(uint32_t drawableKey, uint32_t color);

uint32_t drawable_redraw(uint32_t drawableKey, uint32_t topLeftX,
			 uint32_t topLeftY, uint32_t bottomRightX, 
			 uint32_t bottomRightY);

uint32_t drawable_set_clip_region(uint32_t drawableKey, uint32_t topLeftX,
				  uint32_t topLeftY, uint32_t bottomRightX, 
				  uint32_t bottomRightY);

uint32_t drawable_set_pixel(uint32_t drawableKey, uint32_t ptX, uint32_t ptY,
			    uint32_t color);

uint32_t drawable_linedraw(uint32_t drawableKey, uint32_t x1,
			   uint32_t y1, uint32_t x2, 
			   uint32_t y2,
			   uint32_t width,
			   uint32_t color,
			   uint32_t rasterOp);

uint32_t drawable_rectfill(uint32_t drawableKey, uint32_t topLeftX,
			   uint32_t topLeftY, uint32_t bottomRightX, 
			   uint32_t bottomRightY,
			   uint32_t color,
			   uint32_t rasterOp);

uint32_t drawable_rectfillborder(uint32_t drawableKey, uint32_t topLeftX,
			   uint32_t topLeftY, uint32_t bottomRightX, 
			   uint32_t bottomRightY,
			   uint32_t color,
			   uint32_t border_color,
			   uint32_t rasterOp);

uint32_t drawable_tridraw(uint32_t drawableKey, uint32_t pt1x, 
			  uint32_t pt1y, uint32_t pt2x, uint32_t pt2y, 
			  uint32_t pt3x, uint32_t pt3y, 
			  uint32_t brd1, uint32_t brd2, uint32_t brd3,
			  uint32_t color, uint32_t raster_op);

uint32_t drawable_trifill(uint32_t drawableKey, uint32_t pt1x, 
			  uint32_t pt1y, uint32_t pt2x, uint32_t pt2y, 
			  uint32_t pt3x, uint32_t pt3y, 
			  uint32_t color, uint32_t raster_op);
     
uint32_t drawable_bitblt(uint32_t drawableKey, uint32_t startX, 
			 uint32_t startY, uint32_t width,
			 uint32_t total_pixels, uint32_t *pixel_data);

uint32_t drawable_destroy(uint32_t drawableKey);

/* This one's more than just a wrapper.  It relies on a default
   console font but since it relies on a "printf" and other supporting
   functions, it resides locally in whatever domain needs it.
 */
uint32_t drawable_draw_string(uint32_t drawableKey, uint32_t x, uint32_t y,
			      uint32_t fg_color, uint32_t bg_color, 
			      uint8_t *str);

#endif
