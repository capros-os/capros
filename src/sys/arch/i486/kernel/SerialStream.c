/*
 * Copyright (C) 2001, Jonathan S. Shapiro.
 * Copyright (C) 2005, 2006, Strawberry Development Group.
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

/* This is a serial debugging stream based closely on the earlier
 * logic introduced by Mike Hilsdale. */

#include <kerninc/kernel.h>
#include <kerninc/KernStream.h>
#include <eros/arch/i486/io.h>
#include <arch-kerninc/IRQ-inline.h>
#include "IDT.h"

#define COMIRQ   irq_Serial0
#define COMBASE  0x3f8

extern struct KernStream TheSerialStream;


void
SerialStream_Init()
{
  uint16_t dlab = COMBASE + 3;

  /* Initialize COM port */

  /* Currently: 9600, 8N1 */
  /* (This seemes to not be needed when using VMware
   * and psuedo terminal pipes, but it's here for
   * completeness and for real hardware.)
   */

  /* set DLAB bit      */
  /* b7=1              */
  outb(inb(dlab) | 0x80, dlab);

  /* set 16-bit divisor to 12 (0Ch = 9600 baud) */
  outb(0x0C, COMBASE);
  outb(0, COMBASE + 1);

  /* set data bits to 8 */
  /* b0=1 b1=1          */
  outb(inb(dlab) | 0x03, dlab);

  /* set parity to None */
  /* b3=0 b4=0 b5=0     */
  outb(inb(dlab) & 0xC7, dlab);

  /* set stop bits to 1 */
  /* b2=0               */
  outb(inb(dlab) & 0xFB, dlab);

  /* unset DLAB bit     */
  /* b7=0               */
  outb(inb(COMBASE + 3) & 0x7f, COMBASE + 3);

  /* tell COM port to report INTs on received char */
  outb(1, COMBASE + 1);
}

uint8_t
SerialStream_Get(void)
{
  uint8_t c;

  /* Bit 0 (received data ready) of combase + 5 (serialization status
     register) is 1 if there is a byte ready for us to read. */
  while ((inb(COMBASE + 5) & 1) == 0) {
    /* wait for data */
  }
  c = inb(COMBASE);

  return c;
}

void
SerialStream_Put(uint8_t c)
{
  
  /* Bit 5 (transmit buffer empty) of combase + 5 (serialization status
     register) is 1 if the transmit buffer is ready for a new byte. */
  while ((inb(COMBASE + 5) & 0x20) == 0) {
    /* wait for the serial port to be ready . . . maybe we should
       run faster than 9600 baud :p */
  }
  outb(c, COMBASE);

  return;
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

  /* look at interrupt id to quell UART */
  /* may want to actually do something here sometime later */
  inb(COMBASE + 2);

  if (kstream_debuggerIsActive)
    return;

  assert(irq == COMIRQ);

  c = TheSerialStream.Get();

  if (c == ETX)
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

struct KernStream TheSerialStream = {
  SerialStream_Init,
  SerialStream_Put
#ifdef OPTION_DDB
  ,
  SerialStream_Get,
  SerialStream_SetDebugging,
  SerialStream_EnableDebuggerInput
#endif /*OPTION_DDB*/
};
KernStream* kstream_SerialStream = &TheSerialStream;
