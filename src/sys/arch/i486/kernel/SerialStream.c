/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
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

/* This is a serial debugging stream based closely on the earlier
 * logic introduced by Mike Hilsdale. */

#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <eros/arch/i486/io.h>
#include <arch-kerninc/IRQ-inline.h>
#include "IDT.h"
#include "IRQ386.h"

#define COMIRQ   irq_Serial0
#define COMBASE  0x3f8
// When DLAB == 0:
// Input:
#define RX  (COMBASE + 0)
#define IIR (COMBASE + 2)
#define LCR (COMBASE + 3)
#define LSR (COMBASE + 5)
#define MSR (COMBASE + 6)
// Output:
#define TX  (COMBASE + 0)
#define IER (COMBASE + 1)
#define LCR (COMBASE + 3)
#define MCR (COMBASE + 4)

extern struct KernStream TheSerialStream;


void
SerialStream_MachInit(void)
{
  /* Initialize COM port */

  /* (This seemes to not be needed when using VMware
   * and psuedo terminal pipes, but it's here for
   * completeness and for real hardware.)
   */

  /* set Divisor Latch Access Bit (DLAB) */
  /* b7=1              */
  outb(inb(LCR) | 0x80, LCR);

#if 0 // 9600 baud
  /* set 16-bit divisor to 12 (0Ch = 9600 baud) */
  outb(12, COMBASE);
#else // 57600 baud
  // set 16-bit divisor to 2
  outb(2, COMBASE);
#endif
  outb(0, COMBASE + 1);

  /* Currently: 8N1 */
  /* set data bits to 8 */
  /* b0=1 b1=1          */
  outb(inb(LCR) | 0x03, LCR);

  /* set parity to None */
  /* b3=0 b4=0 b5=0     */
  outb(inb(LCR) & 0xC7, LCR);

  /* set stop bits to 1 */
  /* b2=0               */
  outb(inb(LCR) & 0xFB, LCR);

  /* unset DLAB bit     */
  /* b7=0               */
  outb(inb(LCR) & 0x7f, LCR);

  /* tell COM port to report INTs on received char */
  outb(0x05, IER);

  // Most PCs require OUT2 to enable interrupts.
  outb(0x08, MCR);
}

void
SerialStream_RawOutput(uint8_t c)
{
  
  /* Bit 5 (transmit buffer empty) of LSR (line status register)
     is 1 if the transmit buffer is ready for a new byte. */
  while ((inb(LSR) & 0x20) == 0) {
    /* wait for the serial port to be ready . . . maybe we should
       run faster than 9600 baud :p */
  }
  outb(c, TX);

  return;
}

#ifdef OPTION_DDB
uint8_t
SerialStream_Get(void)
{
  uint8_t c;

  /* Bit 0 (received data ready) of LSR (line status register)
     is 1 if there is a byte ready for us to read. */
  while ((inb(LSR) & 1) == 0) {
    /* wait for data */
  }
  c = inb(RX);

  return c;
}

void
SerialStream_SetDebugging(bool onOff)
{
  kstream_debuggerIsActive = onOff;
  
  if (! onOff)
    irq_Enable(COMIRQ);
}

static void
SerialInterrupt(savearea_t *sa)
{
  char c;
  uint32_t irq = IRQ_FROM_EXCEPTION(sa->ExceptNo);

  (void) inb(IIR);	// look at interrupt id to quell UART
  /* may want to actually do something here sometime later */

  if (kstream_debuggerIsActive)
    return;

  assert(irq == COMIRQ);

  c = TheSerialStream.Get();

  if (c == ETX)	// control-C
    Debugger();

  irq_Enable(irq);
}

void
SerialStream_EnableDebuggerInput(void)
{
  irq_SetHandler(COMIRQ, SerialInterrupt);
  printf("Set up keyboard (console) interrupt handler!\n");

#if 0
  /* Establish initial keyboard state visibly: */
  UpdateKbdLeds();
#endif

  /* FIX: This may prove a problem, as I need to check exactly where
   * the interrupt vectors are set up in the boot sequence... */
  irq_Enable(COMIRQ);
}
#endif // OPTION_DDB
