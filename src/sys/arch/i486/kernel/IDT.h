#ifndef __IDT_H__
#define __IDT_H__
/*
 * Copyright (C) 1998, 1999, Jonathan S. Shapiro.
 * Copyright (C) 2005, Strawberry Development Group.
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


/* If IDT_ENTRIES is revised, the size of the IDT table in interrupt.S
 * must be updated also.
 */
#define IDT_ENTRIES 0x33
#define HW_ENTRIES 0x30


enum intVecType {
  iv_DivZero 		= 0x0,
  iv_Debug 		= 0x1,
  iv_Nmi 		= 0x2,
  iv_BreakPoint 	= 0x3,
  iv_Overflow 		= 0x4,
  iv_Bounds 		= 0x5,
  iv_BadOpcode 		= 0x6,
  iv_DeviceNotAvail	= 0x7,
  iv_DoubleFault 	= 0x8,
  iv_CoprocessorOverrun = 0x9,	/* not used on 486 and above */
  iv_InvalTSS 		= 0xa,
  iv_SegNotPresent 	= 0xb,
  iv_StackSeg 		= 0xc,
  iv_GeneralProtection 	= 0xd,
  iv_PageFault 		= 0xe,
  iv_CoprocError	= 0x10,
  iv_AlignCheck         = 0x11,
  iv_MachineCheck       = 0x12,
  iv_SIMDFloatingPoint  = 0x13,
  
  iv_IRQ0 		= 0x20,
  iv_IRQ1 		= 0x21,
  iv_IRQ2 		= 0x22,
  iv_IRQ3 		= 0x23,
  iv_IRQ4 		= 0x24,
  iv_IRQ5 		= 0x25,
  iv_IRQ6 		= 0x26,
  iv_IRQ7 		= 0x27,
  iv_IRQ8 		= 0x28,
  iv_IRQ9 		= 0x29,
  iv_IRQ10 		= 0x2a,
  iv_IRQ11 		= 0x2b,
  iv_IRQ12 		= 0x2c,
  iv_IRQ13 		= 0x2d,
  iv_IRQ14 		= 0x2e,
  iv_IRQ15 		= 0x2f,
  iv_Yield		= 0x30,
  iv_InvokeKey		= 0x31,
  iv_EmulPseudoInstr	= 0x32,
};

typedef enum intVecType intVecType;

enum IRQ386Type {
  irq_IRQ0,
  irq_HardClock 	= irq_IRQ0,	/* IRQ0 */
  irq_IRQ1,
  irq_Keyboard 		= irq_IRQ1,	/* IRQ1 */
  irq_IRQ2,
  irq_Cascade 		= irq_IRQ2,	/* IRQ2. Should not happen. */
  irq_IRQ3,
  irq_Serial1 		= irq_IRQ3,	/* IRQ3. Known as comm2 in DOS */
  irq_IRQ4,
  irq_Serial0 		= irq_IRQ4,	/* IRQ4. Known as comm1 in DOS */
  irq_IRQ5,
  irq_Parallel1 	= irq_IRQ5,	/* IRQ5. Known as lpt2 in DOS */
  irq_IRQ6,
  irq_Floppy		= irq_IRQ6,	/* IRQ6 */
  irq_IRQ7,
  irq_Parallel0 	= irq_IRQ7,	/* IRQ7. Known as lpt1 in DOS */
  irq_IRQ8,
  irq_WallClock 	= irq_IRQ8,	/* IRQ8 */
  irq_IRQ9,
  irq_IRQ10,
  irq_IRQ11,
  irq_IRQ12,
  irq_IRQ13,
  irq_MathCoproc 	= irq_IRQ13, /* IRQ13 if brain dead NPU */
  irq_IRQ14,
  irq_HardDisk0 	= irq_IRQ14, /* IRQ14 */
  irq_IRQ15,
  irq_HardDisk1 	= irq_IRQ15, /* IRQ14 */
};

    
typedef void (*VecFn)(savearea_t*);
extern VecFn IntVecEntry[IDT_ENTRIES];
extern uint32_t IDTdescriptor[2];

struct IDT {
     
#if 0
  static INLINE bool IsInterruptPending(uint32_t interrupt);
  static INLINE void ResetPIC();
#endif

#if 0
public:
  static uint8_t pic1_cache;
  static uint8_t pic2_cache;
#endif
};

/* Former member functions of IDT */

INLINE void 
idt_lidt()
{
#ifdef __ELF__
  __asm__ __volatile__("lidt IDTdescriptor"
		       : /* no output */
		       : /* no input */
		       : "memory");
#else
  __asm__ __volatile__("lidt __3IDT$IDTdescriptor"
		       : /* no output */
		       : /* no input */
		       : "memory");
#endif
}


void idt_SetupInterruptControllers();
    
/* Following is called by the assembly-level switch routine, even
 * though it is private.
 */
void idt_OnTrapOrInterrupt(savearea_t *) NORETURN;
void idt_OnKeyInvocationTrap(savearea_t *) NORETURN;
void idt_OnEmulatedInstr();
void idt_SetEntry(int, void (*)(void), bool);

void idt_Init();

void idt_WireVector(uint32_t vector, void (*pf)(savearea_t* sa));

INLINE VecFn 
idt_GetVector(uint32_t vector)
{
  return IntVecEntry[vector];
}

void idt_YieldVector(savearea_t*);

void idt_UnboundVector(savearea_t*);

#endif /* __IDT_H__ */
