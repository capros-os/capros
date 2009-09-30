/*
 * drivers/base/core.c - core driver model code (device registration, etc)
 *
 * Copyright (c) 2002-3 Patrick Mochel
 * Copyright (c) 2002-3 Open Source Development Labs
 * Copyright (c) 2006 Greg Kroah-Hartman <gregkh@suse.de>
 * Copyright (c) 2006 Novell, Inc.
 *
 * This file is released under the GPLv2
 *
 */

#include <linuxk/linux-emul.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kdev_t.h>
#include <linux/notifier.h>

#include <linux/semaphore.h>

int (*platform_notify)(struct device * dev) = NULL;
int (*platform_notify_remove)(struct device * dev) = NULL;

/*
 * sysfs bindings for devices.
 */

/**
 * dev_driver_string - Return a device's driver name, if at all possible
 * @dev: struct device to get the name of
 *
 * Will return the device's driver's name if it is bound to a device.  If
 * the device is not bound to a device, it will return the name of the bus
 * it is attached to.  If it is not attached to a bus either, an empty
 * string will be returned.
 */
const char *dev_driver_string(struct device *dev)
{
	return dev->driver ? dev->driver->name :
			(dev->bus ? dev->bus->name :
			(dev->class ? dev->class->name : ""));
}

void device_initialize(struct device *dev)
{
        // kobj_set_kset_s(dev, devices_subsys);
        kobject_init(&dev->kobj, NULL);
        // klist_init(&dev->klist_children, klist_children_get,
        //            klist_children_put);
        INIT_LIST_HEAD(&dev->dma_pools);
        INIT_LIST_HEAD(&dev->node);
        init_MUTEX(&dev->sem);
        spin_lock_init(&dev->devres_lock);
        INIT_LIST_HEAD(&dev->devres_head);
        device_init_wakeup(dev, 0);
        set_dev_node(dev, -1);
}
