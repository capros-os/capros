#ifndef __EP9315_SYSCON_H_
#define __EP9315_SYSCON_H_
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

/* Declarations for the Cirrus EP9315 System Controller. */

/* Bits for PwrCnt */
#define SYSCONPwrCnt_DMAM2PCH1	0x00010000
#define SYSCONPwrCnt_DMAM2PCH0	0x00020000
#define SYSCONPwrCnt_DMAM2PCH3	0x00040000
#define SYSCONPwrCnt_DMAM2PCH2	0x00080000
#define SYSCONPwrCnt_DMAM2PCH5	0x00100000
#define SYSCONPwrCnt_DMAM2PCH4	0x00200000
#define SYSCONPwrCnt_DMAM2PCH7	0x00400000
#define SYSCONPwrCnt_DMAM2PCH6	0x00800000
#define SYSCONPwrCnt_DMAM2PCH9	0x01000000
#define SYSCONPwrCnt_DMAM2PCH8	0x02000000
#define SYSCONPwrCnt_DMAM2MCH0	0x04000000
#define SYSCONPwrCnt_DMAM2MCH1	0x08000000
#define SYSCONPwrCnt_USH_EN	0x10000000
#define SYSCONPwrCnt_UARTBAUD	0x20000000
#define SYSCONPwrCnt_FIR_EN	0x80000000

/* Bits for DeviceCfg */
#define SYSCONDeviceCfg_SHena	0x00000001
#define SYSCONDeviceCfg_KEYS	0x00000002
#define SYSCONDeviceCfg_ADCPD	0x00000004
#define SYSCONDeviceCfg_RAS	0x00000008
#define SYSCONDeviceCfg_RasOnP3	0x00000010
#define SYSCONDeviceCfg_I2SoAC97	0x00000040
#define SYSCONDeviceCfg_I2SonSSP	0x00000080
#define SYSCONDeviceCfg_EonIDE	0x00000100
#define SYSCONDeviceCfg_PonG	0x00000200
#define SYSCONDeviceCfg_GonIDE	0x00000400
#define SYSCONDeviceCfg_HonIDE	0x00000800
#define SYSCONDeviceCfg_HC1EN	0x00001000
#define SYSCONDeviceCfg_HC1IN	0x00002000
#define SYSCONDeviceCfg_HC3EN	0x00004000
#define SYSCONDeviceCfg_HC3IN	0x00008000
#define SYSCONDeviceCfg_TIN	0x00020000
#define SYSCONDeviceCfg_U1EN	0x00040000
#define SYSCONDeviceCfg_EXVC	0x00080000
#define SYSCONDeviceCfg_U2EN	0x00100000
#define SYSCONDeviceCfg_A1onG	0x00200000
#define SYSCONDeviceCfg_A2onG	0x00400000
#define SYSCONDeviceCfg_CPENA	0x00800000
#define SYSCONDeviceCfg_U3EN	0x01000000
#define SYSCONDeviceCfg_MonG	0x02000000
#define SYSCONDeviceCfg_TonG	0x04000000
#define SYSCONDeviceCfg_GonK	0x08000000
#define SYSCONDeviceCfg_IonU2	0x10000000
#define SYSCONDeviceCfg_D0onG	0x20000000
#define SYSCONDeviceCfg_D1onG	0x40000000
#define SYSCONDeviceCfg_SWRST	0x80000000

/* Bits for SysCfg */
#define SYSCONSysCfg_LCSn1	0x00000001
#define SYSCONSysCfg_LCSn2	0x00000002
#define SYSCONSysCfg_LEECLK	0x00000008
#define SYSCONSysCfg_LEEDA	0x00000010
#define SYSCONSysCfg_LASDO	0x00000020
#define SYSCONSysCfg_LCSn6	0x00000040
#define SYSCONSysCfg_LCSn7	0x00000080
#define SYSCONSysCfg_SBOOT	0x00000100
#define SYSCONSysCfg_REV	0xf0000000

#define SYSCONSysSWLock_Unlock	0xaa

typedef struct SYSCONRegisters {
  uint32_t PwrSts;
  uint32_t PwrCnt;
  uint32_t Halt;
  uint32_t Standby;
  uint32_t unused1[2];
  uint32_t TEOI;
  uint32_t STFClr;
  uint32_t ClkSet1;
  uint32_t ClkSet2;
  uint32_t unused2[6];
  uint32_t ScratchReg0;
  uint32_t ScratchReg1;
  uint32_t unused3[2];
  uint32_t APBWait;
  uint32_t BusMstrArb;
  uint32_t BootModeClr;
  uint32_t unused4[9];
  uint32_t DeviceCfg;
  uint32_t VidClkDiv;
  uint32_t MIRClkDiv;
  uint32_t I2SClkDiv;
  uint32_t KeyTchClkDiv;
  uint32_t CHIP_ID;
  uint32_t unused5;
  uint32_t SysCfg;
  uint32_t unused6[8];
  uint32_t SysSWLock;
} GPIORegisters;

#define SYSCON (*(volatile struct SYSCONRegisters *)SYSCON_BASE)

#endif /* __EP9315_SYSCON_H_ */
