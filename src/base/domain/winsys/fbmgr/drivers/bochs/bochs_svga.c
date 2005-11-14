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

#include <eros/target.h>
#include <eros/Invoke.h>
#include <eros/machine/io.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/StdKeyType.h>
#include <eros/cap-instr.h>

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>

#include <string.h>
#include <stdlib.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>
#include <domain/MemmapKey.h>

#include <addrspace/addrspace.h>
#include <graphics/color.h>

#include "debug.h"

#include "../../../winsyskeys.h"
#include "../video.h"
#include "bochs_svga.h"

#define max(a,b) ((a) >= (b) ? (a) : (b))
#define min(a,b) ((a) <= (b) ? (a) : (b))

/* The following keeps the resolution within limits for external video
   projection systems. */
#define LIMIT_RESOLUTION

#define FRAMEBUFFER_START 0x80000000u   /* 2 GB into address space */


/* First command to driver must be "initialize", otherwise driver
   will refuse all other commands.  The "initialize" code is not done
   by the constructor because the "initialize" code needs to know the
   PCI base address register (which varies and must be passed in with
   the initialize command). */
static bool driver_initialized = false;
#define RETURN_IF_NOT_INITIALIZED  if (!driver_initialized) { \
                                 msg->snd_code = RC_Video_NotInitialized; \
kprintf(KR_OSTREAM, "*** DRIVER NOT INITIALIZED!\n"); \
                                 return true; \
                               }

/* Need to validate raster operations
  note that the bochs device does not do any "hardware" acceleration */
#define RETURN_IF_BAD_ROP(rop) \
      if (rop < SVGA_ROP_CLEAR || rop > SVGA_ROP_SET) {  \
	return RC_eros_key_RequestError; \
      }


/* To generate bitmaps, I used the X11 bitmap tool.  I drew an
   outline of the cursor and saved it as the "cursor bits" structure.
   Then, I inverted the outline and removed all pixels inside the
   cursor and saved this as the "cursor mask" structure.  The bitmap
   utility stores the pixels in least-significant-bit-first order.
   Thus, this conversion macro is necessary to convert the bytes. */
#define CONVERT_BYTE(byte) ((byte & 0x01) << 7 | (byte & 0x02) << 5 | \
                            (byte & 0x04) << 3 | (byte & 0x08) << 1 | \
                            (byte & 0x10) >> 1 | (byte & 0x20) >> 3 | \
                            (byte & 0x40) >> 5 | (byte & 0x80) >> 7)

/*  Call this macro repeatedly to convert 8-bit pixel data to an array
    of 32-bit values.  After each use, the pixel data can be
    extracted  */
#define HEADER_PIXEL(data,pixel) {\
  pixel[0] = (((data[0] - 33) << 2) | ((data[1] - 33) >> 4)); \
  pixel[1] = ((((data[1] - 33) & 0xF) << 4) | ((data[2] - 33) >> 2)); \
  pixel[2] = ((((data[2] - 33) & 0x3) << 6) | ((data[3] - 33))); \
  data += 4; \
}

/* globals */
uint8_t *framebuffer;		  /* Start address of the current fb 
                                     (depends on current resolution and is >= 
				     framebuffer_abszero) */
uint8_t *framebuffer_abszero;	  /* Start address of the max-size fb */
uint32_t fb_size;		  /* Max size for framebuffer */
uint32_t send_data[10];           /* Buffer for sending meta-data to clients */

uint16_t fb_lss;		/* Needed for patching up ProcAddrSpace */
uint16_t current_xres, current_yres;

rect_t clipRegion;

/* Declare a big buffer for receiving data from invocations */
uint32_t receive_buffer[1024];


#define MAX_CURSORS 8
struct softcursor {
  uint8_t width, height;
  uint8_t hotspot_x, hotspot_y;
  uint32_t bits[32*32];
};
struct softcursor cursors[MAX_CURSORS];
uint32_t cursor_save_under[32*32];
bool cursor_visible;
uint8_t cursor_saved_width; /*  saved width and height may be different from*/
uint8_t cursor_saved_height; /* the original cursor's width + height */

point_t cursor_user_pos;
uint32_t cursor_id;
/* This is the upper-left corner of the cursor bitmap and may be slightly off-screen.
   It is different from what the client thinks the current cursor position is! */
point_t cursor_saved_pos;

extern unsigned fbfault_received();

static uint32_t
bound_dim(uint32_t dim, uint32_t delta, uint32_t max)
{
  return (dim + delta) > max ? (max - delta) : dim; 
}

static void
bochs_setreg(uint16_t regno, uint16_t value)
{
  outw (regno, BOCHS_VBE_DISPI_IOPORT_INDEX);
  outw (value, BOCHS_VBE_DISPI_IOPORT_DATA);
}

static uint32_t
graphics_init()
{

  cursor_id = 0xFFFFFFFF;
  cursor_visible = 0;

  bochs_setreg (BOCHS_VBE_DISPI_INDEX_ENABLE, BOCHS_VBE_DISPI_DISABLED);
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_ID, BOCHS_VBE_DISPI_ID2);

  current_xres = 800;
  current_yres = 600;

  bochs_setreg (BOCHS_VBE_DISPI_INDEX_XRES, 800);
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_YRES, 600);
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_BPP, BOCHS_VBE_DISPI_BPP_32);

  /* Establish the start address of the video framebuffer.  This
     changes only when we switch resolutions.
     (actually, in bochs it never changes at all) */
  framebuffer = framebuffer_abszero;

  /* Turn on the device (effectively): */
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_ENABLE, BOCHS_VBE_DISPI_ENABLED);

  /* Set the default clipping region: */
  clipRegion.topLeft.x = 0u;
  clipRegion.topLeft.y = 0u;
  clipRegion.bottomRight.x = 800;
  clipRegion.bottomRight.y = 600;

  return RC_OK;
}

/* Build an address space for the framebuffer.  Use start
   address and size as reported by video device. */
static uint32_t
framebuffer_map()
{
  uint32_t result;
  uint32_t fb_start;

  /* First, we need to get the physical address of the start of the FB
     as well as its max size. */
  fb_start = VBE_DISPI_LFB_PHYSICAL_ADDRESS;
  fb_size  = 4194304; /* 4MiB */

  DEBUG(video_fb) {
    kprintf(KR_OSTREAM, "In framebuffer_map():: fb_start = 0x%08x\n", 
	    fb_start);
    kprintf(KR_OSTREAM, "                       fb_size  = 0x%08x\n", fb_size);
  }
     
  kprintf(KR_OSTREAM, "Mapping the frame buffer\n");
  /* Now inform the kernel that we're going to map a certain address range */
  result = eros_DevPrivs_publishMem(KR_DEVPRIVS, fb_start, fb_start+fb_size, 0);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "*** video driver: call to publishMem failed: %u (0x%x)",
	    result, result);
    return RC_eros_key_RequestError;
  }

  DEBUG(video_fb) kprintf(KR_OSTREAM, "framebuffer_map() constructing a memory"
			  " mapper domain...");
  result = constructor_request(KR_MEMMAP, KR_BANK, KR_SCHED, KR_VOID,
			       KR_FRAMEBUF);
  if (result != RC_OK) {
    DEBUG(video_fb) kprintf(KR_OSTREAM, "** ERROR: framebuffer_map() "
			    "constructor_request() returned %u", result);
    return result;
  }

  DEBUG(video_fb) kprintf(KR_OSTREAM, "framebuffer_map() now calling "
			  "memmap_map()");
  result = memmap_map(KR_FRAMEBUF, KR_PHYSRANGE, fb_start, fb_size, &fb_lss);
  DEBUG(video_fb) kprintf(KR_OSTREAM, "   returned fb_lss is %d\n", fb_lss);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: framebuffer_map() returned %u", result);
    return result;
  }

  framebuffer_abszero = (uint8_t *)FRAMEBUFFER_START;

  return RC_OK;
}

/* Place the newly constructed "mapped memory" tree into the process's
   address space. */
static uint32_t
patch_addrspace(void)
{
  /* Assumptions:  winsys main has already prep'd address space for
     mapping, so next available slot in ProcAddrSpace node is 16 */
  uint32_t next_slot = 16;

  /* Insert FRAMEBUF capability and then any needed window keys.  If
     the fb_lss requires local window keys, then fb_lss must be
     EROS_ADDRESS_LSS.  However, there isn't room in a 32-slot node
     for the 15 needed local window keys for an lss=4 subspace (since
     we already have used up 18 slots)! Thus, punt if this is the
     case. */
  DEBUG(video_fb) {
    kprintf(KR_OSTREAM, "bochs_svga: patch_addrspc() "
	    "inserting FB capability in slot %u", next_slot);
  }

  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  node_swap(KR_SCRATCH, next_slot++, KR_FRAMEBUF, KR_VOID);
  if (fb_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: bochs_svga(): no room for local window "
	     "keys for FRAMEBUF!");


  return next_slot;
}

/* Generate address faults in the entire mapped region in order to
   ensure entire address subspace is fabricated and populated with
   correct page keys. NOTE: This seems to take a lot less time if the
   'base' is treated as a 32-bit address and 32 bytes are handled at a
   time. */
static void
init_mapped_memory(uint32_t *base, uint32_t size)
{
  uint32_t u;

  kprintf(KR_OSTREAM, "bochs_svga: init'ing mapped memory at 0x%08x through 0x%08x...",
	  (uint32_t)base, (uint32_t)((char *)base + size));

  for (u=0; u < (size / (sizeof(uint32_t))); u=u+EROS_PAGE_SIZE)
    base[u] &= 0xffffffffu;

  kprintf(KR_OSTREAM, "bochs_svga: init mapped memory complete.");
}

static void
init_complete(void)
{
  DEBUG(video_init) kprintf(KR_OSTREAM, "Done with driver init.\n");
  driver_initialized = true;
}

uint32_t 
bochs_video_initialize(uint32_t num_elements, uint32_t *elements,
		 /* out */ uint32_t *next_available_slot)
{
  if (next_available_slot == NULL)
    return RC_eros_key_RequestError;

  DEBUG(video_init) kprintf(KR_OSTREAM, "video_initialize() storing pci info:"
			    " 0x%08x  0x%08x\n", elements[0], elements[1]);

  DEBUG(video_init) kprintf(KR_OSTREAM, "video_initialize() mapping fb.\n");

  /* Now use memory mapper to fabricate a "sub space" for the
     framebuffer */
  if (framebuffer_map() != RC_OK)
    return RC_Video_MemMapFailed;

  /* Now patch up our address space accordingly */
  *next_available_slot = patch_addrspace();

  /* Because the mapped memory subspaces are expanded lazily,
     cause address faults to occur up front to ensure that entire
     space tree is fabricated and populated. */
  DEBUG(video_fb) kprintf(KR_OSTREAM, "framebuffer_map() now causing addr"
			  "faults starting at = 0x%08x",
			  (uint32_t)framebuffer_abszero);

  init_mapped_memory((uint32_t *)framebuffer_abszero, fb_size);
  /* Finally, enable the graphics and have some fun! */
  if (graphics_init() != RC_OK)
    return RC_Video_HWInitFailed;

  init_complete();
  return RC_OK;
}

bool
bochs_video_get_resolution(/* out */ uint32_t *width,
		     /* out */ uint32_t *height,
		     /* out */ uint32_t *depth)
{
  if (!width || !height || !depth)
    return false;

  *width  = current_xres;
  *height = current_yres;
  *depth  = 32;

  return true;
}

bool
bochs_video_max_resolution(/* out */ uint32_t *width,
		     /* out */ uint32_t *height,
		     /* out */ uint32_t *depth)
{
  if (!width || !height || !depth)
    return false;

  *width  = BOCHS_VBE_DISPI_MAX_XRES;
  *height = BOCHS_VBE_DISPI_MAX_YRES;
  *depth  = 32;

  return true;
}

uint32_t
bochs_video_set_resolution(uint32_t width, uint32_t height, uint32_t depth)
{
  if (width > BOCHS_VBE_DISPI_MAX_XRES)
    return RC_Video_NotSupported;

  if (height > BOCHS_VBE_DISPI_MAX_YRES)
    return RC_Video_NotSupported;

  /* Ignore requested depth for now since that's what VMWARE driver does. */
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_ENABLE, BOCHS_VBE_DISPI_DISABLED);
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_ID, BOCHS_VBE_DISPI_ID2);

  current_xres = width;
  current_yres = height;

  bochs_setreg (BOCHS_VBE_DISPI_INDEX_XRES, width);
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_YRES, height);
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_BPP, BOCHS_VBE_DISPI_BPP_32);

  /* Establish the start address of the video framebuffer.  This
     changes only when we switch resolutions.
     (actually, in bochs it never changes at all) */
  framebuffer = framebuffer_abszero;

  /* Turn on the device (effectively): */
  bochs_setreg (BOCHS_VBE_DISPI_INDEX_ENABLE, BOCHS_VBE_DISPI_ENABLED);

  return RC_OK;
}

static void
undisplay_cursor(void)
{
  int cy, displaywidth;

  if (cursor_visible) {
    displaywidth = 4 * current_xres;
    for (cy = 0; cy < cursor_saved_height; cy++) {
      memcpy(&framebuffer[cursor_saved_pos.x
                       + (cursor_saved_pos.y + cy) * displaywidth],
	     &cursor_save_under[cy * cursor_saved_width],
	     cursor_saved_width);
    cursor_visible = 0;
    }
  }
}

uint32_t
bochs_video_show_cursor_at(uint32_t id, point_t location)
{
  int cx, cy, displaywidth;
  int lskip, rskip, topskip, botskip;
  
  if (id > MAX_CURSORS)
    return RC_Video_BadCursorID;

  kprintf(KR_OSTREAM, "in bochs_video_show_cursor_at id=%d location={x=%d y=%d}\n", id, location.x, location.y);

  displaywidth = current_xres * 4;
  undisplay_cursor();

  /* Just in case the client didn't check the pixel: */
  location.x = bound_dim(location.x, 0, current_xres);
  location.y = bound_dim(location.y, 0, current_yres);

  cursor_user_pos = location;

  lskip = max (0, cursors[id].hotspot_x - location.x);
  rskip = max (current_xres, location.x +
	       cursors[id].width - cursors[id].hotspot_x)
          - current_xres;
  topskip = max (0, cursors[id].hotspot_y - location.y);
  botskip = max (current_yres, location.y +
		 cursors[id].height - cursors[id].hotspot_y)
            - current_yres;

  cursor_saved_pos.x = location.x + lskip;
  cursor_saved_pos.y = location.y + topskip;
  cursor_saved_height = cursors[id].height;
  cursor_saved_width =  cursors[id].width;
  cursor_id = id;

  /*  kprintf(KR_OSTREAM, "cursorwidth cursorheight topskip botskip lskip rskip = %d %d %d %d %d %d\n", 
      cursors[id].width, cursors[id].height, topskip, botskip, lskip, rskip);*/
 
  /* iterate over the onscreen portion of the cursor, saving the
     framebuffer contents underneath (done a scanline at a time),
     then drawing the visible pixels of the cursor */

  /* kprintf(KR_OSTREAM, "about to read from cursor_save_under (%08x)\n", cursor_save_under);
  kprintf(KR_OSTREAM, "cursor_save_under[0] = %d\n", cursor_save_under[0]);
  kprintf(KR_OSTREAM, "about to read from frame buffer (%08x)\n", framebuffer);
  kprintf(KR_OSTREAM, "framebuffer[0] = %d\n", framebuffer[0]);*/
  
  for (cy = topskip; cy < cursors[id].height - botskip; cy++) {
    memcpy(&cursor_save_under[lskip + cy * cursors[id].width],
	   &framebuffer[location.x + lskip + (location.y + cy) * displaywidth],
	   cursors[id].width - lskip - rskip);
    for (cx = lskip; cx < cursors[id].width - rskip; cx++) {
      /* FIX: for now, alpha must be 0 (opaque) or 255 (transparent)
         other values will be rendered as if opaque */
      /* alpha is stored in the high byte of each pixel in the cursor */
      if (cursors[id].bits[cx + cy * cursors[id].width] >> 24 == 0) {
	framebuffer[location.x + cx + (location.y + cy) * displaywidth] =
	  cursors[id].bits[cx + cy * cursors[id].width] & 0x00FFFFFF;
      }
    }
  }

  cursor_visible = 1;

  return RC_OK;
}

uint32_t
bochs_video_functionality(void)
{
  uint32_t retval = 0u;

  return retval;      
}

uint32_t
bochs_video_define_cursor(uint32_t id, point_t hotspot, point_t size,
			  uint32_t depth, uint8_t *bits, uint8_t *mask_bits)
{
  bool cursor_was_undisplayed = 0;

  DEBUG(video_fb) kprintf (KR_OSTREAM, "in sbochs_video_define_cursor (id=%d)...\n", id);

  if (id > MAX_CURSORS)
    return RC_Video_BadCursorID;

  if ((unsigned)size.x > 32 || (unsigned)size.y > 32)
    return RC_Video_NotSupported;

  if (cursor_id == id && cursor_visible) {
    cursor_was_undisplayed = 1;
    undisplay_cursor();
  }
 
#ifdef CONVERTED_TO_ALPHA_CURSORS

  cursors[id].hotspot_x = hotspot.x;
  cursors[id].hotspot_y = hotspot.y;
  cursors[id].width = size.x;
  cursors[id].height = size.y;

  memcpy (cursors[id].bits, cursor_bits_and_alpha, min(4096, size.x * size.y * 4));

#else

  kprintf (KR_OSTREAM, "in bochs_video_define_cursor, #else block ...\n");
  cursors[id].hotspot_x = 1;
  cursors[id].hotspot_y = 1;
  cursors[id].width = 6;
  cursors[id].height = 7;
  {
#define CBLACK 0x00000000
#define CWHITE 0x00FFFFFF
#define CTRANS 0xFF000000
    static uint32_t cursor_bits_and_alpha[6*7] = {
      CBLACK, CTRANS, CTRANS, CTRANS, CTRANS, CTRANS,
      CBLACK, CBLACK, CTRANS, CTRANS, CTRANS, CTRANS,
      CBLACK, CWHITE, CBLACK, CTRANS, CTRANS, CTRANS,
      CBLACK, CWHITE, CWHITE, CBLACK, CTRANS, CTRANS,
      CBLACK, CWHITE, CWHITE, CWHITE, CBLACK, CTRANS,
      CBLACK, CWHITE, CWHITE, CWHITE, CWHITE, CBLACK,
      CBLACK, CBLACK, CBLACK, CBLACK, CBLACK, CBLACK,
    };
#undef CBLACK
#undef CWHITE
#undef CTRANS
    memcpy (cursors[id].bits, cursor_bits_and_alpha, sizeof(cursor_bits_and_alpha));
  }
#endif /* CONVERTED_TO_ALPHA_CURSORS */  


  kprintf (KR_OSTREAM, "in sbochs_video_define_cursor, post memcpy...\n");
  if (cursor_was_undisplayed)
    bochs_video_show_cursor_at(id, cursor_user_pos);

  return RC_OK;
}

uint32_t bochs_video_define_alpha_cursor(uint32_t id,
					 point_t hotspot,
					 point_t size,
					 uint32_t *bits)
{
  return RC_Video_NotSupported;
}

uint32_t bochs_video_define_vram_pixmap(uint32_t id,
			     point_t dims,
			     uint32_t depth,
			     uint8_t *bits)
{
  return RC_Video_NotSupported;
}


uint32_t bochs_video_show_vram_pixmap(uint32_t id, point_t src, rect_t dst)
{
  return RC_eros_key_RequestError;
}

uint32_t
bochs_video_rectfill(rect_t rect, color_t color, uint32_t raster_op)
{
  return RC_Video_NotSupported;
}

bool
bochs_video_copy_to_fb(uint32_t *buffer, rect_t bounds, point_t dst, 
		 uint32_t buffer_width)
{
  uint32_t rows;
  int32_t height = (bounds.bottomRight.y - bounds.topLeft.y);
  int32_t width  = (bounds.bottomRight.x - bounds.topLeft.x);
  uint32_t *from = buffer;
  int32_t offset;
  uint32_t displaywidth;
  point_t xlate = { dst.x - bounds.topLeft.x, dst.y - bounds.topLeft.y };
  int cursor_was_visible;

  displaywidth = current_xres;

  /* If client shoots the bank from whom this buffer was purchased,
     catch the process keeper's signal here and return false. */
  if (fbfault_received())
    return false;
  else {

  /* Remove the cursor before modifying the framebuffer 
     FIX: check if we're actually overdrawing the cursor before 
     saving + restoring the cursor */
    cursor_was_visible = cursor_visible;
    if (cursor_was_visible)
      undisplay_cursor();

    for (rows = bounds.topLeft.y; rows < bounds.topLeft.y+height; rows++) {
      uint32_t col = 0;

      for (col = bounds.topLeft.x; col < bounds.topLeft.x+width; col++) {
	  point_t pixel = { col + xlate.x, rows + xlate.y };

	/* First, clip the fb location to the current screen */
	  if (pixel.x > current_xres || pixel.x < 0 ||
	      pixel.y > current_yres || pixel.y < 0)
	    continue;

	offset = (col + xlate.x) + displaywidth * (rows + xlate.y);

	DEBUG(redraw)
	  kprintf(KR_OSTREAM, "video: testing offset %d (0x%08x) \n"
		  "                  against fb_size = 0x%08x\n", offset,
		  fb_size);

	/* Sometimes portions of a window are off the screen. Catch
	   those cases here.  (This shouldn't ever be exercised if the
	   clipping above is correct...?) */
	if (offset < 0 || offset > fb_size)
	  continue;

	DEBUG(redraw) {
	  kprintf(KR_OSTREAM, "   fb 0x%08x and offset = 0x%08x\n",
		  (uint32_t)framebuffer, offset);

	  kprintf(KR_OSTREAM, "   setting fb 0x%08x to contents of 0x%08x\n"
		  "                which should be [0x%08x]\n",
		  (uint32_t)(&(framebuffer[offset])),
		  (uint32_t)(&(from[col + rows*buffer_width])),
		  from[col + rows*buffer_width]);
	}

	{
	  uint32_t color = from[col + rows * buffer_width];

	  ((uint32_t *)framebuffer)[offset] = color;
	}
      }

      //      bcopy(from, &(framebuffer[offset]), width*sizeof(color_t));
      //      from += buffer_width;
    }

    if (cursor_was_visible)
      bochs_video_show_cursor_at(cursor_id, cursor_user_pos);

  }

  return true;
}

uint32_t
bochs_video_copy_rect(rect_t src, rect_t dst)
{
  return RC_Video_NotSupported;
}
