/*
 * Copyright (C) 2007, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System runtime library.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */
/* This material is based upon work supported by the US Defense Advanced
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include <linuxk/linux-emul.h>
#include <linuxk/lsync.h>
#include <linux/serial_core.h>
#include <eros/Invoke.h>	// get RC_OK
#include <domain/assert.h>
#include <idl/capros/SuperNode.h>
#include <idl/capros/DevPrivs.h>

#include <linux/amba/bus.h>
#include <linux/amba/serial.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <eros/arch/arm/mach-ep93xx/ep9315-syscon.h>

// Stuff from arch/arm/mach-ep93xx/core.c

#define EP93XX_UART_MCR_OFFSET		(0x0100)

static void ep93xx_uart_set_mctrl(struct amba_device *dev,
				  void __iomem *base, unsigned int mctrl)
{
	unsigned int mcr;

	mcr = 0;
	if (!(mctrl & TIOCM_RTS))
		mcr |= 2;
	if (!(mctrl & TIOCM_DTR))
		mcr |= 1;

	__raw_writel(mcr, base + EP93XX_UART_MCR_OFFSET);
}

static void ep93xx_uart_gate_clk(bool enable, uint32_t mask)
{
  capros_Node_getSlotExtended(KR_LINUX_EMUL, LE_DEVPRIVS, KR_TEMP0);
  result_t result = capros_DevPrivs_deviceConfig(KR_TEMP0, enable, mask);
  assert(result == RC_OK);	// else misconfiguration
}

#if 0 // not used yet

static void ep93xx_uart_gate_clk1(bool enable)
{
  ep93xx_uart_gate_clk(enable, SYSCONDeviceCfg_U1EN);
}

static struct amba_pl010_data ep93xx_uart_data1 = {
	.set_mctrl = ep93xx_uart_set_mctrl,
	.gate_clk  = ep93xx_uart_gate_clk1
};

static struct amba_device uart1_device = {
	.dev		= {
		.bus_id		= "apb:uart1",
		.platform_data	= &ep93xx_uart_data1,
	},
	.res		= {
		.start	= EP93XX_UART1_PHYS_BASE,
		.end	= EP93XX_UART1_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART1, NO_IRQ },
	.periphid	= 0x00041010,
};

static void ep93xx_uart_gate_clk2(bool enable)
{
  ep93xx_uart_gate_clk(enable, SYSCONDeviceCfg_U2EN);
}

static struct amba_pl010_data ep93xx_uart_data2 = {
	.set_mctrl = 0,
	.gate_clk  = ep93xx_uart_gate_clk2
};

static struct amba_device uart2_device = {
	.dev		= {
		.bus_id		= "apb:uart2",
		.platform_data	= &ep93xx_uart_data2,
	},
	.res		= {
		.start	= EP93XX_UART2_PHYS_BASE,
		.end	= EP93XX_UART2_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART2, NO_IRQ },
	.periphid	= 0x00041010,
};
#endif

static void ep93xx_uart_gate_clk3(bool enable)
{
  ep93xx_uart_gate_clk(enable, SYSCONDeviceCfg_U3EN);
}

static struct amba_pl010_data ep93xx_uart_data3 = {
	.set_mctrl = ep93xx_uart_set_mctrl,
	.gate_clk  = ep93xx_uart_gate_clk3
};

static struct amba_device uart3_device = {
	.dev		= {
		.bus_id		= "apb:uart3",
		.platform_data	= &ep93xx_uart_data3,
	},
	.res		= {
		.start	= EP93XX_UART3_PHYS_BASE,
		.end	= EP93XX_UART3_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	.irq		= { IRQ_EP93XX_UART3, NO_IRQ },
	.periphid	= 0x00041010,
};

int
amba_driver_register(struct amba_driver * drv)
{
  /* For now, bypass all the bus stuff and just get the serial port working. */
  int ret = (*drv->probe)(&uart3_device, 0 /* id not used by amba-pl010 */);
  if (ret)
    printk(KERN_ERR "uart3 probe returned %d!\n", ret);
  return 0;
}

void amba_driver_unregister(struct amba_driver * drv)
{
  printk(KERN_ERR "amba_driver_unregister called.\n");	// FIXME
}

extern int pl010_init(void);
int capros_serial_initialization(void)
{
  return pl010_init();
}
