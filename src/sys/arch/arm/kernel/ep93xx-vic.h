#ifndef __EP93XX_VIC_H_
#define __EP93XX_VIC_H_
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

/* Declarations for the Cirrus EP9315 Vectored Interrupt Controller. */

#define VIC_VectCntl_Enable 0x20

/* VIC1 sources: */
/* 0, 1 unused */
#define VIC_Source_COMMRX	 2
#define VIC_Source_COMMTX	 3
#define VIC_Source_TC1OI	 4
#define VIC_Source_TC2OI	 5
#define VIC_Source_AACINTR	 6
#define VIC_Source_DMAM2P0	 7
#define VIC_Source_DMAM2P1	 8
#define VIC_Source_DMAM2P2	 9
#define VIC_Source_DMAM2P3	10
#define VIC_Source_DMAM2P4	11
#define VIC_Source_DMAM2P5	12
#define VIC_Source_DMAM2P6	13
#define VIC_Source_DMAM2P7	14
#define VIC_Source_DMAM2P8	15
#define VIC_Source_DMAM2P9	16
#define VIC_Source_DMAM2M0	17
#define VIC_Source_DMAM2M1	18
#define VIC_Source_GPIO0INTR	19
#define VIC_Source_GPIO1INTR	20
#define VIC_Source_GPIO2INTR	21
#define VIC_Source_GPIO3INTR	22
#define VIC_Source_UART1RXINTR	23
#define VIC_Source_UART1TXINTR	24
#define VIC_Source_UART2RXINTR	25
#define VIC_Source_UART2TXINTR	26
#define VIC_Source_UART3RXINTR	27
#define VIC_Source_UART3TXINTR	28
#define VIC_Source_INT_KEY	29
#define VIC_Source_INT_TOUCH	30
/* unused			31 */

/* VIC2 sources: */
#define VIC_Source_INT_EXT0	32
#define VIC_Source_INT_EXT1	33
#define VIC_Source_INT_EXT2	34
#define VIC_Source_TINTR	35
#define VIC_Source_WEINT	36
#define VIC_Source_INT_RTC	37
#define VIC_Source_INT_IrDA	38
#define VIC_Source_INT_MAC	39
#define VIC_Source_INT_EXT3	40
#define VIC_Source_INT_PROG	41
#define VIC_Source_CLK1HZ	42
#define VIC_Source_V_SYNC	43
#define VIC_Source_INT_VIDEO_FIFO	44
#define VIC_Source_INT_SSP1RX	45
#define VIC_Source_INT_SSP1TX	46
#define VIC_Source_GPIO4INTR	47
#define VIC_Source_GPIO5INTR	48
#define VIC_Source_GPIO6INTR	49
#define VIC_Source_GPIO7INTR	50
#define VIC_Source_TC3OI	51
#define VIC_Source_INT_UART1	52
#define VIC_Source_SSPINTR	53
#define VIC_Source_INT_UART2	54
#define VIC_Source_INT_UART3	55
#define VIC_Source_USHINTR	56
#define VIC_Source_INT_PME	57
#define VIC_Source_INT_DSP	58
#define VIC_Source_GPIOINTR	59
#define VIC_Source_SAIINTR	60

typedef struct VICRegisters {
  uint32_t IRQStatus;
  uint32_t FIQStatus;
  uint32_t RawIntr;
  uint32_t IntSelect;
  uint32_t IntEnable;
  uint32_t IntEnClear;
  uint32_t SoftInt;
  uint32_t SoftIntClear;
  uint32_t Protection;
  uint32_t unused0[3];
  uint32_t VectAddr;
  uint32_t DefVectAddr;
  uint32_t unused1[50];
  uint32_t VectAddrN[16];
  uint32_t unused2[48];
  uint32_t VectCntlN[16];
  uint32_t unused3[872];
  uint32_t PeriphID0;
  uint32_t PeriphID1;
  uint32_t PeriphID2;
  uint32_t PeriphID3;
} VICRegisters;

#define VIC1 (*(volatile struct VICRegisters *)VIC1_BASE)
#define VIC2 (*(volatile struct VICRegisters *)VIC2_BASE)

#endif /* __EP93XX_VIC_H_ */
