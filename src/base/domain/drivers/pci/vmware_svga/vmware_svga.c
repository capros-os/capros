/*
 * Copyright (C) 2002, Jonathan S. Shapiro.
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
#include <eros/i486/io.h>
#include <eros/KeyConst.h>
#include <eros/NodeKey.h>
#include <eros/ProcessKey.h>
#include <eros/StdKeyType.h>
#include <eros/cap-instr.h>

#include <idl/eros/key.h>
#include <idl/eros/DevPrivs.h>
#include <idl/eros/Number.h>

#include <string.h>
#include <stdlib.h>

#include <domain/ConstructorKey.h>
#include <domain/domdbg.h>
#include <domain/Runtime.h>
#include <domain/SpaceBankKey.h>
#include <domain/MemmapKey.h>

/* Every video device driver must implement the Drawable interface,
   the FramebufferManager interface, and the VideoDriverKey
   interface. */
#include <domain/DrawableKey.h>
#include <domain/drivers/VideoDriverKey.h>

#include "svga_reg.h"
#include "constituents.h"
#include "vmware_io.h"
#include "debug.h"
#include "DrawableRequest.h"
#include "fifo.h"

#define min(a,b) ((a) <= (b) ? (a) : (b))

#define KR_OSTREAM   KR_APP(0)
#define KR_DEVPRIVS  KR_APP(1)
#define KR_ADDRSPC   KR_APP(2)
#define KR_SCRATCH   KR_APP(3)
#define KR_FRAMEBUF  KR_APP(4)
#define KR_PHYSRANGE KR_APP(5)
#define KR_NEWPAGE   KR_APP(6)
#define KR_FIFO      KR_APP(7)
#define KR_NEWNODE   KR_APP(8)
#define KR_START_VIDEO     KR_APP(9)
#define KR_START_DRAWABLE  KR_APP(10)
#define KR_START_FBM       KR_APP(11)
#define KR_MEMMAP_C        KR_APP(12)
#define KR_ZERO_SPACE      KR_APP(13)

#define KR_CLIENT_BANK     KR_ARG(0)

#define FIFO_START        0x80000000u   /* 2 GB into address space */
#define FRAMEBUFFER_START 0x88000000u   /* 2.125 GB into address space */

/* This service implements different interfaces. The following macros
   identify the different interfaces and are used to distinguish the
   start keys. */
#define DRIVER_INTERFACE          0x000au
#define DRAWABLE_INTERFACE        0x000bu
#define FRAMEBUFFER_MGR_INTERFACE 0x000cu

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

/* All the cursor commands pertain to hardware cursors */
#define RETURN_IF_NO_HARDWARE_CURSOR \
      if ( !(card_functionality & SVGA_CAP_CURSOR_BYPASS) || \
           !(card_functionality & SVGA_CAP_CURSOR_BYPASS_2) ) { \
	msg->snd_code = RC_Video_NotSupported; \
	return true; \
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

extern rect_t clipRegion; /* defined in drawable_cmds.c */

/* Declare a big buffer for receiving data from invocations */
uint32_t receive_buffer[1024];

/* Following is used to compute 32 ^ lss for patching together address
   space */
#define LWK_FACTOR(lss) (mult(EROS_NODE_SIZE, lss) * EROS_PAGE_SIZE)
static uint32_t
mult(uint32_t base, uint32_t exponent)
{
  uint32_t u;
  int32_t result = 1u;

  if (exponent == 0)
    return result;

  for (u = 0; u < exponent; u++)
    result = result * base;

  return result;
}

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

#if 0
  VMWRITE( SVGA_REG_WIDTH, 800);
  VMWRITE( SVGA_REG_HEIGHT, 600);
#endif

  /* Cache the current registers */
  read_registers();

  /* Establish the start address of the video framebuffer.  This
     changes only when we switch resolutions. */
  framebuffer = (uint8_t *) ((uint8_t *)(framebuffer_abszero) + svga_regs[SVGA_REG_FB_OFFSET]);

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

/* Convenience routine for buying a new node for use in expanding the
   address space. */
static uint32_t
make_new_addrspace(uint16_t lss, fixreg_t key)
{
  uint32_t result = spcbank_buy_nodes(KR_BANK, 1, key, KR_VOID, KR_VOID);
  if (result != RC_OK) {
    DEBUG(video_fb) kprintf(KR_OSTREAM, 
			    "Error: make_new_addrspace: buying node "
			    "returned error code: %u.\n", result);
    return result;
  }

  result = node_make_node_key(key, lss, 0, key);
  if (result != RC_OK) {
    DEBUG(video_fb) kprintf(KR_OSTREAM, 
			    "Error: make_new_addrspace: making node key "
			    "returned error code: %u.\n", result);
    return result;
  }

  return RC_OK;
}

/* Build an "sub" address space for the framebuffer.  Use start
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
  result = constructor_request(KR_MEMMAP_C, KR_BANK, KR_SCHED, KR_VOID,
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
static void
patch_addrspace(void)
{
  eros_Number_value window_key;
  uint32_t next_slot = 0;

  /* Stash the current ProcAddrSpace capability */
  process_copy(KR_SELF, ProcAddrSpace, KR_SCRATCH);

  /* Make a node with max lss */
  make_new_addrspace(EROS_ADDRESS_LSS, KR_ADDRSPC);

  /* Patch up KR_ADDRSPC as follows:
     slot 0 = capability for original ProcAddrSpace
     slots 1-15 = local window keys for ProcAddrSpace
     slot 16 = capability for FIFO
     slot 16 - ?? = local window keys for FIFO, as needed
     remaining slot(s) = capability for FRAMEBUF and any needed window keys
  */
  node_swap(KR_ADDRSPC, 0, KR_SCRATCH, KR_VOID);

  for (next_slot = 1; next_slot < 16; next_slot++) {
    window_key.value[2] = 0;	/* slot 0 of local node */
    window_key.value[1] = 0;	/* high order 32 bits of address
				   offset */

    /* low order 32 bits: multiple of EROS_NODE_SIZE ^ (LSS-1) pages */
    window_key.value[0] = next_slot * LWK_FACTOR(EROS_ADDRESS_LSS-1); 

    DEBUG(video_fifo)
      kprintf(KR_OSTREAM, "vmware_svga: patch_addrspc() inserting local "
	      "window key:\n         slot[%u] with addr = 0x%08x", next_slot,
	      window_key.value[0]);

    /* insert the window key at the appropriate slot */
    node_write_number(KR_ADDRSPC, next_slot, &window_key); 
  }

  next_slot = 16;

  /* Insert FIFO capability. NOTE:  if fifo_lss == EROS_ADDRESS_LSS
     then there won't be room for the framebuffer! */
  DEBUG(video_fifo) {
    kprintf(KR_OSTREAM, "vmware_svga: patch_addrspc() "
	    "inserting FIFO capability (with lss=%u) in slot %u", fifo_lss, 
	    next_slot);
  }

  node_swap(KR_ADDRSPC, next_slot, KR_FIFO, KR_VOID);
  if (fifo_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: vmware_svga(): no room for local window "
	     "keys for FIFO!");

  next_slot++;

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

  node_swap(KR_ADDRSPC, next_slot, KR_FRAMEBUF, KR_VOID);
  if (fb_lss == EROS_ADDRESS_LSS)
    kdprintf(KR_OSTREAM, "** ERROR: vmware_svga(): no room for local window "
	     "keys for FRAMEBUF!");

  next_slot++;

  /* Finally, patch up the ProcAddrSpace register */
  process_swap(KR_SELF, ProcAddrSpace, KR_ADDRSPC, KR_VOID);
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
  result = constructor_request(KR_MEMMAP_C, KR_BANK, KR_SCHED, KR_VOID,
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

/* Set the screen resolution. NOTE: The result for this driver is
   simply to set a "clipping" window that gets centered in full-screen
   mode.  This doesn't really switch the underlying graphics mode. */
static uint32_t
set_resolution(uint32_t width, uint32_t height, uint32_t depth)
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

/* The "DriverRequest()" method is this driver's implementation of the
   VideoDriverKey interface. */
static bool
DriverRequest(Message *msg)
{
  /* Use these two variables to ensure that the message wasn't
     truncated! */
  uint32_t expect;
  uint32_t got;

  DEBUG(video_cmds) kprintf(KR_OSTREAM, "vmware_svga:: received driver request");

  switch (msg->rcv_code) {

  case OC_Video_GetDrawable:
    {
      RETURN_IF_NOT_INITIALIZED;

      msg->snd_key0 = KR_START_DRAWABLE;
      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_Initialize:
    {
      expect = msg->rcv_w3 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(KR_OSTREAM, "vmware_svga:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      /* First, store PCI base address register */
      if (!good_base_address_register(receive_buffer[0], receive_buffer[1])) {
	msg->snd_code = RC_Video_BusError;
	return true;
      }

      /* Then check vmware version */
      if (!good_vmware_version()) {
	msg->snd_code = RC_Video_HWError;
	return true;
      }

      /* Now use memory mapper to fabricate a "sub space" for the
	 SVGA command fifo queue */
      if (fifo_map() != RC_OK) {
	msg->snd_code = RC_Video_MemMapFailed;
	return true;
      }

      /* Now use memory mapper to fabricate a "sub space" for the
	 framebuffer */
      if (framebuffer_map() != RC_OK) {
	msg->snd_code = RC_Video_MemMapFailed;
	return true;
      }

      /* Now patch up our address space accordingly */
      patch_addrspace();

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
      if (card_functionality == 0x0) {
	msg->snd_code = RC_Video_HWInitFailed;
	return true;
      }

      /* Finally, enable the graphics and have some fun! */
      if (graphics_init() != RC_OK) {
	msg->snd_code = RC_Video_HWInitFailed;
	return true;
      }

      /* Initialize the command FIFO queue */
      if (fifo_init(fifo, fifo_size) != RC_OK) {
	msg->snd_code = RC_Video_AccelError;
	return true;
      }

      init_complete();
    }
    break;

  case OC_Video_Shutdown:
    {
      /* FIX:  add code to gracefully terminate the driver domain */
      return false;
    }
    break;

  case OC_Video_SetMode:
    {
      RETURN_IF_NOT_INITIALIZED;

      msg->snd_code = set_resolution(msg->rcv_w1, msg->rcv_w2, msg->rcv_w3);
    }
    break;

  case OC_Video_MaxResolution:
    {
      RETURN_IF_NOT_INITIALIZED;
      msg->snd_w1 = svga_regs[SVGA_REG_MAX_WIDTH];
      msg->snd_w2 = svga_regs[SVGA_REG_MAX_HEIGHT];

      /* NOTE: max resolution is whatever the host system has */
      msg->snd_w3 = svga_regs[SVGA_REG_HOST_BITS_PER_PIXEL];

      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_GetMode:
    {
      RETURN_IF_NOT_INITIALIZED;
      msg->snd_w1 = svga_regs[SVGA_REG_WIDTH];
      msg->snd_w2 = svga_regs[SVGA_REG_HEIGHT];
      msg->snd_w3 = svga_regs[SVGA_REG_BITS_PER_PIXEL];
      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_Functionality:
    {
      uint32_t retval = 0u;

      RETURN_IF_NOT_INITIALIZED;
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
      
      msg->snd_w1 = retval;
      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_DefineCursor:
    {
      uint32_t u;
      uint32_t id, hotx, hoty, w, h;
      uint32_t depthAND, depthXOR;

      RETURN_IF_NOT_INITIALIZED;

      RETURN_IF_NO_HARDWARE_CURSOR;

      id       = receive_buffer[0];
      hotx     = receive_buffer[1];
      hoty     = receive_buffer[2];
      w        = receive_buffer[3];
      h        = receive_buffer[4];
      depthAND = receive_buffer[5];
      depthXOR = receive_buffer[6];

      if (id > SVGA_MAX_ID) {
	msg->snd_code = RC_Video_BadCursorID;
	return true;
      }

      expect = (SVGA_PIXMAP_SIZE(w,h,depthAND) + 
	SVGA_PIXMAP_SIZE(w,h,depthXOR)) * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent) - 7 * sizeof(uint32_t);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(KR_OSTREAM, "vmware_svga:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      fifo_insert(fifo, SVGA_CMD_DEFINE_CURSOR);
      fifo_insert(fifo, id);
      fifo_insert(fifo, hotx);
      fifo_insert(fifo, hoty);
      fifo_insert(fifo, w);
      fifo_insert(fifo, h);
      fifo_insert(fifo, depthAND);
      fifo_insert(fifo, depthXOR);

      for (u = 0; u < (got / sizeof(uint32_t)); u++)
	fifo_insert(fifo, receive_buffer[u+7]);

      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_HideCursor:
    {
      uint32_t id = msg->rcv_w1;

      RETURN_IF_NOT_INITIALIZED;

      RETURN_IF_NO_HARDWARE_CURSOR;

      if (id > SVGA_MAX_ID) {
	msg->snd_code = RC_Video_BadCursorID;
	return true;
      }

      VMWRITE( SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_HIDE);

      if (card_functionality & SVGA_CAP_CURSOR_BYPASS_2)
        VMWRITE( SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_REMOVE_FROM_FB);

      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_ShowCursor:
    {
      uint32_t id = msg->rcv_w1;
      uint32_t x  = msg->rcv_w2;
      uint32_t y  = msg->rcv_w3;

      RETURN_IF_NOT_INITIALIZED;

      RETURN_IF_NO_HARDWARE_CURSOR;

      if (id > SVGA_MAX_ID) {
	msg->snd_code = RC_Video_BadCursorID;
	return true;
      }

      /* Just in case the client didn't check the pixel: */
      x = bound_dim(x, 0, svga_regs[SVGA_REG_WIDTH]);
      y = bound_dim(y, 0, svga_regs[SVGA_REG_HEIGHT]);

      /* Now write the position to the registers */
      VMWRITE( SVGA_REG_CURSOR_ID, id);
      VMWRITE( SVGA_REG_CURSOR_X, x);
      VMWRITE( SVGA_REG_CURSOR_Y, y);

      /* Display the cursor */
      VMWRITE( SVGA_REG_CURSOR_ON, SVGA_CURSOR_ON_SHOW);

      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_DefinePixmapLine:
    {
      uint32_t u;
      uint32_t fifo_count = 0;
      uint32_t id, width, height, depth, line_no;
      uint32_t tmp;
      uint32_t screen_depth = svga_regs[SVGA_REG_BITS_PER_PIXEL];

      if ((card_functionality & SVGA_CAP_OFFSCREEN) == 0) {
        kprintf(KR_OSTREAM, "###HEY: driver doesn't have OFFSCREEN...\n");
	msg->snd_code = RC_Video_NotSupported;
	return true;
      }

      id       = receive_buffer[0];
      width    = receive_buffer[1];
      height   = receive_buffer[2];
      depth    = receive_buffer[3];
      line_no  = receive_buffer[4];

      /* FIX: if message was truncated, then the above values are
	 suspect.  But, the above values are needed for the
	 computation of 'expect'.  Hmmmm... */
      expect = width * (depth / 8);
      got = min(msg->rcv_limit, msg->rcv_sent) - 5 * sizeof(uint32_t);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(KR_OSTREAM, "vmware_svga:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      if (id > SVGA_MAX_ID) {
	msg->snd_code = RC_Video_BadID;
	return true;
      }

      fifo_insert(fifo, SVGA_CMD_DEFINE_PIXMAP_SCANLINE);
      fifo_insert(fifo, id);
      fifo_insert(fifo, width);
      fifo_insert(fifo, height);
      fifo_insert(fifo, min(screen_depth, depth));
      fifo_insert(fifo, line_no);

      for (u = 0; u < (width * (depth / 8)) / sizeof(uint32_t); u++) {
	if (screen_depth == 16 && screen_depth < depth) {
	  uint16_t converted = makeColor16(receive_buffer[u+5],
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
	    if (u+1 == (width * (depth / 8)) / sizeof(uint32_t)) {
	      fifo_count++;
	      fifo_insert(fifo, tmp);
	    }
	  }
	}
	else {
	  fifo_count++;
	  fifo_insert(fifo, receive_buffer[u+5]);
	}
      }

      DEBUG(pixmap) kprintf(KR_OSTREAM, "vmware_svga: definepixmap; "
			    "inserted %u words into FIFO\n"
			    "     for pixmap of width=%u",
			    fifo_count, width);

      msg->snd_code = RC_OK;
    }
    break;

  case OC_Video_RectPixmapFill:
    {
      uint32_t id;
      uint32_t x;
      uint32_t y;
      uint32_t w;
      uint32_t h;

      if ((card_functionality & SVGA_CAP_RECT_PAT_FILL) == 0) {
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      expect = 5 * sizeof(uint32_t);
      got = min(msg->rcv_limit, msg->rcv_sent);

      if (expect != got) {
	DEBUG(msg_trunc) kprintf(KR_OSTREAM, "vmware_svga:: TRUNCATION: "
				 "expect = %u and got = %u.\n", expect, got);
	msg->snd_code = RC_eros_key_RequestError;
	return true;
      }

      id = receive_buffer[0];
      x = receive_buffer[1];
      y = receive_buffer[2];
      w = receive_buffer[3];
      h = receive_buffer[4];

      if (id > SVGA_MAX_ID) {
	msg->snd_code = RC_Video_BadID;
	return true;
      }

      /* Note that this is RECT_PIXMAP_COPY and *not*
	 RECT_PIXMAP_FILL.  The FILL call seems counterintuitive
	 since it makes the pixmap screen aligned. */
      fifo_insert(fifo, SVGA_CMD_RECT_PIXMAP_COPY);
      fifo_insert(fifo, id);
      fifo_insert(fifo, 0);
      fifo_insert(fifo, 0);
      fifo_insert(fifo, x);
      fifo_insert(fifo, y);
      fifo_insert(fifo, w);
      fifo_insert(fifo, h);

      msg->snd_code = RC_OK;
    }
    break;

  default:
    {
      DEBUG(video_cmds) kprintf(KR_OSTREAM, "No such command: 0x%04x.\n",
				msg->rcv_code);
      msg->snd_code = RC_eros_key_RequestError;
      break;
    }
  }
  return true;
}

/* Main processing logic for this "service" */
static bool
ProcessRequest(Message *msg)
{
  /* Dispatch the request based on the invocation interface */
  switch (msg->rcv_keyInfo) {
  case DRIVER_INTERFACE:
    {
      return DriverRequest(msg);
    }
    break;

  case DRAWABLE_INTERFACE:
    {
      return DrawableRequest(KR_OSTREAM, msg);
    }
    break;

  default:
    {
      msg->snd_code = RC_eros_key_UnknownRequest;
    }
    break;
  }
  return true;
}

int main(void)
{
  Message msg;

  bzero(receive_buffer, sizeof(receive_buffer));

  /* Keys can't be used unless they are in process's key register set.
     Copy the needed keys from the constituent node to the key
     register set: */
  node_extended_copy(KR_CONSTIT, KC_OSTREAM, KR_OSTREAM);
  node_extended_copy(KR_CONSTIT, KC_DEVPRIVS, KR_DEVPRIVS);
  node_extended_copy(KR_CONSTIT, KC_PHYSRANGE, KR_PHYSRANGE);
  node_extended_copy(KR_CONSTIT, KC_MEMMAP_C, KR_MEMMAP_C);
  node_extended_copy(KR_CONSTIT, KC_ZERO_SPACE, KR_ZERO_SPACE);

  /* Move the DEVPRIVS key to the ProcIoSpace slot so we can do io calls. */
  process_swap(KR_SELF, ProcIoSpace, KR_DEVPRIVS, KR_VOID);

  /* Make a start key to pass back to constructor.  This key
     implements the VideoDriverKey interface. */
  process_make_start_key(KR_SELF, DRIVER_INTERFACE, KR_START_VIDEO);

  /* Make a start key to pass back to clients.  This key
     implements the Drawable interface. */
  process_make_start_key(KR_SELF, DRAWABLE_INTERFACE, KR_START_DRAWABLE);

  DEBUG(video_init) 
    kprintf(KR_OSTREAM, "main is about to go into available state...\n");

  msg.snd_invKey = KR_RETURN;
  msg.snd_code = 0;
  msg.snd_key0 = KR_START_VIDEO;
  msg.snd_key1 = KR_VOID;
  msg.snd_key2 = KR_VOID;
  msg.snd_rsmkey = KR_RETURN;
  msg.snd_data = send_data;
  msg.snd_len = 0;
  msg.snd_w1 = 0;
  msg.snd_w2 = 0;
  msg.snd_w3 = 0;

  msg.rcv_rsmkey = KR_RETURN;
  msg.rcv_key0 = KR_ARG(0);
  msg.rcv_key1 = KR_ARG(1);
  msg.rcv_key2 = KR_ARG(2);
  msg.rcv_data = receive_buffer;
  msg.rcv_limit = sizeof(receive_buffer);
  msg.rcv_code = 0;
  msg.rcv_w1 = 0;
  msg.rcv_w2 = 0;
  msg.rcv_w3 = 0;

  RETURN(&msg);  /* return start key to constructor */

  DEBUG(video_init) 
    kprintf(KR_OSTREAM, "MAIN getting initialized...\n");

  while (ProcessRequest(&msg)) {
    msg.snd_invKey = KR_RETURN;
    RETURN(&msg);		       
    msg.rcv_data = receive_buffer;
    msg.rcv_limit = sizeof(receive_buffer);
  };

  return 0;  
}
