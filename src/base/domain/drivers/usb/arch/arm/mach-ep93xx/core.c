/*
 * Copyright (C) 2008, 2009, Strawberry Development Group.
 *
 * This file is part of the CapROS Operating System.
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
#include <linux/device.h>
#include <linux/platform_device.h>
#include <domain/assert.h>
#include <eros/Invoke.h>
#include <idl/capros/Process.h>
#include <idl/capros/Constructor.h>

#include <asm/hardware.h>
#include <asm/io.h>
//#include <eros/arch/arm/mach-ep93xx/ep9315-syscon.h>

// Stuff from arch/arm/mach-ep93xx/core.c

static struct resource ep93xx_ohci_resources[] = {
	[0] = {
		.start	= EP93XX_USB_PHYS_BASE,
		.end	= EP93XX_USB_PHYS_BASE + 0x0fff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_EP93XX_USB,
		.end	= IRQ_EP93XX_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device ep93xx_ohci_device = {
	.name		= "ep93xx-ohci",
	.id		= -1,
	.dev		= {
		.kobj		= {
			.name		= "ep93xx-ohci-dev"
		},
		.dma_mask		= (void *)0xffffffff,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(ep93xx_ohci_resources),
	.resource	= ep93xx_ohci_resources,
};

extern int ohci_hcd_mod_init(void);
int capros_hcd_initialization(void)
{
  int err;
  result_t result;
  err = ohci_hcd_mod_init();
  if (err) return err;

  // Create USB Registry.
  // KR_APP2(0) has the constructor.
  // Give it the USBHCD key.
  result = capros_Process_makeStartKey(KR_SELF, 0, KR_TEMP1);
  assert(result == RC_OK);
  result = capros_Constructor_request(KR_APP2(0), KR_BANK, KR_SCHED, KR_TEMP1,
             KR_VOID);
  assert(result == RC_OK);	// FIXME

  platform_device_register(&ep93xx_ohci_device);
  /* platform_device_register calls platform_device_add, which in Linux
  calls device_add, which calls bus_attach_device.
  bus_attach_device checks platform_bus_type.drivers_autoprobe, which
  was previously set by bus_register, so bus_attach_device calls device_attach.
  There being no dev->driver yet, device_attach scans all the drivers
  on the platform bus, calling driver_probe_device.
  In particular it finds the device_driver ohci_hcd_ep93xx_driver.driver,
  part of the platform_driver ohci_hcd_ep93xx_driver
  that was registered by a call to platform_driver_register
  in ohci_hcd_mod_init, called just above.
  That driver, being registered by plaform_driver_register, has the bus
  platform_bus_type, whose match function platform_match is called.
  ep93xx_ohci_device.name, which is "ep93xx-ohci", matches
  ohci_hcd_ep93xx_driver.driver.name, so driver_probe_device goes ahead
  and calls really_probe.
  really_probe sees that there is no platform_bus_type.probe,
  but there is an ohci_hcd_ep93xx_driver.driver.probe
  (copied from ohci_hcd_ep93xx_driver.probe by platform_driver_register),
  so it calls that.
  really_probe also sets dev->driver
  and leaves it set if the probe was successful.  (Whew!)
  CapROS emulation has no device_add, but we must call the probe function: */

extern struct platform_driver ohci_hcd_ep93xx_driver;
  ep93xx_ohci_device.dev.driver = &ohci_hcd_ep93xx_driver.driver;
  err = (*ohci_hcd_ep93xx_driver.probe)(&ep93xx_ohci_device);
  if (err)
    ep93xx_ohci_device.dev.driver = NULL;

  return err;
}
