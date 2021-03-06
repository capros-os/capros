/*
 * Copyright (C) 1998, 1999, 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, 2008-2010, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System,
 * and is derived from the EROS Operating System.
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
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <string.h>
#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <kerninc/Machine.h>
#include <kerninc/dma.h>
#include <kerninc/Activity.h>
#include <kerninc/util.h>
#include <kerninc/PhysMem.h>
#include <kerninc/SysTimer.h>
#include <kerninc/mach-rtc.h>
#include <kerninc/PCI.h>
#include <kerninc/IRQ.h>
#include <idl/capros/arch/i386/SysTrace.h>
#include <idl/capros/arch/i386/DevPrivsX86.h>
#include <arch-kerninc/PTE.h>
#include "CpuFeatures.h"
#include <kerninc/Process.h>
#include <kerninc/Depend.h>
#include "Process486.h"
#include "CMOS.h"
#include "Cpu.h"
#include "asm.h"

#include "GDT.h"
#include "IDT.h"
#include "TSS.h"
#include "process_asm_offsets.h"

uint32_t mach_BusArchitecture();

void mach_EnableVirtualMapping();

unsigned int CpuVendorCode = capros_arch_i386_DevPrivsX86_VendorCode_Unknown;

/* On entry, we have a stack (created in lostart.S) and interrupts
 * are disabled.  Static constructors have been run.
 * 
 * Otherwise, the state of the machine is whatever environment
 * the bootstrap code gave us (typically not much).  In particular,
 * controllers and devices may be in arbitrarily inconsistent states
 * unless the static constructors have ensured otherwise.
 * 
 * On exit from this routine, the kernel should be running in
 * an appropriate virtual map, and should have performed enough
 * initialization to have a minimal interrupt table installed.
 * Interrupts are disabled on entry, and should be enabled on exit.
 * Note that on exit from this procedure the controller entry points
 * have not yet been established (that will happen next).
 * 
 * The implicit assumption here is that interrupt handling happens
 * in two tiers, and that the procesor interrupts can be enabled
 * without enabling all device interrupts.
 * 
 * It is customary for this code to hand-initialize the console, so
 * that boot-related diagnostics can be seen.
 */

void 
mach_BootInit()
{
  // A quick check that the C and assembler Process structure offsets match:
  assert(offsetof(Process, faultInfo) == PR_OFF_faultInfo);

  // Determine CpuVendorCode from vendor string.
#define vendor(vendorIDString, name) \
  if (! strcmp(CpuVendor, vendorIDString)) \
    CpuVendorCode = capros_arch_i386_DevPrivsX86_VendorCode_##name;
  vendor("GenuineIntel", Intel)
  else vendor("CyrixInstead", Cyrix)
  else vendor("AuthenticAMD", AMD)
  else vendor("UMC UMC UMC", UMC)
  else vendor("CentaurHauls", Centaur)
  else vendor("GenuineTMx86", Transmeta)
  else vendor("TransmetaCPU", Transmeta)
  else vendor("Geode by NSC", NSC)
#undef vendor

  /* The kernel address space must, however, be constructed and
   * enabled before the GDT, IDT, and TSS descriptors are loaded,
   * because these descriptors reference linear addresses that change
   * when the mapping is updated.
   */
  
  (void) mach_SetMappingTable(KernPageDir_pa); /* Well known address! */
  (void) mach_EnableVirtualMapping();

  printf("About to load GDT\n");

  gdt_Init();
  
  /*    printf("main(): loaded GDT\n");
   * Commented out to avoid missing symbol complaints
   */
  
  idt_Init();
  
/*    printf("main(): loaded IDT\n"); */
  
  tss_Init();
  
/*    printf("main(): loaded TSS\n"); */

  switch(mach_BusArchitecture()) {
  case bt_Unknown:
    printf("Unknown bus type!\n");
    halt('u');
  case bt_ISA:
    printf("ISA bus\n");
    break;
  case bt_MCA:
    printf("MCA bus -- get a real machine!\n");
    break;
  case bt_PCI:
    printf("PCI bus\n");
    break;
  }

  init_dma();
  
  assert(sizeof(uint16_t) == 2);
  assert(sizeof(uint32_t) == 4);
  assert(sizeof(void *) == 4);
  assert(sizeof(Key) == 16);
  assert(offsetof(KeyBits, keyType) == 12);

  /* Verify the queue key representation pun: */
  assert(sizeof(StallQueue) == 2 * sizeof(uint32_t));
  
  /* Verify that in shifting the interrupt vectors around we haven't
   * violated the assumptions in eros/i486/target.h */

  assert(IRQ_FROM_EXCEPTION(iv_IRQ0) == 0);

  printf("Kernel is mapped. CR0 = %#x\n", ReadCR0());

  /*  printf("Pre enable: intdepth=%d\n", IDT::intdepth); */

  /* We enable interrupts on the processor here.  Note that at this
   * point all of the hardware interrupts are disabled.  We need to
   * enable processor interrupts before we start autoconfiguring,
   * since some of the IRQ detection code will proceed by inducing an
   * interrupt to detect the configured IRQ.
   * 
   * The down side to enabling here is that we have not yet set up the
   * exception handlers for processor generated exceptions.  Those
   * will get initialized very early in the autoconfiguration process.
   * The alternative would be a small redesign that would allow us to
   * initialize them by hand via a call to the AutoConfig logic here.
   * 
   * In principle, the kernel ought to be completely free of
   * processor-generated exceptions, so the current design is probably
   * just fine.  This note is here mostly as a guideline for other
   * processors that may require different solutions.
   */
  
#ifdef OPTION_KERN_PROFILE
  /* This must be done before the timer interrupt is enabled!! */
  extern void InitKernelProfiler();
  InitKernelProfiler();
#endif

#ifdef EROS_HAVE_FPU
  mach_InitializeFPU();
#endif
  
  irq_ENABLE_for_IRQ();
  
  mach_InitHardClock();
  
#ifdef OPTION_DDB
  kstream_dbg_stream->EnableDebuggerInput();
#endif

  printf("Motherboard interrupts initialized\n");
}

int
RtcSave(capros_RTC_time_t newTime)
{
  return 0;	// nothing to do?
}

static unsigned int
cmosBcd(unsigned int byte)
{
  return BCDToBin(cmos_cmosByte(byte));
}

capros_RTC_time_t
RtcRead(void)
{
  return kernMktime(
    cmosBcd(0x9),	// year
    cmosBcd(0x8),	// month
    cmosBcd(0x7),	// day of month
    cmosBcd(0x4),	// hours
    cmosBcd(0x2),	// minutes
    cmosBcd(0x0)	// seconds
    );
  // We don't use:
  // cmosBcd(0x6)	// day of week
}

int
RtcSet(capros_RTC_time_t newTime)
{
  return -1;	// not implemented
}


/* This cannot be run until the kernel has it's own map -- the gift
 * map we get from the bootstrap code at the moment is too small.
 */
uint32_t
mach_BusArchitecture()
{
  static uint32_t busType = bt_Unknown;

  if (busType != bt_Unknown)
    return busType;
  
  busType = bt_ISA;

  if (memcmp((char*)0x0FFFD9, "EISA", 4) == 0)
    busType = bt_EISA;

  if (pciBios_Present())
    busType = bt_PCI;

  return busType;
}

#if 0
/* Map the passed physical pages starting at the designated kernel
 * virtual address:
 */
void
Machine::MapBuffer(kva_t va, kpa_t p0, kpa_t p1)
{
  const uint32_t ndx0 = (KVTOL(va) >> 22) & 0x3ffu;
  const uint32_t ndx1 = (KVTOL(va) >> 12) & 0x3ffu;

  kpa_t maptbl_paddr;
  
  __asm__ __volatile__("movl %%cr3, %0"
		       : "=r" (maptbl_paddr)
		       : /* no inputs */);

  PTE *pageTbl = (PTE*) PTOV(maptbl_paddr);
  pageTbl = (PTE*) PTOV( (pageTbl[ndx0].AsWord() & ~EROS_PAGE_MASK) );

  PTE *pte = pageTbl + ndx1;

  /* These PTE's are already marked present, writable, etc. etc. by
   * construction in the kernel mapping table - just update the
   * frames.
   */
  (*pte) = p0;
  pte++;
  if (p1) {
    (*pte) = p1;
    PTE_SET(*pte, PTE_V);
  }
  else
    PTE_CLR(*pte, PTE_V);

  if (CpuType > 3) {
    Machine::FlushTLB(KVTOL(va));
    Machine::FlushTLB(KVTOL(va + 4096));
  }
  else
    Machine::FlushTLB();
}
#endif

/* non-static because compiler cannot detect ASM usage and complains */
uint32_t BogusIDTDescriptor[2] = { 0, 0 };

void
mach_HardReset()
{
  /* Load an IDT with no valid entries, which will force a machine
   * check when the next interrupt occurs:
   */
  
  __asm__ __volatile__("lidt BogusIDTDescriptor"
		       : /* no output */
		       : /* no input */
		       : "memory");

  /* now force an interrupt: */
  __asm__ ("int $0x30");	/* iv_Yield */
}

#ifdef EROS_HAVE_FPU

void
mach_InitializeFPU()
{
  FPUInit();

  uint32_t cr0 = ReadCR0();
  cr0 &= ~ CR0_EM;
  cr0 |= CR0_ET | CR0_TS | CR0_MP;
  WriteCR0(cr0);
}

void
mach_DisableFPU()
{
  uint32_t cr0 = ReadCR0();
  assert((cr0 & (CR0_EM | CR0_MP)) == CR0_MP);	// should always have EM=0, MP=1
  cr0 |= CR0_TS;
  WriteCR0(cr0);
}

void
mach_EnableFPU()
{
  assert((ReadCR0() & (CR0_EM | CR0_MP)) == CR0_MP); // should have EM=0, MP=1
  ClearTSFlag();
}
#endif

/* Following probably would be easier to do in assembler, but updating
 * the mode table is a lot easier to do in C++.  Second argument says
 * whether we wish to count cycles (1) or events (2).  Generally we
 * will want events.
 */

extern void Pentium_SetCounterMode(uint32_t mode, uint32_t wantCy);
extern void PentiumPro_SetCounterMode(uint32_t mode, uint32_t wantCy);


static const char * ModeNames[capros_arch_i386_SysTrace_mode_NUM_MODE] = {
  "Cycles",
  "Instrs",
  "DTLB",
  "ITLB",
  "Dmiss",
  "Imiss",
  "Dwrtbk",
  "Dfetch",
  "Ifetch",
  "Branch",
  "TkBrnch",
};
static uint32_t PentiumModes[capros_arch_i386_SysTrace_mode_NUM_MODE] = {
  0x0,				/* SysTrace_Mode_Cycles	*/
  0x16,				/* SysTrace_Mode_Instrs	*/
  0x02,				/* SysTrace_Mode_DTLB	*/
  0x0d,				/* SysTrace_Mode_ITLB	*/
  0x29,				/* SysTrace_Mode_Dmiss	*/
  0x0e,				/* SysTrace_Mode_Imiss	*/
  0x06,				/* SysTrace_Mode_Dwrtbk	*/
  0x28,				/* SysTrace_Mode_Dfetch	*/
  0x0c,				/* SysTrace_Mode_Ifetch	*/
  0x12,				/* SysTrace_Mode_Branches */
  0x14,				/* SysTrace_Mode_BrTaken */
};
static uint32_t PentiumProModes[capros_arch_i386_SysTrace_mode_NUM_MODE] = {
  0x79,				/* SysTrace_Mode_Cycles	*/
  0xc0,				/* SysTrace_Mode_Instrs	*/
  0x0,		/* no analog */	/* SysTrace_Mode_DTLB	*/
  0x85,				/* SysTrace_Mode_ITLB	*/
  0x45,				/* SysTrace_Mode_Dmiss	*/
  0x81,				/* SysTrace_Mode_Imiss	*/
  0x0,          /* no analog */	/* SysTrace_Mode_Dwrtbk	*/
  0x43,				/* SysTrace_Mode_Dfetch	*/
  0x80,				/* SysTrace_Mode_Ifetch	*/
  0xc4,				/* SysTrace_Mode_Branches */
  0xc9,				/* SysTrace_Mode_BrTaken */
};

/* Other possible modes of interest:

   Pentium   Ppro    What
   0x17      0x0     V-pipe instrs
   0x19      0x04    WB-full stalls
   0x1a      ??      mem read stalls   
   0x13      0xca    btb hits (ppro: retired taken mispredicted branches)
   0x0       0x65    Dcache reads (ppro: burst read transactions)
   0x1f      ??      Agen interlocks */

bool
mach_SetCounterMode(uint32_t mode)
{
  uint32_t wantcy = (mode == capros_arch_i386_SysTrace_mode_cycles) ? 1 : 0;
  if (mode >= capros_arch_i386_SysTrace_mode_NUM_MODE)
    return false;

  if (CpuType == 5) {
    Pentium_SetCounterMode(PentiumModes[mode], wantcy);
  }
  else if (CpuType == 6) {
    PentiumPro_SetCounterMode(PentiumProModes[mode], wantcy);
  }

  return true;
}

const char *
mach_ModeName(uint32_t mode)
{
  if (mode >= capros_arch_i386_SysTrace_mode_NUM_MODE)
    return "???";

  return ModeNames[mode];
}

/* If proc == NULL, load the current process's address space. */
bool    // returns true if successful
mach_LoadAddrSpace(Process * proc)
{
  if (!proc) {
    proc = proc_Current();
    if (!proc) {
      mach_SetMappingTable(KernPageDir_pa);
      return true;
    }
  }
  // If proc has a small space, setting the mapping table is unnecessary,
  // but harmless.
  mach_SetMappingTable(proc->md.MappingTable);
  return true;
}
