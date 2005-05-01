/*
 * Copyright (C) 2001, Michael Hilsdale.
 *
 * This file is part of the EROS Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Instructions to use this code
 * =============================
 *
 * 1. call InitVesa()
 * 2. receive a good mode number from ChooseVESA()
 * 3. set that mode number using SetConsoleMode(bi, consoleMode)
 *    which will populate the ConsoleInfo struct inside BootInfo bi
 *
 */

#include <eros/target.h>
#include <eros/i486/io.h>
#include <kerninc/BootInfo.h>
#include "boot.h"
#include "boot-asm.h"
#include "debug.h"

/*************************************************************************/

/* Preferences */
/* =========== */

#define FB_PREF   0              /* Preference to use linear fb (1, default)
				  * or windowed fb (0)
				  * NOTE: This is only a preference.  If the
				  * prefered fb type is not available, the
				  * other type will be used.
				  */

/* Mode Preferences */
/* Refer to comments in ChooseVesa() */
#define maxXpref   1024
#define maxYpref   768
#define maxBPPpref 32

/*************************************************************************/

#define MAX_MODES 29             /* Reasonable guess as to max number
				  * of modes to expect.
				  */

#define BELL 7

/* Display Window Control */
#define SETWIN 0
#define GETWIN 1
#define WINA   0
#define WINB   1

/* Environment Info */
#define UNKNOWN     0
#define VMWARE      1
#define VMWARE_MODE 0x4022
uint8_t environment;

uint8_t VESAinit = 0; /* VESA not initialized */
uint8_t origMode;

/* Supported mode information */
uint8_t numModesSupported = 0;
uint16_t modeList[MAX_MODES];

/* Mode state variables */
uint8_t curBank;
uint8_t bankShift;


/* This is what we want. */
/*************************/
uint8_t *frameBufferBase;
/*************************/


struct Vbe3Block {
  /* This struct is VESA 3.0 compliant.
   * The total size of this struct is 512 bytes.
   */
  char     VESASignature[4];
  uint16_t VESAVersion;
  char     *OEMStringPtr;
  uint32_t Capabilities;
  uint16_t *VideoModePtr;
  uint16_t TotalMemory;
  uint16_t OEMSoftwareRev;
  char     *OEMVendorNamePtr;
  char     *OEMProductNamePtr;
  char     *OEMProductRevPtr;
  char     reserved[222];
  char     reservedOEM[256];
} __attribute((packed));

struct Mode
{
  short int number:9;
  short int reserved:2;
  short int refresh:1;
  short int reserved2:2;
  short int linearmodel:1;
  short int cleardisplay:1;
};

struct ModeInfoBlock
{
  /* This struct is VBE 3.0 compliant */
  uint16_t modeAttributes;
  uint8_t WinAAttributes;
  uint8_t WinBAttributes;
  uint16_t WinGranularity;
  uint16_t WinSize;
  uint16_t WinASegment;
  uint16_t WinBSegment;
  uint32_t WinFuncPtr;
  uint16_t BytesPerScanLine;
  uint16_t XResolution;
  uint16_t YResolution;
  uint8_t XCharSize;
  uint8_t YCharSize;
  uint8_t NumberOfPlanes;
  uint8_t BitsPerPixel;
  uint8_t NumberOfBanks;
  uint8_t MemoryModel;
  uint8_t BankSize;
  uint8_t NumberOfImagePages;
  uint8_t reserved;
  uint8_t RedMaskSize;
  uint8_t RedFieldPosition;
  uint8_t GreenMaskSize;
  uint8_t GreenFieldPosition;
  uint8_t BlueMaskSize;
  uint8_t BlueFieldPosition;
  uint8_t RsvdMaskSize;
  uint8_t RsvdFieldPosition;
  uint8_t DirectColorModeInfo;
  uint32_t PhysBasePtr; /* physical address for flat memory frame buffer */
  uint32_t reserved2; /* VBE 2.0 spec uses this for *OffScreenMemOffset */
  uint16_t reserved3; /* VBE 2.0 spec uses this for OffScreenMemSize */
  uint16_t LinBytesPerScanLine;
  uint8_t BnkNumberOfImagePages;
  uint8_t LinNumberOfImagePages;
  uint8_t LinRedMaskSize;
  uint8_t LinRedFieldPosition;
  uint8_t LinGreenMaskSize;
  uint8_t LinGreenFieldPosition;
  uint8_t LinBlueMaskSize;
  uint8_t LinBlueFieldPosition;
  uint8_t LinRsvdMaskSize;
  uint8_t LinRsvdFieldPosition;
  uint32_t MaxPixelClock;
  uint8_t reserved4[189]; /* Shouldn't this be 190? The spec says 189. */
} __attribute((packed));

struct CRTCInfo
{
  uint16_t HorizontalTotal;
  uint16_t HorizontalSyncStart;
  uint16_t HorizontalSyncEnd;
  uint16_t VerticalTotal;
  uint16_t VerticalSyncStart;
  uint16_t VerticalSyncEnd;
  uint8_t flags;
  uint32_t PixelClock;
  uint16_t RefreshRate;
  char Reserved[40];
};

struct ModeAttributes
{
  /* ISSUE: these need to be defined using uint16_t for alignment
     reasons. */
  uint16_t hw:1;
  uint16_t resv:1;
  uint16_t tty:1;
  uint16_t color:1;
  uint16_t graphics:1;
  uint16_t notvga:1;
  uint16_t nowin:1;
  uint16_t linfb:1;
  uint16_t dbscan:1;
  uint16_t ilace:1;
  uint16_t tpbuf:1;
  uint16_t stereo:1;
  uint16_t dual:1;
  uint16_t resv2:3;
};

enum WinAttributes {
  wa_relocatable = 0x1u,
  wa_readable    = 0x2u,
  wa_writeable   = 0x4u,
} ;

enum MemModels {
  mm_Text = 0,
  mm_CGA  = 1,
  mm_Hercules    = 2,
  mm_Planar      = 3,
  mm_PackedPixel = 4,
  mm_NonChain    = 5,		/* non-chain 4, 256 color */
  mm_DirectColor = 6,
  mm_YUV         = 7,
  /* 0x08-0x0f reserved for future VESA standards */
  /* 0x10-0xff reserved for OEM-defined modes */
};

/* Global structs */
struct Vbe3Block Vbe3InfoBlock;
struct ModeInfoBlock modeInfoBlock;
struct ModeAttributes modeAttributes;
struct CRTCInfo CRTCInfoBlock;

/* External functions */

/* Located in hibios.S */
#define AsmGetVgaMode() GetDisplayMode()
#define AsmSetVgaMode(x) SetDisplayMode(x)
extern unsigned SetDisplayMode(uint8_t mode);

/* Located in vesa.S */
extern unsigned AsmGetVbeControllerInfo(struct Vbe3Block *);
extern unsigned AsmGetVbeModeInfo(uint16_t mode, struct ModeInfoBlock *);
extern unsigned AsmSetVbeMode(uint16_t mode, struct CRTCInfo *);
extern unsigned AsmReturnCurrentVbeMode();
extern unsigned AsmDisplayWindowControl(uint8_t getset, uint8_t AorB, uint16_t winNum);

/* Local functions */

void     getVbe3Info();                           /* uses assembly magic */
void     setVbe3Info();
uint8_t  findEnvironment();
#if 0
void     printVbe3Info();
void     printCapabilities();
#endif
void     printVideoModes();
void     getVideoModes();
void     printCurrentModeInfo();
void     getModeInfo(const uint16_t modeNumber);  /* uses assembly magic */
uint8_t  setMode(const uint16_t modeNumber);      /* uses assembly magic */
/* Issue: The entire #define mechanism is actually quite
   disgusting. No. Even more disgusting than *that*. */
#define  handleReturn(s)  moreHelpfulHandleReturn(#s, s)
uint8_t  moreHelpfulHandleReturn(const char * str, const uint16_t retVal);
void     testGraphics();
void     ChezPaul();
void     vertline();
void     horizline();
void     dabble();                                /* pretty pattern */
void     putPixel(const uint16_t x, const uint16_t y, const uint32_t color);
void     setBank(const uint16_t bank);            /* uses assembly magic */
uint16_t getCurrentMode();                        /* uses assembly magic */
void     line(int x1, int y1, int x2, int y2, int color); /* copied */
void     drawMoire();                             /* copied */
void     test();
void     SetConsoleMode(BootInfo *bi, uint32_t mode);

/*************************************************************************/

unsigned
GetVbeControllerInfo(struct Vbe3Block *info)
{
  unsigned result = AsmGetVbeControllerInfo(info);

#define REAL2PA(x) \
    ( ((((unsigned long)(x)) >> 12) & 0xffff0u) + \
      (((unsigned long) (x)) & 0xffffu) )

#define REAL2BOOT(x, ty) PA2BOOT( REAL2PA(x), ty)

  /* The Vbe3InfoBlock contains various pointers.  Because we used the
   * int 0x10 interface, the returned pointers are real mode pointers
   * of the form 0xsssspppp, where ssss is the segment value. We need
   * to convert these pointer into values that can be used directly
   * from the bootstrap code....
   */

  info->VideoModePtr = 
    REAL2BOOT(info->VideoModePtr, uint16_t *);

  info->OEMStringPtr =
    REAL2BOOT(info->OEMStringPtr, char*);

  info->OEMVendorNamePtr =
    REAL2BOOT(info->OEMVendorNamePtr, char*);

  info->OEMProductNamePtr =
    REAL2BOOT(info->OEMProductNamePtr, char *);

  info->OEMProductRevPtr =
    REAL2BOOT(info->OEMProductRevPtr, char *);

  return result;
}

uint8_t
InitVESA()
{
  char sig[5];

  printf("Starting VESA support...");

  origMode = AsmGetVgaMode();

  /* get ready to tell card to use VESA support */
  memset(&Vbe3InfoBlock, 0, sizeof(Vbe3InfoBlock));
  strcpy(Vbe3InfoBlock.VESASignature,"VBE2"); /* REQUIRED EVEN WHEN NOT DEBUGGING */

  /* query card for VESA information */
  getVbe3Info();

  /* test for VESA compliance */
  memcpy(sig, Vbe3InfoBlock.VESASignature, 4);
  sig[4] = 0;
  if(strcmp(sig,"VESA"))
    {
      printf("Failed!\n");
      return 1;  /* no VESA support */
    }
  else
    printf("Successful!\n");

  VESAinit = 1; /* VESA is supported and initialized */
  environment = findEnvironment();

  return 0;
}

uint32_t
ChooseVESA()
{
  uint8_t a;
  uint16_t b;

  struct {
    uint16_t mode;
    uint16_t x;
    uint16_t y;
    uint16_t bpp;
  } best;

  best.mode = 0;
  best.x = 0;
  best.y = 0;
  best.bpp = 0;

  if(!VESAinit)
    {
      printf("ERROR: Either VESA was not initialized or is not supported.\nAborting....");
      waitkbd();
      printf("\n");
      return 0;
    }

  /*printVbe3Info();
  waitkbd();*/
  /*testGraphics();*/

  /* Find a suitable mode */
  /* Current logic: Find the highest possible supported resolution
   * that is less than or equal to the prefered resolution.
   * If none is found, this function will return 0, and a text
   * mode should be used instead of a graphics mode.
   * Caveat: If the prefered bit depth is lower than those
   * available, then no mode will be chosen.  (For example, if you
   * want 1024x768x24, but only 1024x768x32 is available, no
   * graphics mode will be selected.)
   */
  for(a = 0; a < numModesSupported; a++)
    {
      getModeInfo(modeList[a]);
      if(modeInfoBlock.XResolution <= maxXpref && modeInfoBlock.XResolution >= best.x)
	if(modeInfoBlock.YResolution <= maxYpref && modeInfoBlock.YResolution >= best.y)
	  if(modeInfoBlock.BitsPerPixel <= maxBPPpref && modeInfoBlock.BitsPerPixel >= best.bpp)
	    {
	      /* FIXME:
	       * If no linfb available, don't use graphic mode since console doesn't support it yet. */
#if 0
      	      if(modeAttributes.linfb)
		{
#endif
		  best.mode = modeList[a];
		  best.x = modeInfoBlock.XResolution;
		  best.y = modeInfoBlock.YResolution;
		  best.bpp = modeInfoBlock.BitsPerPixel;
#if 0
		}
#endif
	    }
    }

  printf("modes supported: %d\n", numModesSupported);
  for(b = 0; b < numModesSupported; b++)
    {
      getModeInfo(modeList[b]);
      printf("0x%x: %4d x %4d x %2d ", modeList[b], modeInfoBlock.XResolution,
	     modeInfoBlock.YResolution, modeInfoBlock.BitsPerPixel);
      printf((modeAttributes.linfb ? "linfb" : "windowed"));
      printf("\n");
    }

  getModeInfo(best.mode);
  if(best.mode)
    {
      printf("Will use graphics mode 0x%x: %4d x %4d x %2d\n", best.mode, best.x, best.y, best.bpp);
      printCurrentModeInfo();
      waitkbd();
    }

  /* DEBUG info: */
  /*
  getModeInfo(best.mode);
  printf("Desired video mode:\n");
  printf("  X   : %d\n",maxXpref);
  printf("  Y   : %d\n",maxYpref);
  printf("  BPP : %d\n\n",maxBPPpref);
  printf("Best video mode found:\n");
  printf("  Mode: 0x%x\n",best.mode);
  printf("  X   : %d\n",best.x);
  printf("  Y   : %d\n",best.y);
  printf("  BPP : %d\n\n",best.bpp);
  printCurrentModeInfo();
  waitkbd();
  */

  return best.mode;

  /* if(environment == VMWARE)
    {
      setMode(VMWARE_MODE);
      return VMWARE_MODE;
    }
  else
    {
      AsmSetVgaMode(origMode);
      return origMode;
    } */
}

void
getVbe3Info()
{
  /* Get VBE information */

  /* Required registers:
   *   AX    = 4F00h  VBE function number (Return VBE Controller Information)
   *   ES:DI =        pointer to buffer of VbeInfoBlock struct
   *
   * Output:
   *   AX    = should equal 4Fh if successful
   */

  handleReturn(GetVbeControllerInfo(&Vbe3InfoBlock));
  getVideoModes();
}

#if 0
void
setVbe3Info()
{
  /* Use this function for debug only. */

  char *oemString,*vendorName,*productName,*productRev;

  printf("Setting sample VBE3 data...\n");

  oemString = &Vbe3InfoBlock.reservedOEM[0];
  strcpy(oemString,"Test OEM String");
  vendorName = &Vbe3InfoBlock.reservedOEM[strlen(oemString)+1];
  strcpy(vendorName,"Vendor Company Name");
  productName = &Vbe3InfoBlock.reservedOEM[strlen(oemString)+strlen(vendorName)+2];
  strcpy(productName,"ACME Graphics");
  productRev = &Vbe3InfoBlock.reservedOEM[strlen(oemString)+strlen(vendorName)+strlen(productName)+3];
  strcpy(productRev,"0.1 beta");

  strcpy(Vbe3InfoBlock.VESASignature,"VBE2"); /* REQUIRED EVEN WHEN NOT DEBUGGING */

  Vbe3InfoBlock.VESAVersion = 0x3D2;
  Vbe3InfoBlock.OEMStringPtr = "Test OEM string";
  Vbe3InfoBlock.Capabilities = 0;
  Vbe3InfoBlock.VideoModePtr = 0;
  Vbe3InfoBlock.TotalMemory = 1024;
  Vbe3InfoBlock.OEMSoftwareRev = 0xFFFF;
  Vbe3InfoBlock.OEMVendorNamePtr = vendorName;
  Vbe3InfoBlock.OEMProductNamePtr = productName;
  Vbe3InfoBlock.OEMProductRevPtr = productRev;
}
#endif

uint8_t
findEnvironment()
{
  if(!strcmp(Vbe3InfoBlock.OEMStringPtr,"VMware, Inc. SVGA"))
    return VMWARE;

  return UNKNOWN;
}

#if 0
void
printVbe3Info()
{
  char vbeString[5];

  memcpy(vbeString, Vbe3InfoBlock.VESASignature, 4);
  vbeString[4] = 0;

  printf("VESA3 Communications Test\n");
  printf("=========================\n\n");
  printf("VESA signature   : %s\n", vbeString);
  printf("VESA version     : %d.%d\n",Vbe3InfoBlock.VESAVersion >> 8,
	 Vbe3InfoBlock.VESAVersion & 0xFF);
  printf("OEM string       : %s\n", Vbe3InfoBlock.OEMStringPtr);
  printf("Capabilities Word: 0x%x\n", Vbe3InfoBlock.Capabilities);
  printCapabilities();
  printf("Video Modes Ptr  : 0x%x\n", Vbe3InfoBlock.VideoModePtr);
  printVideoModes();
  printf("Total memory     : %d 64 kB chunks (%d bytes)\n", Vbe3InfoBlock.TotalMemory,
	 (Vbe3InfoBlock.TotalMemory * 65536));
  printf("OEM Software Rev : %d.%d\n", Vbe3InfoBlock.OEMSoftwareRev >> 8,
	 Vbe3InfoBlock.OEMSoftwareRev & 0xFF);
  printf("OEM Vendor Name  : %s\n", Vbe3InfoBlock.OEMVendorNamePtr);
  printf("OEM Product Name : %s\n", Vbe3InfoBlock.OEMProductNamePtr);
  printf("OEM Product Rev  : %s\n", Vbe3InfoBlock.OEMProductRevPtr);
}

void
printCapabilities()
{
  long caps = Vbe3InfoBlock.Capabilities;
  short int dac,vga,ramdac,stereo,signaling;

  dac = (caps<<31)>>31;
  vga = (caps<<30)>>31;
  ramdac = (caps<<29)>>31;
  stereo = (caps<<28)>>31;
  signaling = (caps<<27)>>31;

  if(dac)
    printf("  DAC width is switchable to 8 bits per primary color.\n");
  else
    printf("  DAC is fixed width, with 6 bits per primary color.\n");

  if(vga)
    printf("  Controller is not VGA compatible.\n");
  else
    printf("  Controller is VGA compatible.\n");

  if(ramdac)
    printf("  Abnormal (older) RAMDAC operation (refer to VESA spec).\n");
  else
    printf("  Normal RAMDAC operation.\n");

  if(stereo) {
    printf("  Hardware stereoscopic signaling supported by controller.\n");
    if(signaling)
      printf("    Stereo signaling suported via VESA EVC connector.\n");
    else
      printf("    Stereo signaling supported via external VESA stereo connector.\n");
  }

  else
    printf("  No hardware stereoscopic signaling support.\n");
}
#endif

void
printModeAttr(uint16_t attrs)
{
  struct ModeAttributes attrib;
  attrib = *((struct ModeAttributes *) &attrs);

  printf ("    %s %s %s %s %s %s %s %s %s %s %s %s\n",
	  (attrib.hw) ? "hw" : "not-hw",
	  (attrib.tty) ? "tty" : "notty",
	  (attrib.color) ? "clr" : "mno",
	  (attrib.graphics) ? "gfx" : "txt",
	  (attrib.notvga) ? "novga" : "vga",
	  (attrib.nowin) ? "novgawin" : "vgawin",
	  (attrib.linfb) ? "fb" : "nofb",
	  (attrib.dbscan) ? "dscan" : "nodscan",
	  (attrib.ilace) ? "ilace" : "noilace",
	  (attrib.tpbuf) ? "3buf" : "no3buf",
	  (attrib.stereo) ? "3d" : "no3d",
	  (attrib.dual) ? "2dsp" : "no2dsp");

  /*printf ("%s %s %s %s %s %s %s %s %s %s %s %s\n",
	  (attrs & 1u) ? "hw" : "not-hw",
	  (attrs & (1u << 2)) ? "tty" : "notty",
	  (attrs & (1u << 3)) ? "clr" : "mno",
	  (attrs & (1u << 4)) ? "gfx" : "txt",
	  (attrs & (1u << 5)) ? "novga" : "vga",
	  (attrs & (1u << 6)) ? "novgawin" : "vgawin",
	  (attrs & (1u << 7)) ? "fb" : "nofb",
	  (attrs & (1u << 8)) ? "dscan" : "nodscan",
	  (attrs & (1u << 9)) ? "ilace" : "noilace",
	  (attrs & (1u << 10)) ? "3buf" : "no3buf",
	  (attrs & (1u << 11)) ? "3d" : "no3d",
	  (attrs & (1u << 12)) ? "2dsp" : "no2dsp");*/
}

void
getVideoModes()
{
  int counter = 0;
  uint16_t *vmptr;

  vmptr = Vbe3InfoBlock.VideoModePtr;

  for (counter = 0; counter < MAX_MODES; counter++)
    {
    modeList[counter] = vmptr[counter];

    if (vmptr[counter] == 0xffffu || vmptr[counter] == 0)
      break;
  }
  numModesSupported = counter;
}

void
printCurrentModeInfo()
{
  printf("%4d x %4d %2d bits per pixel\n",modeInfoBlock.XResolution,modeInfoBlock.YResolution,modeInfoBlock.BitsPerPixel);

  printModeAttr(modeInfoBlock.modeAttributes);
  printf("    modeAttr 0x%04x LinearFB: (%c) pa=0x%08x memModel=0x%x r.g.b %d.%d.%d\n", 
	 modeInfoBlock.modeAttributes,
	 (modeInfoBlock.modeAttributes & (1u << 7)) ? 'y' : 'n', 
	 modeInfoBlock.PhysBasePtr,
	 modeInfoBlock.MemoryModel,
	 modeInfoBlock.RedMaskSize,
	 modeInfoBlock.GreenMaskSize,
	 modeInfoBlock.BlueMaskSize);
  printf("    redFieldPos %d greenFieldPos %d blueFieldPos %d\n", 
	 modeInfoBlock.RedFieldPosition,
	 modeInfoBlock.GreenFieldPosition,
	 modeInfoBlock.BlueFieldPosition);
  printf("    winGran 0x%x winSz 0x%x winASeg 0x%08x winBSeg 0x%08x\n", 
	 modeInfoBlock.WinGranularity, modeInfoBlock.WinSize,
	 modeInfoBlock.WinASegment, modeInfoBlock.WinBSegment);
  printf("    winAAttr 0x%x winBAttr 0x%x\n", 
	 modeInfoBlock.WinAAttributes, modeInfoBlock.WinBAttributes);

  printf("    numPlanes %d MikeLinFB? %c bytesPerScanLine %d\n", modeInfoBlock.NumberOfPlanes, modeAttributes.linfb ? 'y' : 'n', modeInfoBlock.BytesPerScanLine);
}

void
printVideoModes()
{
  int counter = 0;

  if(!numModesSupported)
    {
      printf("ERROR: Running printVideoModes() function without modeList defined.\nRun getVideoModes() first.\n");
      waitkbd();
      return;
    }

  printf("  The following video mode numbers are supported by this controller:\n  ");
  for (counter = 0; counter < numModesSupported; counter++)
    {
    printf("0x%04x ", modeList[counter]);
    if (counter % 8 == 7)
      printf("\n  ");
  }
  printf("\n");
  waitkbd();

  for (counter = 0; counter < numModesSupported; counter++) {
    getModeInfo(modeList[counter]);
      
    printf("    Mode 0x%04x: ", modeList[counter]);

    printCurrentModeInfo();

    if (modeList[counter] == 0xffffu || modeList[counter] == 0)
      break;

    if (counter % 4 == 3) {
      printf("Press any key to continue...");
      waitkbd();
      printf("\n");
    }
  }

  printf("  This controller supports %d possible modes.\n",counter);
  printf("Press any key to continue...");
  waitkbd();
  printf("\n");
}

void
getModeInfo(uint16_t modeNumber)
{
  /* Get VBE Mode Information */

  /* Call VBE function 01h - Return VBE Mode Information
   * Required registers to setup:
   *   AX    = 4F01h
   *   CX    = modeNumber
   *   ES:DI = pointer to modeInfoBlock structure
   *
   * Output:
   *   AX    = should equal 4Fh if successful
   */

  memset(&modeInfoBlock, 0, sizeof(modeInfoBlock));

  /*printf("Getting mode information for mode 0x%x....\n", modeNumber);*/

  /* do assembly magic here */
  handleReturn(AsmGetVbeModeInfo(modeNumber, &modeInfoBlock));

  /* VBE1.1: packed pixel. Convert expression to VBE3 conventions. Theoretically, this 
     should only happen in version 1.1, but...*/
  if (modeInfoBlock.MemoryModel == 0x4 && Vbe3InfoBlock.VESAVersion == 0x101) { 
    switch(modeInfoBlock.BitsPerPixel) {
    case 16:
      modeInfoBlock.RedMaskSize = 5;
      modeInfoBlock.GreenMaskSize = 5;
      modeInfoBlock.BlueMaskSize = 5;
      /* Following are GUESSES: */
      modeInfoBlock.RedFieldPosition = 10;
      modeInfoBlock.GreenFieldPosition = 5;
      modeInfoBlock.BlueFieldPosition = 0;

      modeInfoBlock.MemoryModel = 0x6;
      break;
    case 24:
    case 32:
      modeInfoBlock.RedMaskSize = 8;
      modeInfoBlock.GreenMaskSize = 8;
      modeInfoBlock.BlueMaskSize = 8;
      /* Following are GUESSES: */
      modeInfoBlock.RedFieldPosition = 16;
      modeInfoBlock.GreenFieldPosition = 8;
      modeInfoBlock.BlueFieldPosition = 0;

      modeInfoBlock.MemoryModel = 0x6;
      break;
    }
  }

  frameBufferBase = (uint8_t *) modeInfoBlock.PhysBasePtr;
  modeAttributes = *((struct ModeAttributes *) &modeInfoBlock.modeAttributes);

  /* Override linfb setting with preference if possible */
  modeAttributes.linfb = modeAttributes.nowin || (modeAttributes.linfb && FB_PREF);
  /* note that (linfb == 0 && nowin == 1) can never occur according to VESA spec */
  /* linfb nowin pref  final
     =======================
     0     0     0     0      final = 0 : use windowed fb
     1     0     0     0      final = 1 : use linear fb
     1     1     0     1
     0     0     1     0
     1     0     1     1
     1     1     1     1
   */
}

uint8_t
setMode(const uint16_t modeNumber)
{
  /* Set VBE mode */

  /* Put the system into a nice VBE mode, 1024x768x64k.
   * This corresponds to VBE mode 117h.
   * Required registers to setup:
   *   AX    = 4F02h VBE function number (Set VBE Mode)
   *   BX    =       desired mode
   *      D0-D8    = 117h  mode number (117h=0x100010111)
   *      D9-D10   = 0     VBE reserved
   *      D11      = 0     use current default refresh rate
   *      D12-D13  = 0     VBE reserved
   *      D14      = 1     use linear frame buffer model (0=windowed)
   *      D15      = 0     clear display memory
   *                 Thus, BX=0x0100000100010111=0x4117
   *   ES:DI =       can safely ignore since we are not specifying
   *                 our own refresh rate
   * Output:
   *   AX    = should equal 4Fh if successful
   */

  uint16_t setMode;
  struct Mode mode;
  uint8_t a;

  /* Let's play safely... */
  for(a = 0; a < numModesSupported; a++)
    {
      if(modeList[a] == modeNumber)
	break;
    }
  if(modeList[a] != modeNumber)
    {
      printf("Bad mode number!\n");
      return 1;
    }

  getModeInfo(modeNumber);

  mode.number       = modeNumber;
  mode.reserved     = 0;          /* must be 0 */
  mode.refresh      = 0;          /* 0 = current BIOS default refresh
				   * 1 = user specified CRTC values
				   */
  mode.reserved2    = 0;          /* must be 0 */
  mode.linearmodel  = modeAttributes.linfb;
                                  /* 0 = use banked/windowed frame buffer
				   * 1 = use linear/flat frame buffer
				   */
  mode.cleardisplay = 0;          /* 0 = clear display memory
				   * 1 = preserve display memory
				   */

  setMode = *((uint16_t *) &mode); /* Ugly.  This just grabs an int version of the struct. */

  printf("Trying to change to mode 0x%x",modeNumber);
  if(modeAttributes.linfb)
    printf(" with a linear frame buffer....\n");
  else
    printf(" with a windowed frame buffer....\n");

  /* do assembly magic here */
  if(!handleReturn(AsmSetVbeMode(setMode, 0)))
    {
      /* update mode state information */
      bankShift = 0;

      while ((unsigned)(64 >> bankShift) != modeInfoBlock.WinGranularity)
	bankShift++;

      /*while(((uint16_t) 1 << bankShift) != modeInfoBlock.WinGranularity)
	bankShift++;*/
      /*printf("Gran: %d Shift: %d\n",modeInfoBlock.WinGranularity, bankShift);
	waitkbd();*/
      curBank = -1;

      /* TODO: double-check logic in here to automatically use lin fb or win fb appropriately */
      if (modeAttributes.linfb) {
	//printf("Using physical base pointer 0x%08x per linfb bit\n", modeInfoBlock.PhysBasePtr);
	frameBufferBase = (PA2BOOT(modeInfoBlock.PhysBasePtr, uint8_t *));
      }
      else {
	//printf("winAseg at 0x%x\n",modeInfoBlock.WinASegment * 16);
	frameBufferBase = (PA2BOOT(modeInfoBlock.WinASegment * 16, uint8_t *));
      }

      /*printf("using fb at 0x%x\n",frameBufferBase);
	waitkbd();*/

      /*if(getCurrentMode() != modeNumber)
	printf("%c%c%c%c%c%c",BELL,BELL,BELL,BELL,BELL,BELL);
      else
      printf("%c%c%c",BELL,BELL,BELL);*/

      return 0;
    }
  return 1;
}

uint8_t 
moreHelpfulHandleReturn(const char *str, const uint16_t retVal)
{
  if (retVal == 0x4f)
    return 0;

  /* Issue: It's actually quite useful to be able to SEE the
     diagnostics :-) */
  AsmSetVgaMode(origMode);

  printf("Invocation: %s\n", str);
#if 0
  if((retVal & 0xFF) == 0x4F)
    printf("Function was supported, ");
  else
    printf("Function was NOT supported, ");
#else
  printf("Function was NOT supported, ");
#endif

  switch(retVal >> 8) {
  case 0x0:
    printf("and function call was successful.\n");
    break;

  case 0x1:
    printf("and function call failed.\n");
    break;

  case 0x2:
    printf("and function is not supported in the current hardware configuration.\n");
    break;

  case 0x3:
    printf("and function call is invalid in current video mode.\n");
    break;

  default: 
    printf("and an unknown return value was received.\n");
  }

  waitkbd();

  return (retVal!=0x4f);
}

void
testGraphics()
{
  uint32_t a;

  printf("Beginning test of %d graphics modes....\n",numModesSupported);

  for(a=0; a<numModesSupported; a++)
  {
      AsmSetVgaMode(origMode);

      getModeInfo(modeList[a]);

      /* Is it a graphics mode? */
      if(!modeAttributes.graphics)    
	continue;

      /* Issue: mode may not be available in current hardware
         configuration! */
      if (!modeAttributes.hw)	
	continue;

      if (modeInfoBlock.MemoryModel != mm_DirectColor) /* Not a direct color mode */
	continue;

      printCurrentModeInfo();

      printf("Press any key to test...\n");
      waitkbd();

      setMode(modeList[a]);

      ChezPaul();
      horizline();
      waitkbd();

      ChezPaul();
      vertline();
      waitkbd();

      ChezPaul();
      dabble();
      /* Issue: beeping is rude.
	 printf("%c", BELL);
      */
      waitkbd();

      //test();
      ChezPaul();
      drawMoire();
      /* Issue: beeping is rude.
	 printf("%c", BELL);
      */
      waitkbd();
    }
  /*printf("%c%c",BELL,BELL);
    waitkbd();*/
}

void ChezPaul()
{
#define BLACKEN_POINTWISE
#ifdef BLACKEN_POINTWISE
  uint32_t x, y;

  /* Let's draw a pretty pattern on the screen.... */
  for (x = 0; x < modeInfoBlock.XResolution; x++) {
    for (y = 0; y < modeInfoBlock.YResolution; y++) {
      putPixel(x, y, 0);
    }
  }
#else /* High temperature is the preferred method for blackening. */
  uint32_t offset = 0;
  uint32_t totBytes = modeInfoBlock.YResolution * modeInfoBlock.BytesPerScanLine;

  uint32_t max = modeAttributes.linfb ? totBytes : 65536;

  while (totBytes) {
    uint32_t cur = min(max, totBytes);

    if (!modeAttributes.linfb)
      setBank(offset >> 16);

#if 0
    {
      int i;
      for (i = 0; i < cur / sizeof(uint32_t); i++) 
	((uint32_t *) frameBufferBase)[i] = 0;
    }
#else
    memset(frameBufferBase, 0, cur);
#endif

    totBytes -= cur;
    offset += cur;
  }

  curBank = -1;
#endif
}

void vertline()
{
  uint32_t a;
  uint32_t row = 120;
  uint32_t white = ~0u;
  uint32_t red = ~0u & ((1u << modeInfoBlock.RedMaskSize) - 1);
  uint32_t green = ~0u & ((1u << modeInfoBlock.GreenMaskSize) - 1);
  uint32_t blue = ~0u & ((1u << modeInfoBlock.BlueMaskSize) - 1);

  red <<= modeInfoBlock.RedFieldPosition;
  blue <<= modeInfoBlock.BlueFieldPosition;
  green <<= modeInfoBlock.GreenFieldPosition;

  /* Let's draw a pretty pattern on the screen.... */
  for(a = 0; a < modeInfoBlock.YResolution; a++) {
    unsigned i;
    unsigned width = 4;
    for (i = 0; i < width; i++) {
      putPixel(row + i + 0 * width, a, red);
      putPixel(row + i + 1 * width, a, green);
      putPixel(row + i + 2 * width, a, blue);
      putPixel(row + i + 3 * width, a, white);
    }
  }
}

void horizline()
{
  uint32_t a;

  uint32_t col = 120;
  uint32_t white = ~0u;
  uint32_t red = ~0u & ((1u << modeInfoBlock.RedMaskSize) - 1);
  uint32_t green = ~0u & ((1u << modeInfoBlock.GreenMaskSize) - 1);
  uint32_t blue = ~0u & ((1u << modeInfoBlock.BlueMaskSize) - 1);
  red <<= modeInfoBlock.RedFieldPosition;
  blue <<= modeInfoBlock.BlueFieldPosition;
  green <<= modeInfoBlock.GreenFieldPosition;


  /* Let's draw a pretty pattern on the screen.... */
  for(a = 0; a < modeInfoBlock.XResolution; a++) {
    unsigned i;
    unsigned width = 4;
    for (i = 0; i < width; i++) {
      putPixel(a, col + i + 0 * width, red);
      putPixel(a, col + i + 1 * width, green);
      putPixel(a, col + i + 2 * width, blue);
      putPixel(a, col + i + 3 * width, white);
    }
  }
}

void dabble()
{
  uint32_t a, b;
  uint32_t clr = 0;
  uint8_t bpp = modeInfoBlock.BitsPerPixel;
  uint32_t maxc = 1 << (bpp - 1);

  /* Let's draw a pretty pattern on the screen.... */

  /* shap: Actually, the picture is not so pretty if the total number
     of colors is not evenly divisible by the horizontal bits per
     line. */
  for(a = 0; a < modeInfoBlock.YResolution; a++)
    {
      for(b = 0; b < modeInfoBlock.XResolution; b++)
	{
	  putPixel(b, a, clr);
	  clr = (clr == maxc ? clr = 0 : ++clr);
	}
    }
}

void putPixel(const uint16_t x, const uint16_t y, const uint32_t color)
{
  /* Issue: if bits per pixel is not a multiple of 8, then the naive
     computation fails. */
  uint32_t offset = 
    (uint32_t) y * modeInfoBlock.BytesPerScanLine 
    + x * ((modeInfoBlock.BitsPerPixel + 7) / 8);

  /* Assume 64k banks for the moment */
  if(!modeAttributes.linfb) {
    setBank(offset >> 16);
    
    /* Note: general thing here would be offset -= (curBank * BankSize); */
    offset &= 0xffffu;
  }

  /* Logically: effectiveAddress = frameBufferBase + (BankOffset + BankRelativeOffset); */


  /* Issue: absence of funny cases (like 15 bits per pixel) was
     causing drawing to be entirely suppressed. */

  /* *(frameBufferBase + (offset & 0xFFFF)) = (char) color; */
  if (modeInfoBlock.BitsPerPixel <= 8) {
    /* 7, 8 bit modes use bytes */
    *((uint8_t *) (frameBufferBase + offset)) = color;
  }
  else if (modeInfoBlock.BitsPerPixel <= 16) {
    /* 15, 16 bit modes use 16 bit values */
    *((uint16_t *) (frameBufferBase + offset)) = color;
  }
  else {
    /* >16 bit modes use 32 bit values in the frame buffer. */
    *((uint32_t *) (frameBufferBase + offset)) = color;
  }
}

void setBank(uint16_t bank)
{
  if (bank == curBank) return;    /* Bank is already active           */
  curBank = bank;                 /* Save current bank number         */
  bank <<= bankShift;             /* Adjust to window granularity     */

  /* Issue: Need to check if bank flipping is even supported. On Dell,
     for example, there is no B window. */

  if (modeInfoBlock.WinAAttributes & wa_relocatable)
    handleReturn(AsmDisplayWindowControl(SETWIN, WINA, bank));

  if (modeInfoBlock.WinBAttributes & wa_relocatable)
    handleReturn(AsmDisplayWindowControl(SETWIN, WINB, bank));
}

uint16_t
getCurrentMode()
{
  return AsmReturnCurrentVbeMode();
}

/* Copied from VBE spec sample program. */
/* Draw a line from (x1,y1) to (x2,y2) in specified color */

void
line(int x1,int y1,int x2,int y2,int color)
{
    int     d;                      /* Decision variable                */
    int     dx,dy;                  /* Dx and Dy values for the line    */
    int     Eincr,NEincr;           /* Decision variable increments     */
    int     yincr;                  /* Increment for y values           */
    int     t;                      /* Counters etc.                    */

#define ABS(a)   ((a) >= 0 ? (a) : -(a))

    dx = ABS(x2 - x1);
    dy = ABS(y2 - y1);
    if (dy <= dx)
    {
        /* We have a line with a slope between -1 and 1
         *
         * Ensure that we are always scan converting the line from left to
         * right to ensure that we produce the same line from P1 to P0 as the
         * line from P0 to P1.
         */
        if (x2 < x1)
        {
            t = x2; x2 = x1; x1 = t;    /* Swap X coordinates           */
            t = y2; y2 = y1; y1 = t;    /* Swap Y coordinates           */
        }
        if (y2 > y1)
            yincr = 1;
        else
            yincr = -1;
        d = 2*dy - dx;              /* Initial decision variable value  */
        Eincr = 2*dy;               /* Increment to move to E pixel     */
        NEincr = 2*(dy - dx);       /* Increment to move to NE pixel    */
        putPixel(x1,y1,color);      /* Draw the first point at (x1,y1)  */

        /* Incrementally determine the positions of the remaining pixels */
        for (x1++; x1 <= x2; x1++)
        {
            if (d < 0)
                d += Eincr;         /* Choose the Eastern Pixel         */
            else
            {
                d += NEincr;        /* Choose the North Eastern Pixel   */
                y1 += yincr;        /* (or SE pixel for dx/dy < 0!)     */
            }
            putPixel(x1,y1,color);  /* Draw the point                   */
        }
    }
    else
    {
        /* We have a line with a slope between -1 and 1 (ie: includes
         * vertical lines). We must swap our x and y coordinates for this.
         *
         * Ensure that we are always scan converting the line from left to
         * right to ensure that we produce the same line from P1 to P0 as the
         * line from P0 to P1.
         */
        if (y2 < y1)
        {
            t = x2; x2 = x1; x1 = t;    /* Swap X coordinates           */
            t = y2; y2 = y1; y1 = t;    /* Swap Y coordinates           */
        }
        if (x2 > x1)
            yincr = 1;
        else
            yincr = -1;
        d = 2*dx - dy;              /* Initial decision variable value  */
        Eincr = 2*dx;               /* Increment to move to E pixel     */
        NEincr = 2*(dx - dy);       /* Increment to move to NE pixel    */
        putPixel(x1,y1,color);      /* Draw the first point at (x1,y1)  */

        /* Incrementally determine the positions of the remaining pixels */
        for (y1++; y1 <= y2; y1++)
        {
            if (d < 0)
                d += Eincr;         /* Choose the Eastern Pixel         */
            else
            {
                d += NEincr;        /* Choose the North Eastern Pixel   */
                x1 += yincr;        /* (or SE pixel for dx/dy < 0!)     */
            }
            putPixel(x1,y1,color);  /* Draw the point                   */
       }
    }
}

/* Copied from VBE spec sample program. */
/* Draw a simple moire pattern of lines on the display */

void
drawMoire()
{
    uint16_t i;
    uint16_t xres = modeInfoBlock.XResolution;
    uint16_t yres = modeInfoBlock.YResolution;
    uint16_t mask = 0xFF; /* original was 0xFF */

    for (i = 0; i < xres; i += 5)
    {
        line(xres/2,yres/2,i,0,i % mask);
        line(xres/2,yres/2,i,yres,(i+1) % mask);
    }
    for (i = 0; i < yres; i += 5)
    {
        line(xres/2,yres/2,0,i,(i+2) % mask);
        line(xres/2,yres/2,xres,i,(i+3) % mask);
    }
    line(0,0,xres-1,0,15);
    line(0,0,0,yres-1,15);
    line(xres-1,0,xres-1,yres-1,15);
    line(0,yres-1,xres-1,yres-1,15);
}

void
test()
{
  float i;
  uint32_t a, b;
  uint32_t cur = 0;

  for(i = 0; i < 1; i += 0.05)
    {
      drawMoire();
      /* Issue: beeping is rude.
	 printf("%c", BELL);
      */
      waitkbd();
      for(a = 0; a < modeInfoBlock.YResolution; a++)
	{
	  for(b = 0; b < modeInfoBlock.XResolution; b++)
	    {
	      putPixel(b, a, cur);
	    }
	}
    }
}

void
SetConsoleMode(BootInfo *bi, uint32_t mode)
{
  ConsoleInfo *ci;

  bi->consInfo = ci = BootAlloc(sizeof(ConsoleInfo), sizeof(uint32_t));
  /* FIXME:
   * mode == 0 check is here to handle case of windowed modes */
  if(!VESAinit || mode == 0)
    return;

  bi->useGraphicsFB = 1;

  getModeInfo(mode);  /* uses assembly magic */

  /* populate the ConsoleInfo struct */
  ci->len              = sizeof(ConsoleInfo);
  ci->videoMode        = mode;
  ci->frameBuffer      = modeInfoBlock.PhysBasePtr;
  ci->Xlimit           = modeInfoBlock.XResolution; /*chVange?*/
  ci->Ylimit           = modeInfoBlock.YResolution; /*change?*/
  ci->winSize          = modeInfoBlock.WinSize;
  ci->bytesPerScanLine = modeInfoBlock.BytesPerScanLine;
  ci->isBanked         = !modeAttributes.linfb; /* linfb = 1 if linear, 0 if windowed */
  ci->bpp              = modeInfoBlock.BitsPerPixel;
  ci->redMask          = modeInfoBlock.RedMaskSize;
  ci->blueMask         = modeInfoBlock.BlueMaskSize;
  ci->greenMask        = modeInfoBlock.GreenMaskSize;
  ci->redShift         = modeInfoBlock.RedFieldPosition;
  ci->blueShift        = modeInfoBlock.BlueFieldPosition;
  ci->greenShift       = modeInfoBlock.GreenFieldPosition;

  setMode(mode);
}
