/*
 * Copyright (C) 2006, 2007, 2008, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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
Research Projects Agency under Contract Nos. W31P4Q-06-C-0040 and
W31P4Q-07-C-0070.  Approved for public release, distribution unlimited. */

/* This implements a console outputing to UART1.
   It is assumed that the boot loader left UART1 in a good state. */

#include <kerninc/KernStream.h>
#include <arch-kerninc/IRQ-inline.h>
#include "ep93xx-uart.h"
#include "ep93xx-vic.h"
#include "Interrupt.h"
#include <eros/arch/arm/mach-ep93xx/ep9315-syscon.h>

#define VIC1 (VIC1Struct(AHB_VA + VIC1_AHB_OFS))
#define VIC2 (VIC2Struct(AHB_VA + VIC2_AHB_OFS))
#define UART1 (UARTStruct(APB_VA + UART1_APB_OFS))
#define SYSCON (SYSCONStruct(APB_VA + SYSCON_APB_OFS))

#define WRAP_LINES
#ifdef WRAP_LINES
unsigned int curCol;	// column where the cursor is at, 0 == first column
#define MaxColumns 80
#define TABSTOP 8
#endif

static void setInterruptEnable(void);
static void unbufferedOutput(uint8_t c);
static void outputBufferedChar(void);

/* outputProc starts off as unbufferedOutput.
 * After we enable interrupts, it becomes bufferedOutput,
 * so as not to stop processes for too long.
 * (Otherwise, the serial port input buffer will overflow.) */
void (*outputProc) (uint8_t c) = &unbufferedOutput;

static void
unbufferedOutput(uint8_t c)
{
  /* Wait until transmit buffer not full. */
  while (UART1.Flag & UARTFlag_TXFF) ;
  UART1.Data = c;
}

#define outBufferSize 4000
uint8_t outBuffer[outBufferSize];	// circular buffer for output
unsigned int outBufChars = 0;	// number of characters in outBuffer
uint8_t * outBufInP = &outBuffer[0];	// where next char will go
uint8_t * outBufOutP = &outBuffer[0];	// where next char will come from
				// accessed only at interrupt level

// Interrupts must be disabled.
static void
bufferedOutput(uint8_t c)
{
  irqFlags_t flags = mach_local_irq_save();	// to protect outBufChars

  if (outBufChars >= outBufferSize) {
    /* The buffer is full.
    If this is called by a process calling the console key, we should delay it.
    But for now, spin: */
    while (UART1.Flag & UARTFlag_TXFF) ;
    outputBufferedChar();
  }

  uint8_t * p = outBufInP;
  *p = c;
  if (++p >= &outBuffer[outBufferSize])
    p = &outBuffer[0];	// wrap
  outBufInP = p;
  if (0 == outBufChars++)
    setInterruptEnable();

  mach_local_irq_restore(flags);
}

static void
setInterruptEnable(void)
{
  uint32_t ctrl = UARTCtrl_UARTE;	// enable UART

  if (kstream_debuggerIsActive) {
    // disable receive interrupts, debugger will read
    // debugger disables interrupts, so flush output and do unbuffered output
    irqFlags_t flags = mach_local_irq_save();	// to protect outBufChars
    while (outBufChars) {
      while (UART1.Flag & UARTFlag_TXFF) ;
      outputBufferedChar();	// this may recurse once
    }
    mach_local_irq_restore(flags);

    outputProc = &unbufferedOutput;
  } else {
    // enable receive interrupts
    ctrl |= UARTCtrl_RIE | UARTCtrl_RTIE;
    outputProc = &bufferedOutput;
  }

  // Transmit interrupt is enabled iff the transmit buffer is nonempty:
  if (outBufChars)
    ctrl |= UARTCtrl_TIE;

  UART1.Ctrl = ctrl;
}

static void
outputBufferedChar(void)
{
  uint8_t * p = outBufOutP;
  UART1.Data = *p;
  if (++p >= &outBuffer[outBufferSize])
    p = &outBuffer[0];	// wrap
  if (--outBufChars == 0)
    setInterruptEnable();
  outBufOutP = p;
}

INLINE void
RawOutput(uint8_t c)
{
  (*outputProc)(c);
}

#ifdef WRAP_LINES
static void
SpacingChar(void)
{
  if (curCol >= MaxColumns) {
    curCol = 0;
    RawOutput(CR);
    RawOutput(LF);
  }
  curCol++;
}
#endif

void
SerialStream_Flush(void)
{
  while (UART1.Flag & UARTFlag_BUSY) ;
}

void
SerialStream_Init(void)
{
  SerialStream_Flush();

  /* Set the UARTCLK to the low frequency, which give a max baud rate
  of only 230 Kbps, but saves power. */
  SYSCON.PwrCnt &= ~SYSCONPwrCnt_UARTBAUD;
#define UARTCLK 7372800 /* Hz */

#define BaudRate 57600
//#define BaudRate 9600	// for SWCA test
#define BaudRateDivisor (UARTCLK/(16*BaudRate)-1)
  UART1.LinCtrlLow = BaudRateDivisor & 0xff;
  UART1.LinCtrlMid = (BaudRateDivisor >> 8) & 0xff;
  /* Must write LineCtrlHigh last. */
  /* 8 bits No parity 1 stop bit */
  UART1.LinCtrlHigh = UARTLinCtrlHigh_WLEN_8 | UARTLinCtrlHigh_FEN;
  UART1.Ctrl = UARTCtrl_UARTE;
  { int i;
    for (i = 55; i > 0; i--) ;
  }

#ifdef WRAP_LINES
  curCol = 0;
#endif
}

void
SerialStream_Put(uint8_t c)
{
#ifdef WRAP_LINES
  if (kstream_IsPrint(c)) {
    SpacingChar();
  }
  else {
    /* Handle the non-printing characters: */
    switch (c) {
    case BS:
      if (curCol)
	curCol--;
      break;
    case TAB:
      SpacingChar();
      while (curCol % TABSTOP)
        SpacingChar();
      break;
    case CR:
      curCol = 0;
      break;
    default:
      break;
    }
  }
#endif
  RawOutput(c);
}

#ifdef OPTION_DDB
uint8_t
SerialStream_Get(void)
{
  /* Wait until receive buffer not empty. */
  while (UART1.Flag & UARTFlag_RXFE) ;
  uint8_t c = UART1.Data;
  return c;
}

void
SerialStream_SetDebugging(bool onOff)
{
  kstream_debuggerIsActive = onOff;
  setInterruptEnable();
}

static void
UART1IntrHandler(VICIntSource * vis)
{
  (void)VIC2.VectAddr;  // read it to mask interrupts of lower or equal priority

  uint32_t status = UART1.IntIDIntClr;

  if (status & (UARTIntIDIntClr_RIS | UARTIntIDIntClr_RTIS)) {
    char c = SerialStream_Get();

    if (c == ETX)
      Debugger();
  }

  if (status & UARTIntIDIntClr_TIS) {
    // while FIFO not full and outBuffer not empty
    while (!(UART1.Flag & UARTFlag_TXFF)
           && outBufChars) {
      outputBufferedChar();
    }
  }

  VIC2.VectAddr = 0;    // write it to reenable interrupts
                        // of lower or equal priority
}

void
SerialStream_EnableDebuggerInput(void)
{
  InterruptSourceSetup(VIC_Source_INT_UART1, 1, &UART1IntrHandler);
  setInterruptEnable();
  InterruptSourceEnable(VIC_Source_INT_UART1);
}
#endif

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
