/*
 * Copyright (C) 2008, Strawberry Development Group
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
Research Projects Agency under Contract No. W31P4Q-07-C-0070.
Approved for public release, distribution unlimited. */

#include "usbdev.h"

/* Functions local to drivers/usb/core/ */

extern int usb_create_sysfs_dev_files (struct usb_device *dev);
extern void usb_remove_sysfs_dev_files (struct usb_device *dev);
extern int usb_create_sysfs_intf_files (struct usb_interface *intf);
extern void usb_remove_sysfs_intf_files (struct usb_interface *intf);
extern int usb_create_ep_files(struct device *parent, struct usb_host_endpoint *endpoint,
				struct usb_device *udev);
extern void usb_remove_ep_files(struct usb_host_endpoint *endpoint);

extern void usb_disable_endpoint (struct usb_device *dev, unsigned int epaddr);
extern void usb_disable_interface (struct usb_device *dev,
		struct usb_interface *intf);
extern void usb_release_interface_cache(struct kref *ref);
extern void usb_disable_device (struct usb_device *dev, int skip_ep0);
extern void usb_detect_quirks(struct usb_device *udev);

extern int usb_get_device_descriptor(struct usb_device *dev,
		unsigned int size);
extern char *usb_cache_string(struct usb_device *udev, int index);
extern int usb_set_configuration(struct usb_device *dev, int configuration);

extern void usb_kick_khubd(struct usb_device *dev);
extern int usb_match_device(struct usb_device *dev,
			    const struct usb_device_id *id);

extern int  usb_hub_init(void);
extern void usb_hub_cleanup(void);
extern int usb_major_init(void);
extern void usb_major_cleanup(void);
extern int usb_host_init(void);
extern void usb_host_cleanup(void);

#ifdef	CONFIG_PM

extern void usb_autosuspend_work(struct work_struct *work);
extern int usb_port_suspend(struct usb_device *dev);
extern int usb_port_resume(struct usb_device *dev);
extern int usb_external_suspend_device(struct usb_device *udev,
		pm_message_t msg);
extern int usb_external_resume_device(struct usb_device *udev);

static inline void usb_pm_lock(struct usb_device *udev)
{
	mutex_lock_nested(&udev->pm_mutex, udev->level);
}

static inline void usb_pm_unlock(struct usb_device *udev)
{
	mutex_unlock(&udev->pm_mutex);
}

#else

#define usb_port_suspend(dev)		0
#define usb_port_resume(dev)		0
static inline void usb_pm_lock(struct usb_device *udev) {}
static inline void usb_pm_unlock(struct usb_device *udev) {}

#endif

#ifdef CONFIG_USB_SUSPEND

extern void usb_autosuspend_device(struct usb_device *udev);
extern void usb_try_autosuspend_device(struct usb_device *udev);
extern int usb_autoresume_device(struct usb_device *udev);

#else

#define usb_autosuspend_device(udev)		do {} while (0)
#define usb_try_autosuspend_device(udev)	do {} while (0)
static inline int usb_autoresume_device(struct usb_device *udev)
{
	return 0;
}

#endif

extern struct usb_bus * theBus;
extern struct workqueue_struct *ksuspend_usb_wq;
extern struct bus_type usb_bus_type;
extern struct device_type usb_device_type;
extern struct device_type usb_if_device_type;
extern struct usb_device_driver usb_generic_driver;
extern struct usb_driver hub_driver;

int usbdev_generic_probe(struct usb_device *udev);
void usbdev_generic_disconnect(struct usb_device *udev);
int usb_unbind_interface(struct device * dev);

static inline int is_usb_device(const struct device *dev)
{
	return dev->type == &usb_device_type;
}

/* Do the same for device drivers and interface drivers. */

static inline int is_usb_device_driver(struct device_driver *drv)
{
	return container_of(drv, struct usbdrv_wrap, driver)->
			for_devices;
}

/* Interfaces and their "power state" are owned by usbcore */

static inline void mark_active(struct usb_interface *f)
{
	f->is_active = 1;
}

static inline void mark_quiesced(struct usb_interface *f)
{
	f->is_active = 0;
}

static inline int is_active(const struct usb_interface *f)
{
	return f->is_active;
}


/* for labeling diagnostics */
extern const char *usbcore_name;

/* usbfs stuff */
extern struct mutex usbfs_mutex;
extern struct usb_driver usbfs_driver;
extern const struct file_operations usbfs_devices_fops;
extern const struct file_operations usbdev_file_operations;
extern void usbfs_conn_disc_event(void);

extern int usb_devio_init(void);
extern void usb_devio_cleanup(void);

struct dev_state {
	struct list_head list;      /* state list */
	struct usb_device *dev;
	struct file *file;
	spinlock_t lock;            /* protects the async urb lists */
	struct list_head async_pending;
	struct list_head async_completed;
	wait_queue_head_t wait;     /* wake up if a request completed */
	unsigned int discsignr;
	struct pid *disc_pid;
	uid_t disc_uid, disc_euid;
	void __user *disccontext;
	unsigned long ifclaimed;
	u32 secid;
};
int ksuspend_usb_init(void);

/* internal notify stuff */
extern void usb_notify_add_device(struct usb_device *udev);
extern void usb_notify_remove_device(struct usb_device *udev);
extern void usb_notify_add_bus(struct usb_bus *ubus);
extern void usb_notify_remove_bus(struct usb_bus *ubus);

/* Define slots for keys in KR_KEYSTORE: */
#define LKSN_NIWC LKSN_APP
#define LKSN_INTERFACES LKSN_NIWC+1
/* Beginning at LKSN_INTERFACES, each interface has a pair of slots: */
static inline unsigned long
forwarderSlot(struct usb_device * udev, uint8_t localIntfNum)
{
  return LKSN_INTERFACES + ((udev->devnum << 8) + localIntfNum) * 2;
}
static inline unsigned long
driverSlot(struct usb_device * udev, uint8_t localIntfNum)
{
  return forwarderSlot(udev, localIntfNum) + 1;
}

static inline unsigned long
urbToCap(struct urb * urb)
{
  /* Use the address of the urb to ensure the slot is unique. 
  Divide by sizeof(struct urb) to increase the density. */
  return (unsigned long)urb / sizeof(struct urb);
}

void usb_freeUrbWithCap(struct urb * urb);

extern struct list_head newInterfacesList;
extern struct mutex newInterfacesMutex;
extern bool waitingForNewInterfaces;
void newInterface(struct usb_interface * intf);
void makeInterfaceCap(unsigned long /* cap_t */ process,
  struct usb_interface * intf);
int usb_set_altSetting(struct usb_device *dev,
  struct usb_interface * iface, int alternate);
