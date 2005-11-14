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

#include "svga_reg.h"
#include "vmware_io.h"
#include "debug.h"
#include "fifo.h"

#include "../../../winsyskeys.h"
#include "../video.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

/* The following keeps the resolution within limits for external video
   projection systems. */
#define LIMIT_RESOLUTION

#define FIFO_START        0x80000000u   /* 2 GB into address space */
#define FRAMEBUFFER_START 0x88000000u   /* 2.125 GB into address space */

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
/* Need to validate raster operations */
#define RETURN_IF_BAD_ROP(rop) \
      if (rop < SVGA_ROP_CLEAR || rop > SVGA_ROP_SET) {  \
	return RC_eros_key_RequestError; \
      }


/* All the cursor commands pertain to hardware cursors */
#define RETURN_IF_NO_HARDWARE_CURSOR \
      if ( !(card_functionality & SVGA_CAP_CURSOR_BYPASS) || \
           !(card_functionality & SVGA_CAP_CURSOR_BYPASS_2) ) { \
	msg->snd_code = RC_Video_NotSupported; \
	return true; \
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
uint32_t svga_regs[SVGA_REG_TOP]; /* Caches contents of SVGA registers */
uint32_t card_version_id;		/* PCI device id */
uint32_t base_address_reg;	  /* PCI base address reg for the vmware 
                                     device */
uint32_t card_functionality;	  /* Bit flags for what card can do */
uint32_t *fifo;			  /* Start address of card's command FIFO  */
uint32_t fifo_size;		  /* Max size for FIFO */
uint8_t *framebuffer;		  /* Start address of the current fb 
                                     (depends on current resolution and is >= 
				     framebuffer_abszero) */
uint8_t *framebuffer_abszero;	  /* Start address of the max-size fb */
uint32_t fb_size;		  /* Max size for framebuffer */
uint32_t send_data[10];           /* Buffer for sending meta-data to clients */

uint16_t fb_lss;		/* Needed for patching up ProcAddrSpace */
uint16_t fifo_lss;

rect_t clipRegion;

/* Declare a big buffer for receiving data from invocations */
uint32_t receive_buffer[1024];

extern unsigned fbfault_received();

/* Cache the current contents of the SVGA registers */
static void
read_registers()
{
  uint32_t j;

  for (j = 0; j < SVGA_REG_TOP-1; j++)
    svga_regs[j] = VMREAD(j); 

}

static uint32_t
bound_dim(uint32_t dim, uint32_t delta, uint32_t max)
{
  return (dim + delta) > max ? (max - delta) : dim; 
}

static uint32_t
graphics_init()
{
  /* vmWare suggests that we identify the guest operating system.  In
     this case, we identify EROS as "other". */
  VMWRITE( SVGA_REG_GUEST_ID, 0x500A); /* "other" operating system */

  /* Read the current registers */
  read_registers();

#if defined(LIMIT_RESOLUTION)
  VMWRITE( SVGA_REG_WIDTH, 
	   (svga_regs[SVGA_REG_MAX_WIDTH] > 1024) ? 1024 : 800);
  VMWRITE( SVGA_REG_HEIGHT, 
	   (svga_regs[SVGA_REG_MAX_HEIGHT] > 768) ? 768 : 600);

  /* Read the current registers again, because changing width and/or
     height changes other regs */
  read_registers();
#endif

  /* Establish the start address of the video framebuffer.  This
     changes only when we switch resolutions. */
  framebuffer = (uint8_t *) ((uint8_t *)(framebuffer_abszero) + 
			     svga_regs[SVGA_REG_FB_OFFSET]);

  /* Turn on the device (effectively): */
  VMWRITE( SVGA_REG_ENABLE, 1);

  /* Set the default clipping region: */
  clipRegion.topLeft.x = 0u;
  clipRegion.topLeft.y = 0u;
  clipRegion.bottomRight.x = svga_regs[SVGA_REG_WIDTH];
  clipRegion.bottomRight.y = svga_regs[SVGA_REG_HEIGHT];

  return RC_OK;
}

/* Cache the PCI base address register.  All FIFO calls and register
   reads/writes are based on this value. */
static bool
good_base_address_register(uint32_t card_id, uint32_t base)
{
  /* FIX: Is there a better way to check validity of PCI bars? */
  if (base == 0x0)
    return false;

  /* Store the value in a global variable.  The logical AND with ~3 is
     something that is not documented anywhere but is nonetheless
     essential! */
  card_version_id = card_id;
  base_address_reg = base & ~3;

  DEBUG(video_bar) kprintf(KR_OSTREAM, "** Using %u (0x%04x) as the bar.\n",
			   base_address_reg, base_address_reg);

  return true;
}

/* There are currently two vmware versions as reported by the vmware
   device.  This code has only been tested with version 2. */
static bool
good_vmware_version()
{
  uint32_t id;

  DEBUG(video_init) kprintf(KR_OSTREAM, "good_vmware_version(): about to "
			    "call VMWRITE...\n");
  /* The vmWare documentation outlines this procedure for identifying
     which version of the device you have. */
  VMWRITE( SVGA_REG_ID, SVGA_MAKE_ID(SUPPORTED_VMWARE_ID));
  id = VMREAD(SVGA_REG_ID) & 0xFF;

  DEBUG(video_init) {
    kprintf(KR_OSTREAM, "Inside good_vmware_version()::bar = 0x%04x\n",
	    base_address_reg);
    kprintf(KR_OSTREAM, "                              id = %u.\n", id);
  }

#if 0
  kprintf(KR_OSTREAM, "VMware video id %d\n", id);

  return id == SUPPORTED_VMWARE_ID;
#else
  return 1;
#endif
}

static uint32_t
get_card_functionality(void)
{
  return VMREAD(SVGA_REG_CAPABILITIES);
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
  fb_start = VMREAD(SVGA_REG_FB_START);
  fb_size  = VMREAD(SVGA_REG_FB_MAX_SIZE);

  DEBUG(video_fb) {
    kprintf(KR_OSTREAM, "In framebuffer_map():: fb_start = 0x%08x\n", 
	    fb_start);
    kprintf(KR_OSTREAM, "                       fb_size  = 0x%08x\n", fb_size);
  }

  /* FIX:  What to do if fb_start and/or fb_size are not page-aligned?

     uint64_t base = fb_start % EROS_PAGE_SIZE ?
        (fb_start / EROS_PAGE_SIZE) * EROS_PAGE_SIZE : fb_start;
     uint64_t bound = (base + fb_size) % EROS_PAGE_SIZE ?
        base + ((fb_size / EROS_PAGE_SIZE + 1) * EROS_PAGE_SIZE) :
        (base + size);

     Then use base and bound in the devprivs_publishMem() call?
  */
     
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

  /* Insert FIFO capability. NOTE:  if fifo_lss == EROS_ADDRESS_LSS
     then there won't be room for the framebuffer! */
  DEBUG(video_fifo) {
    kprintf(KR_OSTREAM, "vmware_svga: patch_addrspc() "
	    "inserting FIFO capability (with lss=%u) in slot %u", fifo_lss, 
	    next_slot);
  }

  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);
  node_swap(KR_SCRATCH, next_slot++, KR_FIFO, KR_VOID);
  if (fifo_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: vmware_svga(): no room for local window "
	     "keys for FIFO!");

  /* Insert FRAMEBUF capability and then any needed window keys.  If
     the fb_lss requires local window keys, then fb_lss must be
     EROS_ADDRESS_LSS.  However, there isn't room in a 32-slot node
     for the 15 needed local window keys for an lss=4 subspace (since
     we already have used up 18 slots)! Thus, punt if this is the
     case. */
  DEBUG(video_fb) {
    kprintf(KR_OSTREAM, "vmware_svga: patch_addrspc() "
	    "inserting FB capability in slot %u", next_slot);
  }

  node_swap(KR_SCRATCH, next_slot++, KR_FRAMEBUF, KR_VOID);
  if (fb_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: vmware_svga(): no room for local window "
	     "keys for FRAMEBUF!");

  return next_slot;
}

/* Map the device's command FIFO queue into our address space. */
static uint32_t
fifo_map()
{
  uint32_t result;
  uint32_t fifo_start;

  /* First, get the physical address of the start of the FIFO command
     queue as well as its max size. */
  fifo_start = VMREAD(SVGA_REG_MEM_START);
  fifo_size  = VMREAD(SVGA_REG_MEM_SIZE);

  DEBUG(video_fifo) {
    kprintf(KR_OSTREAM, "In fifo_map():: fifo_start = 0x%08x\n", fifo_start);
    kprintf(KR_OSTREAM, "                fifo_size  = 0x%08x\n", fifo_size);
  }

  kprintf(KR_OSTREAM, "Mapping the fifo\n");
  /* Now inform the kernel that we're going to map a certain address range */
  result = eros_DevPrivs_publishMem(KR_DEVPRIVS, fifo_start, fifo_start+fifo_size, 0);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "*** video driver (fifo): call to publishMem failed: %u (0x%x)",
	    result, result);
    return RC_eros_key_RequestError;
  }

  DEBUG(video_fifo) kprintf(KR_OSTREAM, "fifo_map() constructing a memory"
			    " mapper domain...");
  result = constructor_request(KR_MEMMAP, KR_BANK, KR_SCHED, KR_VOID,
			       KR_FIFO);
  if (result != RC_OK) {
    DEBUG(video_fifo) kprintf(KR_OSTREAM, "** ERROR: fifo_map() "
			      "constructor_request() returned %u", result);
    return result;
  }

  DEBUG(video_fifo) kprintf(KR_OSTREAM, "fifo_map() now calling memmap_map()");
  result = memmap_map(KR_FIFO, KR_PHYSRANGE, fifo_start, fifo_size, &fifo_lss);
  if (result != RC_OK) {
    kprintf(KR_OSTREAM, "** ERROR: fifo_map() returned %u", result);
    return result;
  }

  fifo = (uint32_t *)FIFO_START;

  return RC_OK;
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

  kprintf(KR_OSTREAM, "vmware_svga: init'ing mapped memory at 0x%08x...",
	  (uint32_t)base);

  for (u=0; u < (size / (sizeof(uint32_t))); u=u+EROS_PAGE_SIZE)
    base[u] &= 0xffffffffu;

  kprintf(KR_OSTREAM, "vmware_svga: init mapped memory complete.");
}

static void
init_complete(void)
{
  DEBUG(video_init) kprintf(KR_OSTREAM, "Done with driver init.\n");
  driver_initialized = true;
}

uint32_t 
vmware_video_initialize(uint32_t num_elements, uint32_t *elements,
		 /* out */ uint32_t *next_available_slot)
{
  if (next_available_slot == NULL)
    return RC_eros_key_RequestError;

  DEBUG(video_init) kprintf(KR_OSTREAM, "video_initialize() storing pci info:"
			    " 0x%08x  0x%08x\n", elements[0], elements[1]);

  /* First, store PCI base address register */
  if (!good_base_address_register(elements[0], elements[1]))
    return RC_Video_BusError;

  /* Then check vmware version */
  if (!good_vmware_version())
    return RC_Video_HWError;

  DEBUG(video_init) kprintf(KR_OSTREAM, "video_initialize() mapping fifo.\n");

  /* Now use memory mapper to fabricate a "sub space" for the
     SVGA command fifo queue */
  if (fifo_map() != RC_OK)
    return RC_Video_MemMapFailed;

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
  DEBUG(video_fifo) kprintf(KR_OSTREAM, "fifo_map() now causing addr"
			    "faults starting at = 0x%08x",
			    (uint32_t)fifo);

  init_mapped_memory(fifo, fifo_size);

  DEBUG(video_fb) kprintf(KR_OSTREAM, "framebuffer_map() now causing addr"
			  "faults starting at = 0x%08x",
			  (uint32_t)framebuffer_abszero);

  init_mapped_memory((uint32_t *)framebuffer_abszero, fb_size);

  /* Now determine what the video card is capable of */
  card_functionality = get_card_functionality();
  if (card_functionality == 0x0)
    return RC_Video_HWInitFailed;

  /* Finally, enable the graphics and have some fun! */
  if (graphics_init() != RC_OK)
    return RC_Video_HWInitFailed;

  /* Initialize the command FIFO queue */
  if (fifo_init(fifo, fifo_size) != RC_OK)
    return RC_Video_AccelError;

  init_complete();
  return RC_OK;
}

bool
vmware_video_get_resolution(/* out */ uint32_t *width,
		     /* out */ uint32_t *height,
		     /* out */ uint32_t *depth)
{
  if (!width || !height || !depth)
    return false;

  *width  = svga_regs[SVGA_REG_WIDTH];
  *height = svga_regs[SVGA_REG_HEIGHT];
  *depth  = svga_regs[SVGA_REG_BITS_PER_PIXEL];

  return true;
}

bool
vmware_video_max_resolution(/* out */ uint32_t *width,
		     /* out */ uint32_t *height,
		     /* out */ uint32_t *depth)
{
  if (!width || !height || !depth)
    return false;

  *width  = svga_regs[SVGA_REG_MAX_WIDTH];
  *height = svga_regs[SVGA_REG_MAX_HEIGHT];
  *depth  = svga_regs[SVGA_REG_HOST_BITS_PER_PIXEL];

  return true;
}

uint32_t
vmware_video_set_resolution(uint32_t width, uint32_t height, uint32_t depth)
{
  if (width > svga_regs[SVGA_REG_MAX_WIDTH])
    return RC_Video_NotSupported;

  if (height > svga_regs[SVGA_REG_MAX_HEIGHT])
    return RC_Video_NotSupported;

  /* Ignore requested depth for now.  It looks like VMWare will
     always keep the depth the same as the host operating
     system. */
  VMWRITE( SVGA_REG_ENABLE, 0);
  VMWRITE( SVGA_REG_WIDTH, width);
  VMWRITE( SVGA_REG_HEIGHT, height);

  /* Cache the current registers */
  read_registers();

  VMWRITE( SVGA_REG_ENABLE, 1);

  /* Adjust the framebuffer start address by adding the offset
     stored in the SVGA registers. */
  framebuffer = (uint8_t *)((uint8_t *)(framebuffer_abszero) + 
			     svga_regs[SVGA_REG_FB_OFFSET]);


  return RC_OK;
}

uint32_t
vmware_video_show_cursor_at(uint32_t id, point_t location)
{
  if (id > SVGA_MAX_ID)
    return RC_Video_BadCursorID;

  /* Just in case the client didn't check the pixel: */
  location.x = bound_dim(location.x, 0, svga_regs[SVGA_REG_WIDTH]);
  location.y = bound_dim(location.y, 0, svga_regs[SVGA_REG_HEIGHT]);

  /* Now write the position to the registers */
  VMWRITE( SVGA_REG_CURSOR_ID, id);
  VMWRITE( SVGA_REG_CURSOR_X, location.x);
  VMWRITE( SVGA_REG_CURSOR_Y, location.y);

  /* Display the cursor */
  VMWRITE( SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_SHOW);

  return RC_OK;
}

uint32_t
vmware_video_functionality(void)
{
  uint32_t retval = 0u;

  card_functionality = VMREAD(SVGA_REG_CAPABILITIES);

  /* Now return this value in terms of the common set of
     functionality (i.e. ignore the vmware-specific
     functionality) */
  if (card_functionality & SVGA_CAP_RECT_FILL)
    retval |= VIDEO_RECT_FILL;
      
  if (card_functionality & SVGA_CAP_RECT_COPY)
    retval |= VIDEO_RECT_COPY;
      
  if (card_functionality & SVGA_CAP_RECT_PAT_FILL)
    retval |= VIDEO_RECT_PAT_FILL;
      
  if (card_functionality & SVGA_CAP_OFFSCREEN)
    retval |= VIDEO_OFFSCREEN;
      
  if (card_functionality & SVGA_CAP_RASTER_OP)
    retval |= VIDEO_RASTER_OP;
      
  if (card_functionality & SVGA_CAP_CURSOR)
    retval |= VIDEO_HW_CURSOR;
      
  if (card_functionality & SVGA_CAP_ALPHA_CURSOR)
    retval |= VIDEO_ALPHA_CURSOR;

  return retval;      
}

uint32_t
vmware_video_define_cursor(uint32_t id, point_t hotspot, point_t size,
		    uint32_t depth, 
		    uint8_t *cursor_bits, uint8_t *mask_bits)
{
  uint32_t u;
  uint32_t and[size.x];
  uint32_t xor[size.x];

  if (id > SVGA_MAX_ID)
    return RC_Video_BadCursorID;

  fifo_insert(fifo, SVGA_CMD_DEFINE_CURSOR);
  fifo_insert(fifo, id);
  fifo_insert(fifo, hotspot.x);
  fifo_insert(fifo, hotspot.y);
  fifo_insert(fifo, size.x);
  fifo_insert(fifo, size.y);
  fifo_insert(fifo, depth);
  fifo_insert(fifo, depth);

  for (u = 0; u < size.x; u++) {
    uint32_t bits = (CONVERT_BYTE(cursor_bits[u*2+1]) << 8) |
      (CONVERT_BYTE(cursor_bits[u*2]));

    uint32_t mask = (CONVERT_BYTE(mask_bits[u*2+1]) << 8) |
      (CONVERT_BYTE(mask_bits[u*2]));

    and[u] = 0xFFFF0000 | mask;
    xor[u] = 0x00000000 | bits;

    fifo_insert(fifo, and[u]);
  }

  for (u = 0; u < size.x; u++)
    fifo_insert(fifo, xor[u]);

  return RC_OK;
}

uint32_t vmware_video_define_alpha_cursor(uint32_t id,
					  point_t hotspot,
					  point_t size,
					  uint32_t *bits)
{
  uint32_t u;

  if (id > SVGA_MAX_ID)
    return RC_Video_BadCursorID;

  if (bits == NULL)
    return RC_eros_key_RequestError;

  fifo_insert(fifo, SVGA_CMD_DEFINE_ALPHA_CURSOR);
  fifo_insert(fifo, id);
  fifo_insert(fifo, hotspot.x);
  fifo_insert(fifo, hotspot.y);
  fifo_insert(fifo, size.x);
  fifo_insert(fifo, size.y);

  for (u = 0; u < size.x * size.y; u++)
    fifo_insert(fifo, bits[u]);

  return RC_OK;
}

uint32_t vmware_video_define_vram_pixmap(uint32_t id,
			     point_t dims,
			     uint32_t depth,
			     uint8_t *bits)
{
  uint32_t fifo_count = 0;
  uint32_t line_no;
  uint32_t tmp = 0;
  uint32_t screen_depth = svga_regs[SVGA_REG_BITS_PER_PIXEL];
  uint32_t size = SVGA_PIXMAP_SCANLINE_SIZE(dims.x, depth); 

  uint32_t scanline[size];

  if ((card_functionality & SVGA_CAP_OFFSCREEN) == 0) {
    kprintf(KR_OSTREAM, "###HEY: driver doesn't have OFFSCREEN...\n");
    return RC_Video_NotSupported;
  }

  if (id > SVGA_MAX_ID)
    return RC_Video_BadID;

  /* Repeat for all scanlines */
  for (line_no = 0; line_no < dims.y; line_no++) {
    uint32_t u;
    uint8_t p[3];

    for (u = 0; u < size; u++) {
      HEADER_PIXEL(bits, p);
      scanline[u] = (p[0] << 16) + (p[1] << 8) + p[2];
    }

    fifo_insert(fifo, SVGA_CMD_DEFINE_PIXMAP_SCANLINE);
    fifo_insert(fifo, id);
    fifo_insert(fifo, dims.x);
    fifo_insert(fifo, dims.y);
    fifo_insert(fifo, min(screen_depth, depth));
    fifo_insert(fifo, line_no);

    for (u = 0; u < size; u++) {
      if (screen_depth == 16 && screen_depth < depth) {
	uint16_t converted = makeColor16(scanline[u],
					 svga_regs[SVGA_REG_RED_MASK],
					 svga_regs[SVGA_REG_GREEN_MASK],
					 svga_regs[SVGA_REG_BLUE_MASK]);

	/* We need to pack two 16-bit values into one FIFO word */
	if (u % 2) {
	  tmp |= (uint32_t)converted;
	  fifo_count++;
	  fifo_insert(fifo, tmp);
	}
	else {
	  tmp = ((uint32_t)converted) << 16;

	  /* Here's a hack to make sure we don't leave any odd data
	     out */
	  if (u+1 == (dims.x * (depth / 8)) / sizeof(uint32_t)) {
	    fifo_count++;
	    fifo_insert(fifo, tmp);
	  }
	}
      }
      else {
	fifo_count++;
	fifo_insert(fifo, scanline[u]);
      }
    }
  }

  DEBUG(pixmap) kprintf(KR_OSTREAM, "vmware_svga: definepixmap; "
			"inserted %u words into FIFO\n"
			"     for pixmap of width=%u",
			fifo_count, dims.x);

  return RC_OK;
}


uint32_t vmware_video_show_vram_pixmap(uint32_t id, point_t src, rect_t dst)
{
  uint32_t x = dst.topLeft.x;
  uint32_t y = dst.topLeft.y;
  uint32_t w = (dst.bottomRight.x - dst.topLeft.x);
  uint32_t h = (dst.bottomRight.y - dst.topLeft.y);

  if ((card_functionality & SVGA_CAP_RECT_PAT_FILL) == 0)
    return RC_eros_key_RequestError;

  if (id > SVGA_MAX_ID)
    return RC_Video_BadID;

  /* Note that this is RECT_PIXMAP_COPY and *not*
     RECT_PIXMAP_FILL.  The FILL call seems counterintuitive
     since it makes the pixmap screen aligned. */
  fifo_insert(fifo, SVGA_CMD_RECT_PIXMAP_COPY);
  fifo_insert(fifo, id);
  fifo_insert(fifo, src.x);
  fifo_insert(fifo, src.y);
  fifo_insert(fifo, x);
  fifo_insert(fifo, y);
  fifo_insert(fifo, w);
  fifo_insert(fifo, h);

  return RC_OK;
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
vmware_video_rectfill(rect_t rect, color_t color, uint32_t raster_op)
{
  rect_t screenRect = { {0,0}, 
    			{svga_regs[SVGA_REG_WIDTH], 
    			 svga_regs[SVGA_REG_HEIGHT]}};

  RETURN_IF_BAD_ROP(raster_op);

  /* If this card doesn't have an accelerated version of rectfill we
     exit */
  if ((card_functionality & SVGA_CAP_RECT_FILL) == 0)
    return RC_Video_NotSupported;

  /* Clip against clipRegion and then against the screen */
  if (clipRect(&rect, &screenRect)) {
    if (clipRect(&rect, &screenRect)) {

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
  return RC_OK;
}

bool
vmware_video_copy_to_fb(uint32_t *buffer, rect_t bounds, point_t dst, 
		 uint32_t buffer_width)
{
  uint32_t rows;
  int32_t height = (bounds.bottomRight.y - bounds.topLeft.y);
  int32_t width  = (bounds.bottomRight.x - bounds.topLeft.x);
  uint32_t *from = buffer;
  int32_t offset;
  uint32_t displaywidth;
  point_t xlate = { dst.x - bounds.topLeft.x, dst.y - bounds.topLeft.y };

  /* Wait on any outstanding FIFO commands to complete */
  fifo_sync(fifo);

  /* Remove the cursor before modifying the framebuffer */
  VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_REMOVE_FROM_FB);

  displaywidth = (svga_regs[SVGA_REG_BYTES_PER_LINE] * 8) / 
    ((svga_regs[SVGA_REG_BITS_PER_PIXEL] + 7) & ~7);

  /* If client shoots the bank from whom this buffer was purchased,
     catch the process keeper's signal here and return false. */
  if (fbfault_received())
    return false;
  else {

    for (rows = bounds.topLeft.y; rows < bounds.topLeft.y+height; rows++) {
      uint32_t col = 0;

      for (col = bounds.topLeft.x; col < bounds.topLeft.x+width; col++) {
	  point_t pixel = { col + xlate.x, rows + xlate.y };

	/* First, clip the fb location to the current screen */
	  if (pixel.x > svga_regs[SVGA_REG_WIDTH] || pixel.x < 0 ||
	      pixel.y > svga_regs[SVGA_REG_HEIGHT] || pixel.y < 0)
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

	  if (svga_regs[SVGA_REG_BITS_PER_PIXEL] == 16)
	    ((uint16_t *)framebuffer)[offset] = makeColor16(color,
						svga_regs[SVGA_REG_RED_MASK],
						svga_regs[SVGA_REG_GREEN_MASK],
						svga_regs[SVGA_REG_BLUE_MASK]);
	  else
	    ((uint32_t *)framebuffer)[offset] = from[col + rows*buffer_width];
	}
      }

      //      bcopy(from, &(framebuffer[offset]), width*sizeof(color_t));
      //      from += buffer_width;
    }

    /* Now tell driver to render framebuffer */
    fifo_insert(fifo, SVGA_CMD_UPDATE);
    fifo_insert(fifo, dst.x);
    fifo_insert(fifo, dst.y);
    fifo_insert(fifo, width);
    fifo_insert(fifo, height);

    /* Restore the cursor after modifying the framebuffer */
    VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_RESTORE_TO_FB);
  }

  return true;
}

uint32_t
vmware_video_copy_rect(rect_t src, rect_t dst)
{
  /* Remove the cursor before modifying the framebuffer */
  VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_REMOVE_FROM_FB);

  /* May have to do this by hand */
  if ((card_functionality & SVGA_CAP_RECT_COPY) == 0) {

    kprintf(KR_OSTREAM, "*** VMWare video doesn't support RECT_COPY\n");

    /* Wait on any outstanding FIFO commands to complete */

    /* Manually copy pixel values, then manually update */

    /* FIX: Let someone else write this! */
    return RC_Video_NotSupported;
  }
  else {
    fifo_insert(fifo, SVGA_CMD_RECT_COPY);
    fifo_insert(fifo, src.topLeft.x);
    fifo_insert(fifo, src.topLeft.y);
    fifo_insert(fifo, dst.topLeft.x);
    fifo_insert(fifo, dst.topLeft.y);
    fifo_insert(fifo, dst.bottomRight.x - dst.topLeft.x);
    fifo_insert(fifo, dst.bottomRight.y - dst.topLeft.y);
  }

  /* Restore the cursor after modifying the framebuffer */
  VMWRITE(SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_RESTORE_TO_FB);

  return RC_OK;
}
