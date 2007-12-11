/*
 * platform.c - platform 'pseudo' bus for legacy devices
 *
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 *
 * This file is released under the GPLv2
 *
 * Please see Documentation/driver-model/platform.txt for more
 * information.
 */

#include <linuxk/linux-emul.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/init.h>
//#include <linux/bootmem.h>
#include <linux/err.h>
#include <linux/slab.h>

#define to_platform_driver(drv)	(container_of((drv), struct platform_driver, driver))

struct device platform_bus = {
	.bus_id		= "platform",
};
EXPORT_SYMBOL_GPL(platform_bus);

#if 0 // not in CapROS
/**
 *	platform_get_resource - get a resource for a device
 *	@dev: platform device
 *	@type: resource type
 *	@num: resource index
 */
struct resource *
platform_get_resource(struct platform_device *dev, unsigned int type,
		      unsigned int num)
{
	int i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

		if ((r->flags & (IORESOURCE_IO|IORESOURCE_MEM|
				 IORESOURCE_IRQ|IORESOURCE_DMA))
		    == type)
			if (num-- == 0)
				return r;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(platform_get_resource);

/**
 *	platform_get_irq - get an IRQ for a device
 *	@dev: platform device
 *	@num: IRQ number index
 */
int platform_get_irq(struct platform_device *dev, unsigned int num)
{
	struct resource *r = platform_get_resource(dev, IORESOURCE_IRQ, num);

	return r ? r->start : -ENXIO;
}
EXPORT_SYMBOL_GPL(platform_get_irq);

/**
 *	platform_get_resource_byname - get a resource for a device by name
 *	@dev: platform device
 *	@type: resource type
 *	@name: resource name
 */
struct resource *
platform_get_resource_byname(struct platform_device *dev, unsigned int type,
		      char *name)
{
	int i;

	for (i = 0; i < dev->num_resources; i++) {
		struct resource *r = &dev->resource[i];

		if ((r->flags & (IORESOURCE_IO|IORESOURCE_MEM|
				 IORESOURCE_IRQ|IORESOURCE_DMA)) == type)
			if (!strcmp(r->name, name))
				return r;
	}
	return NULL;
}
EXPORT_SYMBOL_GPL(platform_get_resource_byname);

/**
 *	platform_get_irq - get an IRQ for a device
 *	@dev: platform device
 *	@name: IRQ name
 */
int platform_get_irq_byname(struct platform_device *dev, char *name)
{
	struct resource *r = platform_get_resource_byname(dev, IORESOURCE_IRQ, name);

	return r ? r->start : -ENXIO;
}
EXPORT_SYMBOL_GPL(platform_get_irq_byname);

/**
 *	platform_add_devices - add a numbers of platform devices
 *	@devs: array of platform devices to add
 *	@num: number of platform devices in array
 */
int platform_add_devices(struct platform_device **devs, int num)
{
	int i, ret = 0;

	for (i = 0; i < num; i++) {
		ret = platform_device_register(devs[i]);
		if (ret) {
			while (--i >= 0)
				platform_device_unregister(devs[i]);
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(platform_add_devices);
#endif // not in CapROS

struct platform_object {
	struct platform_device pdev;
	char name[1];
};

/**
 *	platform_device_put
 *	@pdev:	platform device to free
 *
 *	Free all memory associated with a platform device.  This function
 *	must _only_ be externally called in error cases.  All other usage
 *	is a bug.
 */
void platform_device_put(struct platform_device *pdev)
{
#if 0 // not in CapROS
	if (pdev)
		put_device(&pdev->dev);
#endif
}
EXPORT_SYMBOL_GPL(platform_device_put);

static void platform_device_release(struct device *dev)
{
	struct platform_object *pa = container_of(dev, struct platform_object, pdev.dev);

	kfree(pa->pdev.dev.platform_data);
	kfree(pa->pdev.resource);
	kfree(pa);
}

/**
 *	platform_device_alloc
 *	@name:	base name of the device we're adding
 *	@id:    instance id
 *
 *	Create a platform device object which can have other objects attached
 *	to it, and which will have attached objects freed when it is released.
 *
 *	This device will be marked as not supporting hotpluggable drivers; no
 *	device add/remove uevents will be generated.  In the unusual case that
 *	the device isn't being dynamically allocated as a legacy "probe the
 *	hardware" driver, infrastructure code should reverse this marking.
 */
struct platform_device *platform_device_alloc(const char *name, unsigned int id)
{
	struct platform_object *pa;

	pa = kzalloc(sizeof(struct platform_object) + strlen(name), GFP_KERNEL);
	if (pa) {
		strcpy(pa->name, name);
		pa->pdev.name = pa->name;
		pa->pdev.id = id;
//		device_initialize(&pa->pdev.dev);
		pa->pdev.dev.release = platform_device_release;

		/* prevent hotplug "modprobe $(MODALIAS)" from causing trouble in
		 * legacy probe-the-hardware drivers, which don't properly split
		 * out device enumeration logic from drivers.
		 */
		pa->pdev.dev.uevent_suppress = 1;
	}

	return pa ? &pa->pdev : NULL;
}
EXPORT_SYMBOL_GPL(platform_device_alloc);

#if 0 // not in CapROS
/**
 *	platform_device_add_resources
 *	@pdev:	platform device allocated by platform_device_alloc to add resources to
 *	@res:   set of resources that needs to be allocated for the device
 *	@num:	number of resources
 *
 *	Add a copy of the resources to the platform device.  The memory
 *	associated with the resources will be freed when the platform
 *	device is released.
 */
int platform_device_add_resources(struct platform_device *pdev, struct resource *res, unsigned int num)
{
	struct resource *r;

	r = kmalloc(sizeof(struct resource) * num, GFP_KERNEL);
	if (r) {
		memcpy(r, res, sizeof(struct resource) * num);
		pdev->resource = r;
		pdev->num_resources = num;
	}
	return r ? 0 : -ENOMEM;
}
EXPORT_SYMBOL_GPL(platform_device_add_resources);

/**
 *	platform_device_add_data
 *	@pdev:	platform device allocated by platform_device_alloc to add resources to
 *	@data:	platform specific data for this platform device
 *	@size:	size of platform specific data
 *
 *	Add a copy of platform specific data to the platform device's platform_data
 *	pointer.  The memory associated with the platform data will be freed
 *	when the platform device is released.
 */
int platform_device_add_data(struct platform_device *pdev, const void *data, size_t size)
{
	void *d;

	d = kmalloc(size, GFP_KERNEL);
	if (d) {
		memcpy(d, data, size);
		pdev->dev.platform_data = d;
	}
	return d ? 0 : -ENOMEM;
}
EXPORT_SYMBOL_GPL(platform_device_add_data);
#endif // not in CapROS

/**
 *	platform_device_add - add a platform device to device hierarchy
 *	@pdev:	platform device we're adding
 *
 *	This is part 2 of platform_device_register(), though may be called
 *	separately _iff_ pdev was allocated by platform_device_alloc().
 */
int platform_device_add(struct platform_device *pdev)
{
	int ret = 0;

	if (!pdev)
		return -EINVAL;

#if 0 // not in CapROS
	if (!pdev->dev.parent)
		pdev->dev.parent = &platform_bus;

	pdev->dev.bus = &platform_bus_type;

	if (pdev->id != -1)
		snprintf(pdev->dev.bus_id, BUS_ID_SIZE, "%s.%u", pdev->name, pdev->id);
	else
		strlcpy(pdev->dev.bus_id, pdev->name, BUS_ID_SIZE);

	int i;
	for (i = 0; i < pdev->num_resources; i++) {
		struct resource *p, *r = &pdev->resource[i];

		if (r->name == NULL)
			r->name = pdev->dev.bus_id;

		p = r->parent;
		if (!p) {
			if (r->flags & IORESOURCE_MEM)
				p = &iomem_resource;
			else if (r->flags & IORESOURCE_IO)
				p = &ioport_resource;
		}

		if (p && insert_resource(p, r)) {
			printk(KERN_ERR
			       "%s: failed to claim resource %d\n",
			       pdev->dev.bus_id, i);
			ret = -EBUSY;
			goto failed;
		}
	}

	pr_debug("Registering platform device '%s'. Parent at %s\n",
		 pdev->dev.bus_id, pdev->dev.parent->bus_id);

	ret = device_add(&pdev->dev);
	if (ret == 0)
		return ret;

 failed:
	while (--i >= 0)
		if (pdev->resource[i].flags & (IORESOURCE_MEM|IORESOURCE_IO))
			release_resource(&pdev->resource[i]);
#endif // not in CapROS
	return ret;
}
EXPORT_SYMBOL_GPL(platform_device_add);

/**
 *	platform_device_del - remove a platform-level device
 *	@pdev:	platform device we're removing
 *
 *	Note that this function will also release all memory- and port-based
 *	resources owned by the device (@dev->resource).  This function
 *	must _only_ be externally called in error cases.  All other usage
 *	is a bug.
 */
void platform_device_del(struct platform_device *pdev)
{
#if 0 // not in CapROS
	int i;

	if (pdev) {
		device_del(&pdev->dev);

		for (i = 0; i < pdev->num_resources; i++) {
			struct resource *r = &pdev->resource[i];
			if (r->flags & (IORESOURCE_MEM|IORESOURCE_IO))
				release_resource(r);
		}
	}
#endif // not in CapROS
}
EXPORT_SYMBOL_GPL(platform_device_del);

/**
 *	platform_device_register - add a platform-level device
 *	@pdev:	platform device we're adding
 *
 */
int platform_device_register(struct platform_device * pdev)
{
//	device_initialize(&pdev->dev);
	return platform_device_add(pdev);
}
EXPORT_SYMBOL_GPL(platform_device_register);

/**
 *	platform_device_unregister - unregister a platform-level device
 *	@pdev:	platform device we're unregistering
 *
 *	Unregistration is done in 2 steps. First we release all resources
 *	and remove it from the subsystem, then we drop reference count by
 *	calling platform_device_put().
 */
void platform_device_unregister(struct platform_device * pdev)
{
	platform_device_del(pdev);
	platform_device_put(pdev);
}
EXPORT_SYMBOL_GPL(platform_device_unregister);

#if 0 // not for CapROS
/**
 *	platform_device_register_simple
 *	@name:  base name of the device we're adding
 *	@id:    instance id
 *	@res:   set of resources that needs to be allocated for the device
 *	@num:	number of resources
 *
 *	This function creates a simple platform device that requires minimal
 *	resource and memory management. Canned release function freeing
 *	memory allocated for the device allows drivers using such devices
 *	to be unloaded without waiting for the last reference to the device
 *	to be dropped.
 *
 *	This interface is primarily intended for use with legacy drivers
 *	which probe hardware directly.  Because such drivers create sysfs
 *	device nodes themselves, rather than letting system infrastructure
 *	handle such device enumeration tasks, they don't fully conform to
 *	the Linux driver model.  In particular, when such drivers are built
 *	as modules, they can't be "hotplugged".
 */
struct platform_device *platform_device_register_simple(char *name, unsigned int id,
							struct resource *res, unsigned int num)
{
	struct platform_device *pdev;
	int retval;

	pdev = platform_device_alloc(name, id);
	if (!pdev) {
		retval = -ENOMEM;
		goto error;
	}

	if (num) {
		retval = platform_device_add_resources(pdev, res, num);
		if (retval)
			goto error;
	}

	retval = platform_device_add(pdev);
	if (retval)
		goto error;

	return pdev;

error:
	platform_device_put(pdev);
	return ERR_PTR(retval);
}
EXPORT_SYMBOL_GPL(platform_device_register_simple);
#endif // not in CapROS

static int platform_drv_probe(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	return drv->probe(dev);
}

#if 0 // not in CapROS
static int platform_drv_probe_fail(struct device *_dev)
{
	return -ENXIO;
}
#endif // not in CapROS

static int platform_drv_remove(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	return drv->remove(dev);
}

static void platform_drv_shutdown(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	drv->shutdown(dev);
}

static int platform_drv_suspend(struct device *_dev, pm_message_t state)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	return drv->suspend(dev, state);
}

static int platform_drv_resume(struct device *_dev)
{
	struct platform_driver *drv = to_platform_driver(_dev->driver);
	struct platform_device *dev = to_platform_device(_dev);

	return drv->resume(dev);
}

/**
 *	platform_driver_register
 *	@drv: platform driver structure
 */
int platform_driver_register(struct platform_driver *drv)
{
//	drv->driver.bus = &platform_bus_type;
	if (drv->probe)
		drv->driver.probe = platform_drv_probe;
	if (drv->remove)
		drv->driver.remove = platform_drv_remove;
	if (drv->shutdown)
		drv->driver.shutdown = platform_drv_shutdown;
	if (drv->suspend)
		drv->driver.suspend = platform_drv_suspend;
	if (drv->resume)
		drv->driver.resume = platform_drv_resume;
#if 0 // not in CapROS
	return driver_register(&drv->driver);
#else
	return 0;
#endif
}
EXPORT_SYMBOL_GPL(platform_driver_register);

/**
 *	platform_driver_unregister
 *	@drv: platform driver structure
 */
void platform_driver_unregister(struct platform_driver *drv)
{
#if 0 // not in CapROS
	driver_unregister(&drv->driver);
#endif // not in CapROS
}
EXPORT_SYMBOL_GPL(platform_driver_unregister);
