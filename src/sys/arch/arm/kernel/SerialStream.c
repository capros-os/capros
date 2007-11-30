/*
 * Copyright (C) 2006, 2007, Strawberry Development Group.
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
#include "ep93xx-uart.h"
#include "ep93xx-vic.h"
#include "Interrupt.h"
#include <eros/arch/arm/mach-ep93xx/ep9315-syscon.h>

void SerialStream_Put(uint8_t c);

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
}

void
SerialStream_Put(uint8_t c)
{
  /* Wait until transmit buffer not full. */
  while (UART1.Flag & UARTFlag_TXFF) ;
  UART1.Data = c;
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

INLINE void
setRxInterruptEnable(void)
{
  if (kstream_debuggerIsActive)
    // disable receive interrupts, debugger will read
    UART1.Ctrl = UARTCtrl_UARTE;
  else
    // enable receive interrupts
    UART1.Ctrl = UARTCtrl_UARTE | UARTCtrl_RIE | UARTCtrl_RTIE;
}

void
SerialStream_SetDebugging(bool onOff)
{
  kstream_debuggerIsActive = onOff;
  setRxInterruptEnable();
}

static void
UART1IntrHandler(VICIntSource * vis)
{
  (void)VIC2.VectAddr;  // read it to mask interrupts of lower or equal priority
  char c = SerialStream_Get();

  if (c == ETX)
    Debugger();

  VIC2.VectAddr = 0;    // write it to reenable interrupts
                        // of lower or equal priority
}

void
SerialStream_EnableDebuggerInput(void)
{
  InterruptSourceSetup(VIC_Source_INT_UART1, 1, &UART1IntrHandler);
  setRxInterruptEnable();
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
KernStream* kstream_SerialStream = &TheSerialStream;
