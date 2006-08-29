#ifndef __EP9315_UART_H_
#define __EP9315_UART_H_
/*
 * Copyright (C) 2006, Strawberry Development Group.
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
   Research Projects Agency under Contract No. W31P4Q-06-C-0040. */

#include <stdint.h>
#include "ep9315.h"

/* Declarations for the Cirrus EP9315 UART. */

/* Bits in RXSts */
#define UARTRXSts_OE	0x8
#define UARTRXSts_BE	0x4
#define UARTRXSts_PE	0x2
#define UARTRXSts_FE	0x1

/* Bits in LinCtrlHigh */
#define UARTLinCtrlHigh_WLEN_8	0x60
#define UARTLinCtrlHigh_WLEN_7	0x40
#define UARTLinCtrlHigh_WLEN_6	0x20
#define UARTLinCtrlHigh_WLEN_5	0x00
#define UARTLinCtrlHigh_FEN	0x10
#define UARTLinCtrlHigh_STP2	0x08
#define UARTLinCtrlHigh_EPS	0x04
#define UARTLinCtrlHigh_PEN	0x02
#define UARTLinCtrlHigh_BRK	0x01

/* Bits in Ctrl */
#define UARTCtrl_LBE	0x80
#define UARTCtrl_RTIE	0x40
#define UARTCtrl_TIE	0x20
#define UARTCtrl_RIE	0x10
#define UARTCtrl_MSIE	0x08
#define UARTCtrl_UARTE	0x01

/* Bits in Flag */
#define UARTFlag_TXFE	0x80
#define UARTFlag_RXFF	0x40
#define UARTFlag_TXFF	0x20
#define UARTFlag_RXFE	0x10
#define UARTFlag_BUSY	0x08
#define UARTFlag_DCD	0x04
#define UARTFlag_DSR	0x02
#define UARTFlag_CTS	0x01

/* Bits in IntIDIntClr */
#define UARTIntIDIntClr_RTIS	0x08
#define UARTIntIDIntClr_TIS	0x04
#define UARTIntIDIntClr_RIS	0x02
#define UARTIntIDIntClr_MIS	0x01

/* Bits in DMACtrl */
#define UARTDMACtrl_DMAERR	0x04
#define UARTDMACtrl_TXDMAE	0x02
#define UARTDMACtrl_RXDMAE	0x01

typedef struct UARTRegisters {
  uint32_t Data;
  uint32_t RXSts;
  uint32_t LinCtrlHigh;
  uint32_t LinCtrlMid;
  uint32_t LinCtrlLow;
  uint32_t Ctrl;
  uint32_t Flag;
  uint32_t IntIDlntClr;
  uint32_t IrLowPwrCntr;	/* UART2 only */
  uint32_t unused1;
  uint32_t DMACtrl;
} UARTRegisters;

#define UART1 (*(volatile struct UARTRegisters *)UART1_BASE)
#define UART2 (*(volatile struct UARTRegisters *)UART2_BASE)
#define UART3 (*(volatile struct UARTRegisters *)UART3_BASE)

#define UART2TMR (*(volatile uint32_t *)(UART2_BASE + 0x84))

/* Bits in Ctrl */
#define ModemCtrl_LOOP	0x10	/* UART1 only */
#define ModemCtrl_OUT2	0x08
#define ModemCtrl_OUT1	0x04
#define ModemCtrl_RTS	0x02	/* UART1 only */
#define ModemCtrl_DTR	0x01	/* UART1 only */

/* Bits in Sts */
#define ModemSts_DCD	0x80
#define ModemSts_RI	0x40
#define ModemSts_DSR	0x20
#define ModemSts_CTS	0x10
#define ModemSts_DDCD	0x08
#define ModemSts_TERI	0x04
#define ModemSts_DDSR	0x02
#define ModemSts_DCTS	0x01

typedef struct ModemRegisters {
  uint32_t Ctrl;
  uint32_t Sts;		/* UART1 only */
} ModemRegisters;

#define Modem1 (*(volatile struct ModemRegisters *)(UART1_BASE + 0x100))
#define Modem3 (*(volatile struct ModemRegisters *)(UART3_BASE + 0x100))

typedef struct HDLCRegisters {
  uint32_t CCtrl;
  uint32_t AddMtchVal;
  uint32_t AddMask;
  uint32_t RXInfoBuf;
  uint32_t Sts;
} HDLCRegisters;

#define HDLC1 (*(volatile struct HDLCRegisters *)(UART1_BASE + 0x20c))
#define HDLC3 (*(volatile struct HDLCRegisters *)(UART3_BASE + 0x20c))

#endif /* __EP9315_UART_H_ */
