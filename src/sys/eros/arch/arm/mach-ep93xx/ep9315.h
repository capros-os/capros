#ifndef __EP9315_H_
#define __EP9315_H_
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

/* Declarations for the Cirrus EP9315. */

/* Register addresses */

/* These are offsets from the beginning of the AHB registers
 (physical address 0x80000000). */
#define DMA_AHB_OFS	0x000000
#define EMAC_AHB_OFS	0x010000
#define USB_AHB_OFS	0x020000
#define RASTER_AHB_OFS	0x030000
#define SDRAM_AHB_OFS	0x060000
#define SMC_AHB_OFS	0x080000
#define BOOT_ROM_AHB_OFS	0x090000
#define IDE_AHB_OFS	0x0a0000
#define VIC1_AHB_OFS	0x0b0000
#define VIC2_AHB_OFS	0x0c0000

/* These are offsets from the beginning of the APB registers
 (physical address 0x80800000). */
#define TIMER_APB_OFS	0x010000
#define I2S_APB_OFS	0x020000
#define SECURITY_APB_OFS	0x030000
#define GPIO_APB_OFS	0x040000
#define AC97_APB_OFS	0x080000
#define SPI_APB_OFS	0x0a0000
#define IRDA_APB_OFS	0x0b0000
#define UART1_APB_OFS	0x0c0000
#define UART2_APB_OFS	0x0d0000
#define UART3_APB_OFS	0x0e0000
#define KEY_APB_OFS	0x0f0000
#define TOUCH_APB_OFS	0x100000
#define PWM_APB_OFS	0x110000
#define RTC_APB_OFS	0x120000
#define SYSCON_APB_OFS	0x130000
#define WATCHDOG_APB_OFS	0x140000

#endif /* __EP9315_H_ */
