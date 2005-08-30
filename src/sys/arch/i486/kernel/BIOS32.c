/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
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

/* X86 implementation of the PCI BIOS interface.  Mostly, we just cop
 * out and pass the buck to the system BIOS.
 */

#include <kerninc/kernel.h>
/*#include <kerninc/util.h>*/
#include <kerninc/PCI.h>
#include <kerninc/IRQ.h>
#include "Segment.h"

#define dbg_bios32	0x1	/* steps in taking snapshot */
#define dbg_pcibios	0x2	/* migration state machine */

/* Following should be an OR of some of the above */
#define dbg_flags   ( 0u )

#define DBCOND(x) (dbg_##x & dbg_flags)
#define DEBUG(x) if DBCOND(x)
#define DEBUG2(x,y) if ((dbg_##x|dbg_##y) & dbg_flags)

/*
 * Different versions of GAS implement different syntax for lcall. The
 * newer ones bitch about the old syntax, while the older ones barf on
 * the new syntax. Lovely. Really.
 *
 * At this point we have upgraded to a newer gas for
 * cross-compilation, so everything should use the * form:
 */

/* #define LCALL(reg) "lcall (" #reg ")" */
#define LCALL(reg) "lcall *(" #reg ")"


/* Issue: the PCI bios is called during hardware init, when CPL == 0,
 * and also at driver init, by which point CPL is 1.  For this reason
 * the descriptor table has TWO bios32 segments that are identical
 * except for CPL.  Every procedure in here must take care to set the
 * right one using the following macro
 */

#define SET_SELECTOR(farptr) \
  farptr.selector = \
    (GetCPL() ? sel_KProcBios32 : sel_KernelBios32)

/* These are derived from MindShare's PCI System Architecture: */
#define PCIBIOSFUN(x) (0xb100 | (x))

extern void halt(char);

enum PciFn {
  pcifn_BiosPresent     = PCIBIOSFUN(0x1),
  pcifn_FindDevice      = PCIBIOSFUN(0x2),
  pcifn_FindClass       = PCIBIOSFUN(0x3),
  
  pcifn_SpecialCycle    = PCIBIOSFUN(0x6),
  
  pcifn_RdConfByte      = PCIBIOSFUN(0x8),
  pcifn_RdConfHalfWord  = PCIBIOSFUN(0x9),
  pcifn_RdConfWord      = PCIBIOSFUN(0xa),
  pcifn_WrConfByte      = PCIBIOSFUN(0xb),
  pcifn_WrConfHalfWord  = PCIBIOSFUN(0xc),
  pcifn_WrConfWord      = PCIBIOSFUN(0xd),
  pcifn_GetIrqRoute     = PCIBIOSFUN(0xe),
  pcifn_SetPciInterrupt = PCIBIOSFUN(0xf),
};


/* Various memory signatures we need to look for: */


enum Signature{
  sig_bios32 = ('_' + ('3' << 8) + ('2' << 16) + ('_' << 24)),
  sig_pci    = ('P' + ('C' << 8) + ('I' << 16) + (' ' << 24)),
  sig_pcisvc = ('$' + ('P' << 8) + ('C' << 16) + ('I' << 24)),
};


union Bios32Entry {
  struct {
    uint32_t signature;		/* Signature::bios32 */
    kpa_t entry;		/* physical address */
    uint8_t revision;		/* revision level */
    uint8_t length;		/* in paragraphs -- currently 1 */
    uint8_t checksum;		/* bytewise checksum */
    uint8_t reserved[5];		/* reserved for future use */
  } fields;
  uint8_t bytes[16];
} ;

typedef union Bios32Entry Bios32Entry;

/* Virtual address of the 32-bit BIOS service directory.  Some BIOS's
 * appear to have more than one.  We use only the first one for now.
 */

static bool Bios32IsInit = false;
static klva_t Bios32EntryPt = 0;
static struct {
  uint32_t offset;
  uint16_t selector;
} Bios32FarPtr = { 0, sel_Null };

inline uint32_t
GetCPL()
{
  uint32_t cpl;
  
  __asm__ ("movw %%cs,%w0\n\t"
	   "andl $0x3,%0"
	   : "=r" (cpl) /* output */
	   : /* no input */);

  return cpl;
}

/* Try to hunt down a 32 bit BIOS services directory: */
static void
bios32_init()
{
  Bios32Entry *b32bottom = 0;
  Bios32Entry *b32top = 0;
  Bios32Entry *entry = 0;
  uint32_t length;
  uint32_t i;
  uint8_t sum;

  DEBUG(bios32)
    dprintf(true, "Initializing BIOS32\n");

  if (Bios32IsInit)
    return;
  
  b32bottom = (Bios32Entry *) 0xe0000;
  b32top    = (Bios32Entry *) 0xffff0;

  for (entry = b32bottom; entry < b32top; entry++) {
    if (entry->fields.signature != sig_bios32)
      continue;

    length = entry->fields.length * 16;
    if (length == 0) {
      printf("BIOS32: WARNING! bogus bios32 structure at 0x%08x\n", entry);
      continue;
    }

    assert(length);

    sum = 0;
    for (i = 0; i < length; i++)
      sum += entry->bytes[i];

    if (sum != 0)
      continue;

    DEBUG(bios32)
      dprintf(true, "BIOS32: structure found at 0x%08x\n", entry);

    if (entry->fields.revision != 0x0)
      fatal("Unsupported BIOS32 revision %d at 0x%08x\n",
		    entry->fields.revision, entry);

    if (Bios32EntryPt == 0) {
      if (entry->fields.entry > 0x100000)
	fatal("BIOS32 entry point in high memory at 0x%08x\n",
		      entry->fields.entry);

      Bios32EntryPt = Bios32FarPtr.offset = entry->fields.entry;
      printf("BIOS32 Services Directory found at 0x%08x\n",
		     entry->fields.entry);
    }
  }

  Bios32IsInit = true;

  DEBUG(bios32)
    dprintf(true, "BIOS32 initialization completed: %s\n",
		    Bios32IsInit ? "good" : "not present");
}

/*
 * Returns the entry point for the given service, NULL on error
 */

static uint32_t
Bios32FindService(uint32_t service)
{
  uint8_t returnCode;	/* %al */
  uint32_t address;		/* %ebx */
  uint32_t length;		/* %ecx */
  uint32_t entry;		/* %edx */

  SET_SELECTOR(Bios32FarPtr);
		
  irq_DISABLE();
  __asm__(LCALL(%%edi)
	  : "=a" (returnCode),
	    "=b" (address),
	    "=c" (length),
	    "=d" (entry)
	  : "0" (service),
	    "1" (0),
	    "D" (&Bios32FarPtr));
  irq_ENABLE();

  switch (returnCode) {
  case 0:
    return address + entry;

  case 0x80:	/* Not present */
    printf("Bios32FindService(%ld) : not present\n", service);
    return 0;

  default: /* Shouldn't happen */
    fatal("Bios32FindService(%ld) : returned 0x%x\n",
		  service, returnCode);
    return 0;
  }
}

bool pciBios_isInit = false;

static klva_t PciBiosEntryPt = 0;
static struct {
  uint32_t offset;
  uint16_t selector;
} PciFarPtr = { 0, sel_Null };

void
pciBios_Init()
{
  if (pciBios_isInit)
    return;

  DEBUG(pcibios)
    dprintf(true,"Initializing PCI Bios\n");
  
  bios32_init();

  if (Bios32EntryPt == 0) {
    printf("PciBios::Init(): no BIOS32 entry point\n");
    return;
  }

  DEBUG(pcibios)
    dprintf(true,"First call to Bios32FindService()\n");
  
  PciBiosEntryPt = Bios32FindService(sig_pcisvc);

  DEBUG(pcibios)
    dprintf(true,"First call to Bios32FindService() -- made it\n");
  
  if (PciBiosEntryPt) {
    uint32_t signature;
    uint8_t status;
    uint8_t majorRevision;
    uint8_t minorRevision;
    uint32_t pack;

    SET_SELECTOR(PciFarPtr);
    PciFarPtr.offset = PciBiosEntryPt;

    irq_DISABLE();
    __asm__(LCALL(%%edi) "\n\t"
	    "jc 1f\n\t"
	    "xor %%ah, %%ah\n"
	    "1:\tshl $8, %%eax\n\t"
	    "movw %%bx, %%ax"
	    : "=d" (signature),
	      "=a" (pack)
	    : "1" (pcifn_BiosPresent),
	      "D" (&PciFarPtr)
	    : "bx", "cx");
    irq_ENABLE();

    status = (pack >> 16) & 0xff;
    majorRevision = (pack >> 8) & 0xff;
    minorRevision = pack & 0xff;

#if 0
    printf("Found PCI BIOS with signature 0x%08x (\"%c%c%c%c\")\n",
		   signature,
		   signature & 0xff,
		   (signature >> 8) & 0xff,
		   (signature >> 16) & 0xff,
		   (signature >> 24) & 0xff);
    
    printf("  status=%d\n", status);
    printf("  majorRev=0x%x\n", majorRevision);
    printf("  minorRev=0x%x\n", minorRevision);
#endif

    if (signature != sig_pci) {
      PciBiosEntryPt = 0;
      printf("PCI BIOS signature is bad\n");
      halt('B');
    }

    if (status)
      printf("Unexpected status %d from PciFn::BiosPresent\n",
		     status);

    if (PciBiosEntryPt) {
      printf("PCI BIOS revision %x.%02x entry at 0x%x\n",
		     majorRevision, minorRevision, PciBiosEntryPt);
    }
  }

  pciBios_isInit = true;

  return;
}

bool
pciBios_Present()
{
  pciBios_Init();
  
  if (PciBiosEntryPt)
    return true;
  
  return false;			/* just for now */
}

/* Patch brain-damaged card entries: */
void
pciBios_Fixup()
{
}



uint32_t
pciBios_FindDevice (uint16_t vendor, uint16_t device_id,
		     uint16_t index, uint8_t* bus, uint8_t* device_fn)
{
  uint16_t bx = 0;
  uint16_t ret = 0;

  DEBUG(pcibios)
    dprintf(true,"Call to PciBios::FindDevice()\n");
  
  SET_SELECTOR(PciFarPtr);

  irq_DISABLE();
  __asm__(LCALL(%%edi) "\n\t"
	  "jc 1f\n\t"
	  "xor %%ah, %%ah\n"
	  "1:"
	  : "=b" (bx),
	  "=a" (ret)
	  : "1" (pcifn_FindDevice),
	  "c" (device_id),
	  "d" (vendor),
	  "S" ((uint32_t) index),
	  "D" (&PciFarPtr));
  irq_ENABLE();

  DEBUG(pcibios)
    dprintf(true,"Call to PciBios::FindDevice() -- done\n");

  *bus = (bx >> 8) & 0xff;
  *device_fn = bx & 0xff;
  return (uint32_t) (ret & 0xff00) >> 8;
}


uint32_t
pciBios_FindClass (uint32_t devClass, uint16_t index,
		    uint8_t *bus, uint8_t* device_fn)
{
  uint16_t bx;
  uint16_t ret;

  SET_SELECTOR(PciFarPtr);

  DEBUG(pcibios)
    dprintf(true,"Call to PciBios::FindClass()\n");
  
  irq_DISABLE();
  __asm__ (LCALL(%%edi)"\n\t"
	   "jc 1f\n\t"
	   "xor %%ah, %%ah\n"
	   "1:"
	   : "=b" (bx),
	   "=a" (ret)
	   : "1" (pcifn_FindClass),
	   "c" (devClass),
	   "S" ((uint32_t) index),
	   "D" (&PciFarPtr));
  irq_ENABLE();

  DEBUG(pcibios)
    dprintf(true,"Call to PciBios::FindClass() -- done\n");

  *bus = (bx >> 8) & 0xff;
  *device_fn = bx & 0xff;
  return (int) (ret & 0xff00) >> 8;
}

uint32_t
pciBios_ReadConfig8 (uint8_t bus,
                     uint8_t device_fn, uint8_t where, uint8_t* value)
{
  uint32_t ret = 0;
  uint32_t bx = (bus << 8) | device_fn;

  SET_SELECTOR(PciFarPtr);

  irq_DISABLE();
  __asm__(LCALL(%%esi) "\n\t"
	  "jc 1f\n\t"
	  "xor %%ah, %%ah\n"
	  "1:"
	  : "=c" (*value),
	  "=a" (ret)
	  : "1" (pcifn_RdConfByte),
	  "b" (bx),
	  "D" ((uint32_t) where),
	  "S" (&PciFarPtr));
  irq_ENABLE();

  return (uint32_t) (ret & 0xff00) >> 8;
}

uint32_t
pciBios_ReadConfig16 (uint8_t bus,
                      uint8_t device_fn,
                      uint8_t where, uint16_t* value)
{
  uint32_t ret = 0;
  uint32_t bx = (bus << 8) | device_fn;

  SET_SELECTOR(PciFarPtr);

  irq_DISABLE();
  __asm__(LCALL(%%esi) "\n\t"
	  "jc 1f\n\t"
	  "xor %%ah, %%ah\n"
	  "1:"
	  : "=c" (*value),
	  "=a" (ret)
	  : "1" (pcifn_RdConfHalfWord),
	  "b" (bx),
	  "D" ((uint32_t) where),
	  "S" (&PciFarPtr));
  irq_ENABLE();

  return (uint32_t) (ret & 0xff00) >> 8;
}

uint32_t
pciBios_ReadConfig32 (uint8_t bus,
                      uint8_t device_fn, uint8_t where, uint32_t* value)
{
  uint32_t ret = 0;
  uint32_t bx = (bus << 8) | device_fn;

  SET_SELECTOR(PciFarPtr);

  irq_DISABLE();
  __asm__(LCALL(%%esi) "\n\t"
	  "jc 1f\n\t"
	  "xor %%ah, %%ah\n"
	  "1:"
	  : "=c" (*value),
	  "=a" (ret)
	  : "1" (pcifn_RdConfWord),
	  "b" (bx),
	  "D" ((uint32_t) where),
	  "S" (&PciFarPtr));
  irq_ENABLE();

  return (uint32_t) (ret & 0xff00) >> 8;
}

uint32_t
pciBios_WriteConfig8 (uint8_t bus,
                      uint8_t device_fn, uint8_t where, uint8_t value)
{
  uint32_t ret;
  uint32_t bx = (bus << 8) | device_fn;

  SET_SELECTOR(PciFarPtr);

  irq_DISABLE();
  __asm__(LCALL(%%esi) "\n\t"
	  "jc 1f\n\t"
	  "xor %%ah, %%ah\n"
	  "1:"
	  : "=a" (ret)
	  : "0" (pcifn_WrConfByte),
	  "c" (value),
	  "b" (bx),
	  "D" ((long) where),
	  "S" (&PciFarPtr));
  irq_ENABLE();

  return (int) (ret & 0xff00) >> 8;
}

uint32_t
pciBios_WriteConfig16 (uint8_t bus,
                       uint8_t device_fn, uint8_t where, uint16_t value)
{
  uint32_t ret = 0;
  uint32_t bx = (bus << 8) | device_fn;

  SET_SELECTOR(PciFarPtr);

  irq_DISABLE();
  __asm__(LCALL(%%esi) "\n\t"
	  "jc 1f\n\t"
	  "xor %%ah, %%ah\n"
	  "1:"
	  : "=a" (ret)
	  : "0" (pcifn_WrConfHalfWord),
	  "c" (value),
	  "b" (bx),
	  "D" ((long) where),
	  "S" (&PciFarPtr));
  irq_ENABLE();

  return (int) (ret & 0xff00) >> 8;
}

uint32_t
pciBios_WriteConfig32 (uint8_t bus,
                       uint8_t device_fn, uint8_t where, uint32_t value)
{
  uint32_t ret = 0;
  uint32_t bx = (bus << 8) | device_fn;

  SET_SELECTOR(PciFarPtr);

  irq_DISABLE();
  __asm__(LCALL(%%esi) "\n\t"
	  "jc 1f\n\t"
	  "xor %%ah, %%ah\n"
	  "1:"
	  : "=a" (ret)
	  : "0" (pcifn_WrConfWord),
	  "c" (value),
	  "b" (bx),
	  "D" ((uint32_t) where),
	  "S" (&PciFarPtr));
  irq_ENABLE();

  return (uint32_t) (ret & 0xff00) >> 8;
}

