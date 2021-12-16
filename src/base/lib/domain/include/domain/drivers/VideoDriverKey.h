#ifndef __VIDEO_DRIVER_KEY_H__
#define __VIDEO_DRIVER_KEY_H__

/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
 *
 * This file is part of the EROS Operating System runtime library.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* List of PCI vendor and device ids that are supported: */

/* VMWare */
#define VMWareVendorID 0x15AD
#define VMWareDeviceID 0x0405

/* This file defines the API common to all video drivers.
 */

enum VideoDriverCommands {
  OC_Video_Initialize,

  OC_Video_Shutdown,
  /*
   Purpose:  Request that the video driver domain terminate itself.

   Returns:
     RC_OK
     RC_RequestError
  */

  OC_Video_GetMode,
  /*
   Purpose:  Retrieve the current graphics mode.

   Output:
     w1 = current width (in pixels)
     w2 = current height (in pixels)
     w3 = current depth (in pixels)

   Returns:
     RC_OK
     RC_Video_NotInitialized
  */


  OC_Video_SetMode,
  /*
   Purpose:  Set the video graphics mode to specified resolution.

   Input:
     w1 = requested width (in pixels)
     w2 = requested height (in pixels)
     w3 = requested depth (in pixels)

   Returns:
     RC_OK
     RC_Video_NotSupported
  */

  OC_Video_MaxResolution,
  /*
   Purpose: Get the maximum width, height and depth the video driver
   supports

   Output:
     w1  = max width (in pixels)
     w2  = max width (in pixels)
     w3  = max width (in pixels)

   Returns:
     RC_OK
     RC_Video_NotInitialized
  */


  OC_Video_DefineCursor,
  /*
   Purpose:  For video devices that support it, send the pixmap def'n of
             a cursor to the device for later use.
   Input:
     snd_data[0]   = id that will represent this cursor
     snd_data[1]   = x coord (relative to the upper left corner of the
                         cursor) of the cursor's "hot spot"
     snd_data[2]   = y coord of the "hot spot"
     snd_data[3]   = width (in pixels) of the cursor
     snd_data[4]   = height (in pixels) of the cursor
     snd_data[5]   = depth (in pixels) of the AND mask bits
     snd_data[6]   = depth (in pixels) of the XOR bits
     snd_data[7 .. 7+(2*cursor_width)]  = scanlines for AND mask followed by
                         scanlines for XOR data

   Output:
     RC_OK           
     RC_Video_NotInitialized
     RC_Video_BadCursorID
     RC_Video_NotSupported
     RC_RequestError 
  */

  OC_Video_HideCursor,
  /*
   Purpose: Turn the display of the cursor "off".  

   Input: 
     w1 = cursor id (must have already been defined; if not,
                     results are unknown.)

   Returns:
     RC_OK
     RC_Video_NotInitialized
     RC_Video_BadCursorID
     RC_Video_NotSupported
  */

  OC_Video_ShowCursor,
  /*
   Purpose: For video devices that support it, display the pixmap
   associated with the cursor id at the location (x,y).  Origin is
   upper left; x-axis increases to the right and y-axis increases
   downward.  The client is responsible for maintaining the current
   cursor location, if it needs it, and also should ensure that (x, y)
   falls within the screen boundaries.

   Input:
     w1 = cursor id
     w2 = x (in pixels)
     w3 = y (in pixels)

   Returns:
     RC_OK
     RC_Video_NotInitialized
     RC_Video_BadCursorID
     RC_Video_NotSupported
     RC_RequestError 
  */

  OC_Video_Functionality,
  /*
   Purpose: Ping the video device to see what functionality it
   supports (such as accelerated operations).  Use the output value
   with the flags defined below to identify specific functionality.

   Output:
     w1 =  32 bit value representing what the card can do

   Returns:
      RC_OK
      RC_Video_NotInitialized
      RC_RequestError
  */

  OC_Video_GetDrawable,
  /*
  Purpose: Return a capability to the Drawable interface (which every
           video driver must support).

  Output:
    key0 = key to Drawable interface

  Returns:
    RC_OK
    RC_Video_NotInitialized
  */

  OC_Video_DefinePixmapLine,
/*
  Purpose: Use this command to define a pixmap line by line.  In other
  words, each invocation defines a separate scan line of the pixmap.
  (This design avoids exceeding any potential message size limits of a
  single invocation.  Pixmaps tend to be large!)

  Input:
    snd_data[0] = pixmap id
    snd_data[1] = pixmap width  (in pixels)
    snd_data[2] = pixmap height (in pixels)
    snd_data[3] = pixmap depth  (in pixels)
    snd_data[4] = line number   (first line number is zero)
    snd_data[5 .. 5+ <scanline size>] = scanline

    *Note:  <scanline size> can be computed using PIXMAP_SCANLINE_SIZE macro.

  Returns:
    RC_OK
    RC_Video_NotSupported
    RC_RequestError
*/

  OC_Video_RectPixmapFill,
/*
  Purpose: Fill a rectangular region of the Drawable with the
            specified pixmap. Pixmap origin will be rendered at the
            specified (x,y) and the pixmap will be copied as many
            times as needed to fill the specified width and
            height. (i.e. the pixmap will not be stretched)

  Input:
    snd_data[0] = pixmap id
    snd_data[1] = x
    snd_data[2] = y
    snd_data[3] = width 
    snd_data[4] = height 
  
  Returns:
    RC_OK
    RC_Video_NotSupported
    RC_RequestError
*/


  };

/* ensure that these error codes don't conflict with the predefined
   codes (like RC_OK, RC_RequestError, etc.) */
#define VIDEO_ERROR_CODES_START 50
enum VideoDriverReturnCodes {
  RC_Video_AccelError = VIDEO_ERROR_CODES_START,     /* error with
							video device's
							accelerated
							hardware */
  RC_Video_BusError,	   /* error with the bus address register */
  RC_Video_HWError,        /* general error with device itself */
  RC_Video_MemMapFailed,   /* couldn't map the device's memory */
  RC_Video_NotInitialized, /* attempt to command device before it's
			      initialized */
  RC_Video_HWInitFailed,   /* error initializing underlying hardware
			      systems */
  RC_Video_BadCursorID,    /* invalid cursor id */
  RC_Video_NotSupported,   /* hardware doesn't support requested operation */
  RC_Video_BadID,          /* hardware has a maximum id and you exceeded it */
};

/* The following flags represent what the underlying hardware is
   capable of.  Each graphics device driver should report its
   functionality in terms of these macros. 

   VIDEO_RECT_FILL:  fill a rectangular region with a specified color

   VIDEO_RECT_COPY:  copy a rectangular region from one location to another

   VIDEO_RECT_PAT_FILL: fill a rectangular region with a specified pattern 

   VIDEO_OFFSCREEN:  hardware has "offscreen" memory for storing pixmaps,
                     bitmaps, cursors, etc.

   VIDEO_RASTER_OP: hardware supports using a logical raster op
                     in conjunction with some or all of its accelerated 
                     commands

   VIDEO_HW_CURSOR:  device supports a hardware cursor

   VIDEO_ALPHA_CURSOR: device supports an alpha hardware cursor
*/

#define	VIDEO_RECT_FILL	       0x0001
#define	VIDEO_RECT_COPY	       0x0002
#define	VIDEO_RECT_PAT_FILL    0x0004
#define	VIDEO_OFFSCREEN        0x0008
#define	VIDEO_RASTER_OP	       0x0010
#define	VIDEO_HW_CURSOR	       0x0020
#define VIDEO_ALPHA_CURSOR     0x0200

#endif

/* Stubs for capability invocation */

/* Pass 'num_data_elements' initialization values (via the init_data
   array) to the driver for initialization.  For example, some drivers
   need to know their PCI base address register. */
uint32_t video_initialize(uint32_t video_key,
			  uint32_t num_data_elements,
			  uint32_t *init_data);

uint32_t video_shutdown(uint32_t video_key);

uint32_t video_functionality(uint32_t video_key,
			     /* out */ uint32_t *flags);

uint32_t video_define_pixmap(uint32_t video_key,
			     uint32_t pixmap_id,
			     uint32_t pixmap_width,
			     uint32_t pixmap_height,
			     uint32_t pixmap_depth,
			     uint8_t *pixmap_data);

uint32_t video_render_pixmap(uint32_t video_key,
			     uint32_t pixmap_id,
			     uint32_t topLeftX,
			     uint32_t topLeftY,
			     uint32_t width,
			     uint32_t height);

/* To generate cursor bitmaps, use the X11 bitmap tool.  Draw an
   outline of the cursor and save it as the "cursor bits" structure.
   Then, invert the outline and remove all pixels inside the cursor
   and save this as the "cursor mask" structure.  The bitmap utility
   stores the pixels in least-significant-bit-first order.  This
   routine includes a conversion macro to convert the byte order. */
uint32_t video_define_cursor(uint32_t video_key,
			     uint32_t cursor_id,
			     uint32_t hotspot_x,
			     uint32_t hotspot_y,
			     uint32_t cursor_width,
			     uint32_t cursor_height,
			     uint32_t cursor_depth,
			     uint8_t *cursor_bits,
			     uint8_t *cursor_mask_bits);

uint32_t video_show_cursor_at(uint32_t video_key,
			      uint32_t cursor_id,
			      uint32_t x,
			      uint32_t y);

uint32_t video_max_resolution(uint32_t video_key,
			      /* out */ uint32_t *max_width,
			      /* out */ uint32_t *max_height,
			      /* out */ uint32_t *max_depth);

uint32_t video_set_resolution(uint32_t video_key,
			      uint32_t width,
			      uint32_t height,
			      uint32_t depth);

uint32_t video_get_resolution(uint32_t video_key,
			      /* out */ uint32_t *width,
			      /* out */ uint32_t *height,
			      /* out */ uint32_t *depth);

uint32_t video_get_drawable_key(uint32_t video_key,
				uint32_t drawable_keyreg);
